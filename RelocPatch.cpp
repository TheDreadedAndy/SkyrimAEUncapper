/**
 * @file RelocPatch.cpp
 * @author Andrew Spaulding (Kasplat)
 * @author Kassent
 * @author Vadfromnu
 * @brief Finds code signatures within the game and applies patches to them.
 * @bug No known bugs.
 *
 * This file contains code signatures which are used by the plugin to find
 * the game code which must be patched. These signatures are parsed by the
 * ApplyGamePatches() function, which will find each location to be
 * patched and apply the patches.
 *
 * Note that many of these signatures and the associated assembly code were
 * found by the other authors which worked on this mod. I (Kasplat)
 * converted each one to an address-independence ID. Additionally, a new
 * signature for PlayerAVOGetCurrent() was found and DisplayTrueSkillLevel
 * was added.
 *
 * Note that we do not allow non-portable structures to leave this file. That
 * is to say, if a structure is not independent between the versions of AE,
 * then it must either be opaque or contained within this file. This means we
 * never return a pointer to the player class, as that structure is not
 * version independent. Additionally, note that we let the game handle these
 * structure differences whenever possible, which it isn't always.
 */

#include "RelocFn.h"
#include "RelocPatch.h"

#include <cstddef>
#include <cstdint>

#include "GameSettings.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"
#include "skse_version.h"
#include "addr_lib/versionlibdb.h"

#include "Hook_Skill.h"
#include "HookWrappers.h"
#include "SafeMemSet.h"
#include "Settings.h"

/// @brief Encodes the various types of hooks which can be injected.
struct HookType {
    enum t {
        None,
        Jump5,
        Jump6,
        DirectJump,
        Call5,
        Call6,
        DirectCall,
        Nop
    };

    /// @brief Gets the patch size of the given hook type.
    static size_t
    Size(
        t type
    ) {
        size_t ret = 0;

        switch (type) {
            case None:
            case Nop:
                ret = 0;
                break;
            case Jump5:
            case Call5:
            case DirectCall:
            case DirectJump:
                ret = 5;
                break;
            case Jump6:
            case Call6:
                ret = 6;
                break;
            default:
                HALT("Cannot get the size of an invalid hook type.");
        }

        return ret;
    }

    /// @brief Gets the allocation size of the hook in the branch trampoline.
    static size_t
    AllocSize(
        t type
    ) {
        size_t ret = 0;

        switch (type) {
            case None:
            case Nop:
            case DirectCall:
            case DirectJump:
                ret = 0;
                break;
            case Jump5:
            case Call5:
                ret = 14;
                break;
            case Jump6:
            case Call6:
                ret = 8;
                break;
            default:
                HALT("Cannot get the trampoline size of an invalid hook type");
        }

        return ret;
    }
};

/// @brief Describes a patch to be applied by a RelocPatch<T>.
struct CodeSignature {
    const char* name;
    HookType::t hook_type;
    uintptr_t hook;
    unsigned long long id;
    size_t patch_size;
    ptrdiff_t offset;
    uintptr_t *return_trampoline;
    bool (*enabled)(void);
    void **result;

    // Optional argument for finding new addresses.
#ifdef _DEBUG
    uintptr_t known_offset;
#endif

    /**
     * @brief Creates a new patch signature structure.
     * @param name The name of the patch signature.
     * @param enabled A function which will be called to check if the patch
     *                is enabled.
     * @param hook_type The non-none type of hook to be inserted.
     * @param hook The hook to install with this patch.
     * @param id The relocatable object/function id.
     * @param patch_size The size of the code to be overwritten.
     * @param return_trampoline The trampoline to be filled with the patch
     *                          return address.
     * @param offset The offset from the signature start to the hook address.
     * @param known_offset The known-correct offset into the binary for
     *                     this game version.
     */
    CodeSignature(
        const char* name,
        bool (*enabled)(void),
        HookType::t hook_type,
        uintptr_t hook,
        unsigned long long id,
        size_t patch_size,
        uintptr_t *return_trampoline = nullptr,
        ptrdiff_t offset = 0
#ifdef _DEBUG
        , uintptr_t known_offset = 0
#endif
    ) : name(name),
        enabled(enabled),
        hook_type(hook_type),
        hook(hook),
        id(id),
        patch_size(patch_size),
        offset(offset),
        return_trampoline(return_trampoline),
#ifdef _DEBUG
        known_offset(known_offset),
#endif
        result(nullptr)
    {}

    /**
     * @brief Creates a new code signature which links to a game object.
     * @param name The name of the code signature.
     * @param id The relocatable function id.
     * @param result The pointer to be filled with the found result.
     * @param known_offset The known-correct offset into the binary for
     *                     this game version.
     */
    CodeSignature(
        const char *name,
        unsigned long long id,
        void **result
#ifdef _DEBUG
        , uintptr_t known_offset = 0
#endif
    ) : name(name),
        enabled(nullptr),
        hook_type(HookType::None),
        hook(0),
        id(id),
        patch_size(0),
        offset(0),
        return_trampoline(nullptr),
#ifdef _DEBUG
        known_offset(known_offset),
#endif
        result(result)
    {}

    /**
     * @brief Checks if the patch has been disabled.
     */
    inline bool Disabled(void) const { return (enabled != nullptr) && !enabled(); }
};

/**
 * @brief Used by the OG game functions we replace to return to their
 *        unmodified implementations.
 *
 * Boing!
 */
///@{
extern "C" {
    uintptr_t ImprovePlayerSkillPoints_ReturnTrampoline;
    uintptr_t ModifyPerkPool_ReturnTrampoline;
    uintptr_t PlayerAVOGetCurrent_ReturnTrampoline;
    uintptr_t DisplayTrueSkillLevel_ReturnTrampoline;
    uintptr_t CheckConditionForLegendarySkill_ReturnTrampoline;
    uintptr_t HideLegendaryButton_ReturnTrampoline;
}
///@}

/**
 * @brief Holds references to the global game variables we need.
 */
///@{
static PlayerCharacter **playerObject;
static void **gameSettings;
///@}

/**
 * @brief Holds the function pointers which we use to call the game functions.
 */
///@{
static UInt16 (*GetLevel_Entry)(void*);
static Setting *(*GetGameSetting_Entry)(void*, const char*);
static float (*PlayerAVOGetBase_Entry)(void*, ActorAttribute::t);
static float (*PlayerAVOGetCurrent_Entry)(void*, ActorAttribute::t);
static void (*PlayerAVOModBase_Entry)(void*, ActorAttribute::t, float);
static void (*PlayerAVOModCurrent_Entry)(void*, UInt32, ActorAttribute::t, float);
///@}

/**
 * @brief The signature used to find the player object.
 */
static const CodeSignature kThePlayer_ObjectSig(
    /* name */   "g_thePlayer",
    /* id */     403521,
    /* result */ reinterpret_cast<void**>(&playerObject)
);

/**
 * @brief The signature used to find the game settings object.
 */
static const CodeSignature kGameSettingCollection_ObjectSig(
    /* name */   "g_gameSettingCollection",
    /* id */     400782,
    /* result */ reinterpret_cast<void**>(&gameSettings)
);

/**
 * @brief The signature used to find the games "GetLevel" function.
 */
static const CodeSignature kGetLevel_FunctionSig(
    /* name */   "GetLevel",
    /* id */     37334,
    /* result */ reinterpret_cast<void**>(&GetLevel_Entry)
);

/**
 * @brief Gets a game setting in the settings collection.
 */
static const CodeSignature kGetGameSetting_FunctionSig(
    /* name */   "GetGameSetting",
    /* id */     22788,
    /* result */ reinterpret_cast<void**>(&GetGameSetting_Entry)
);

/**
 * @brief The code signature used to find the games PlayerAVOGetBase() fn.
 */
static const CodeSignature kPlayerAVOGetBase_FunctionSig(
    /* name */   "PlayerAVOGetBase",
    /* id */     38464,
    /* result */ reinterpret_cast<void**>(&PlayerAVOGetBase_Entry)
);

/**
 * @brief The code signature used to find the games PlayerAVOGetCurrent() fn.
 */
static const CodeSignature kPlayerAVOGetCurrent_FunctionSig(
    /* name */   "PlayerAVOGetCurrent",
    /* id */     38462,
    /* result */ reinterpret_cast<void**>(&PlayerAVOGetCurrent_Entry)
);

/**
 * @brief Mods the base value of an attribute of the player.
 */
static const CodeSignature kPlayerAVOModBase_FunctionSig(
    /* name */   "PlayerAVOModBase",
    /* id */     38466,
    /* result */ reinterpret_cast<void**>(&PlayerAVOModBase_Entry)
);

/**
 * @brief Mods the current value of an attribute of the player.
 */
static const CodeSignature kPlayerAVOModCurrent_FunctionSig(
    /* name */   "PlayerAVOModCurrent",
    /* id */     38467,
    /* result */ reinterpret_cast<void**>(&PlayerAVOModCurrent_Entry)
);

/**
 * @brief The signature and offset used to redirect to the code which alters
 *        the real skill cap.
 *
 * The offset into this signature overwrites a movess instruction and instead
 * redirects to our handler. Note that the last four bytes of this instruction
 * must be overwritten with 0x90 (NOP), at the request of the author of the
 * eXPerience mod (17751). This is handled by the RelocPatch interface.
 *
 * This signature hooks into the following function:
 * ...
 * 48 8b 01        MOV        RAX,qword ptr [param_1]
 * ff 50 18        CALL       qword ptr [RAX + 0x18]
 * 44 0f 28 c0     MOVAPS     XMM8,XMM0
 * ### kHook_SkillCapPatch_Ent ###
 * f3 44 0f        MOVSS      XMM10,dword ptr [DAT_14161af50] = 42C80000h 100.0
 * 10 15
 * ### kHook_SkillCapPatch_Ret ###
 * c1 c2 f0 00
 * 41 0f 2f c2     COMISS     XMM0,XMM10
 * 0f 83 d8        JNC        LAB_14070ef71
 * 02 00 00
 * ...
 *
 * Note that the code being patched expects the current skill level in XMM0 and
 * the maximum skill level in XMM10.
 */
static const CodeSignature kSkillCapPatch_PatchSig(
    /* name */       "SkillCapPatch",
    /* enabled */    []() { return settings.IsSkillCapEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(SkillCapPatch_Wrapper),
    /* id */         41561,
    /* patch_size */ 9,
    /* trampoline */ nullptr,
    /* offset */     0x76
);

/**
 * @brief Replaces the original charge point calculation function call with a
 *        call to our modified function which caps the enchant level at 199.
 *
 * This is a bug fix for the original equation, which gives invalid results for
 * enchantment levels at 200 or above.
 * 
 * Note that we also replace the PlayerAVOGetCurrent() call, so that we can
 * enforce a different formula cap for enchanting charge and magnitude.
 */
static const CodeSignature kCalculateChargePointsPerUse_PatchSig(
    /* name */       "CalculateChargePointsPerUse",
    /* enabled */    []() { return settings.IsEnchantPatchEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(CalculateChargePointsPerUse_Wrapper),
    /* id */         51449,
    /* patch_size */ 14,
    /* trampoline */ nullptr,
    /* offset */     0x32a
);

/**
 * @brief Caps the effective skill level in calculations by always returning
 *        a damaged result.
 *
 * This patch redirects to our hook, with an assembly wrapper allowing the
 * hook to call the original implementation. The assembly wrapper reimplements
 * the first 6 bytes, then jumps to the instruction after the hook.
 */
static const CodeSignature kPlayerAVOGetCurrent_PatchSig(
    /* name */       "PlayerAVOGetCurrent (Patch)",
    /* enabled */    []() { return settings.IsSkillFormulaCapEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(PlayerAVOGetCurrent_Hook),
    /* id */         38462,
    /* patch_size */ 6,
    /* trampoline */ &PlayerAVOGetCurrent_ReturnTrampoline
);

/**
 * @brief Overwrites the skill display PlayerAVOGetCurrent() call to display
 *        the actual, non-damaged, skill level.
 *
 * The function that is overwritten by our PlayerAVOGetCurrent() hook is also
 * used to display the skill level in the skills menu.
 *
 * So as to not confuse players, this hook is used to force the skills menu to
 * show the actual skill level, not the damaged value.
 *
 * This hook replaces the call instruction which would call
 * PlayerAVOGetCurrent() with a call to our reimplemented
 * PlayerAVOGetCurrent_Original().
 */
static const CodeSignature kDisplayTrueSkillLevel_PatchSig(
    /* name */       "DisplayTrueSkillLevel",
    /* enabled */    []() { return settings.IsSkillFormulaCapEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(DisplayTrueSkillLevel_Hook),
    /* id */         52525,
    /* patch_size */ 7,
    /* trampoline */ &DisplayTrueSkillLevel_ReturnTrampoline,
    /* offset */     0x120
);

/**
 * @brief Overwrites the skill color displays call to PlayerAVOGetCurrent to
 *        call the original function.
 * 
 * This patch exists for the same reason as the above patch, except its concern
 * is in getting the skill number to display as the correct color, not number.
 */
static const CodeSignature kDisplayTrueSkillColor_PatchSig(
    /* name */       "DisplayTrueSkillColor",
    /* enabled */    []() { return settings.IsSkillFormulaCapEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(DisplayTrueSkillColor_Hook),
    /* id */         52945,
    /* patch_size */ 10,
    /* trampoline */ nullptr,
    /* offset */     0x32
);

/**
 * @brief Prevents the skill training function from applying our multipliers.
 */
static const CodeSignature kImproveSkillByTraining_PatchSig(
    /* name */       "ImproveSkillByTraining",
    /* enabled */    []() { return settings.IsSkillExpEnabled(); },
    /* hook_type */  HookType::Call5,
    /* hook */       reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Original),
    /* id */         41562,
    /* patch_size */ 5,
    /* trampoline */ nullptr,
    /* offset */     0x98
);

/**
 * @brief Applies the multipliers from the INI file to skill experience.
 */
static const CodeSignature kImprovePlayerSkillPoints_PatchSig(
    /* name */       "ImprovePlayerSkillPoints",
    /* enabled */    []() { return settings.IsSkillExpEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Hook),
    /* id */         41561,
    /* patch_size */ 6,
    /* trampoline */ &ImprovePlayerSkillPoints_ReturnTrampoline
);

/**
 * @brief The signature and offset used to hook into the perk pool modification
 *        routine.
 *
 * Upon entry into our hook, we run our function. We then reimplement the final
 * few instructions in the return path of the function we hooked into. This way,
 * we need only modify one instruction and can still use the RelocPatch interface.
 */
static const CodeSignature kModifyPerkPool_PatchSig(
    /* name */       "ModifyPerkPool",
    /* enabled */    []() { return settings.IsPerkPointsEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ModifyPerkPool_Wrapper),
    /* id */         52538,
    /* patch_size */ 9,
    /* trampoline */ &ModifyPerkPool_ReturnTrampoline,
    /* offset */     0x70
);

/**
 * @brief Passes the EXP gain originally calculated by the game to our hook for
 *        further modification.
 */
static const CodeSignature kImproveLevelExpBySkillLevel_PatchSig(
    /* name */       "ImproveLevelExpBySkillLevel",
    /* enabled */    []() { return settings.IsLevelExpEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveLevelExpBySkillLevel_Wrapper),
    /* id */         41561,
    /* patch_size */ 8,
    /* trampoline */ nullptr,
    /* offset */     0x2D7
);

/**
 * @brief Overwrites the attribute level-up function to adjust the gains based
 *        on the players attribute selection.
 * 
 * We inject this patch just after the player has made their attribute selection,
 * and replace what would have been a call to player_avo->ModBase(...). Then,
 * we manually invoke ModBase and ModCurrent for the attributes and carry weight
 * as specified in the INI file.
 * 
 * Note that this patch overwrites the carry weight change done in the games code
 * as well. It also means the game settings which would usually control these
 * attributes are ignored.
 */
static const CodeSignature kImproveAttributeWhenLevelUp_PatchSig(
    /* name */       "ImproveAttributeWhenLevelUp",
    /* enabled */    []() { return settings.IsAttributePointsEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveAttributeWhenLevelUp_Hook),
    /* id */         51917,
    /* patch_size */ 0x2b,
    /* trampoline */ nullptr,
    /* offset */     0x93
);

/**
 * @brief Alters the reset level of legendarying a skill.
 *
 * Unfortunately, Kasplat has no idea why altering this particular jump makes
 * the change we want.
 */
static const CodeSignature kLegendaryResetSkillLevel_PatchSig(
    /* name */       "LegendaryResetSkillLevel",
    /* enabled */    []() { return settings.IsLegendaryEnabled(); },
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(LegendaryResetSkillLevel_Wrapper),
    /* id */         52591,
    /* patch_size */ 6,
    /* trampoline */ nullptr,
    /* offset */     0x1d7
);

/**
 * @brief Replaces the call to the legendary condition function with our own.
 *
 * This patch simply overwrites the call to the original legendary condition
 * check function with a call to our own reimplemented condition check function.
 */
static const CodeSignature kCheckConditionForLegendarySkill_PatchSig(
    /* name */       "CheckConditionForLegendarySkill",
    /* enabled */    []() { return settings.IsLegendaryEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(CheckConditionForLegendarySkill_Wrapper),
    /* id */         52520,
    /* patch_size */ 10,
    /* trampoline */ &CheckConditionForLegendarySkill_ReturnTrampoline,
    /* offset */     0x157
);

/**
 * @brief Hooks into the legendary button display code to allow it to be hidden.
 *
 * The assembly for this hook is as follows:
 * 48 8b 0d 2e d4 6b 02 	mov    0x26bd42e(%rip),%rcx        # 0x142fc1b78 (player)
 * 48 81 c1 b8 00 00 00 	add    $0xb8,%rcx <- Player actor state
 * 48 8b 01             	mov    (%rcx),%rax
 * 41 8b d7             	mov    %r15d,%edx <- Skill ID.
 * ff 50 18             	callq  *0x18(%rax)
 * 0f 2f 05 bf 57 d1 00 	comiss 0xd157bf(%rip),%xmm0        # 0x141619f20
 * 72 6b                	jb     0x1409047ce
 * 48 8d 05 d6 b9 e9 00 	lea    0xe9b9d6(%rip),%rax        # 0x1417a0140
 * 48 89 85 c0 00 00 00 	mov    %rax,0xc0(%rbp)
 * 48 8d 3d 58 99 c2 ff 	lea    -0x3d66a8(%rip),%rdi
*/
static const CodeSignature kHideLegendaryButton_PatchSig(
    /* name */       "HideLegendaryButton",
    /* enabled */    []() { return settings.IsLegendaryEnabled(); },
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(HideLegendaryButton_Wrapper),
    /* id */         52527,
    /* patch_size */ 10,
    /* trampoline */ &HideLegendaryButton_ReturnTrampoline,
    /* offset */     0x167
);

/**
 * @brief Lists all the code signatures to be resolved/applied
 *        by ApplyGamePatches().
 */
///@{
static const CodeSignature *const kGameSignatures[] = {
    &kThePlayer_ObjectSig,
    &kGameSettingCollection_ObjectSig,
    &kGetLevel_FunctionSig,
    &kGetGameSetting_FunctionSig,
    &kPlayerAVOGetBase_FunctionSig,
    &kPlayerAVOGetCurrent_FunctionSig,
    &kPlayerAVOModBase_FunctionSig,
    &kPlayerAVOModCurrent_FunctionSig,

    &kSkillCapPatch_PatchSig,
    &kCalculateChargePointsPerUse_PatchSig,
    &kPlayerAVOGetCurrent_PatchSig,
    &kDisplayTrueSkillLevel_PatchSig,
    &kDisplayTrueSkillColor_PatchSig,

    &kImproveSkillByTraining_PatchSig,
    &kImprovePlayerSkillPoints_PatchSig,
    &kModifyPerkPool_PatchSig,
    &kImproveLevelExpBySkillLevel_PatchSig,
    &kImproveAttributeWhenLevelUp_PatchSig,

    &kLegendaryResetSkillLevel_PatchSig,
    &kCheckConditionForLegendarySkill_PatchSig,
    &kHideLegendaryButton_PatchSig,
};
static const size_t kNumSigs = sizeof(kGameSignatures) / sizeof(void*);
///@}

/// @brief The opcode for an x86 NOP.
static const uint8_t kNop = 0x90;

/// @brief The running version of skyrim.
static unsigned int runningSkyrimVersion;

/**
* @brief Gets the actor value owner field of the player.
*
* The location of this field is dependent on the running version of the game.
* As such, we must case on that.
*
* Versions before 1.6.629 store it at offset 0xB0. From that version on, it
* is at offset 0xB8.
*
* @return The actor value owner of the player.
*/
static void *
GetPlayerActorValueOwner() {
    ASSERT(playerObject);
    ASSERT(*playerObject);

    size_t avo_offset = (runningSkyrimVersion >= RUNTIME_VERSION_1_6_629)
        ? 0xB8 : 0xB0;
    return reinterpret_cast<char*>(*playerObject) + avo_offset;
}

/**
 * @brief Locates all the signatures necessary for this plugin, and calculates
 *        the needed size of the branch trampoline buffer.
 * @param real_addrs A list of found address signatures, in the same order as
 *                   kGameSignatures.
 * @return The size of the buffer on success, or a negative integer on failure.
 */
static ptrdiff_t
LocateSignatures(
    uintptr_t real_addrs[kNumSigs]
) {
    _MESSAGE("Attempting to locate signatures.");

    auto db = VersionDb();
    db.Load();

    // Attempt to find all the requested signatures.
    int success = 0;
    size_t branch_alloc_size = 0;
    for (size_t i = 0; i < kNumSigs; i++) {
        auto sig = kGameSignatures[i];
        unsigned long long id = sig->id;

        // If the patch is disabled, ignore it.
        if (sig->Disabled()) {
            _MESSAGE("Signature %s is disabled.", sig->name);
            real_addrs[i] = 0;
            continue;
        }

        // Update the branch allocation size.
        branch_alloc_size += HookType::AllocSize(sig->hook_type);

#ifdef _DEBUG
        if (sig->known_offset) {
            ASSERT(db.FindIdByOffset(sig->known_offset, id));
        }
#endif

        // Find the offset.
        void *addr = db.FindAddressById(id);
        if (addr) {
            real_addrs[i] = reinterpret_cast<uintptr_t>(addr) + sig->offset;

            _MESSAGE(
                "Signature %s ([ID: %zu] + 0x%zx) is at offset 0x%zx.",
                sig->name,
                id,
                sig->offset,
                real_addrs[i] - RelocationManager::s_baseAddr
            );
        } else {
            success = -1;
            _MESSAGE(
                "Failed to find signature %s ([ID: %zu] + 0x%zu).",
                sig->name,
                id,
                sig->offset
            );
        }
    }

    if (success < 0) {
        _MESSAGE("Could not locate every signature.");
        return -1;
    }

    _MESSAGE("Successfully located all signatures.");

    return branch_alloc_size;
}

/**
* @brief Applies the necessary game patches to the found real addresses.
* @param The real addresses of the patches.
*/
static void
PatchGameCode(
    uintptr_t real_addrs[kNumSigs]
) {
    _MESSAGE("Applying game patches...");

    for (size_t i = 0; i < kNumSigs; i++) {
        uintptr_t real_address = real_addrs[i];
        auto sig = kGameSignatures[i];

        // Skip disabled patches.
        if (sig->Disabled()) {
            continue;
        }

        size_t hook_size = HookType::Size(sig->hook_type);
        size_t return_address = real_address + hook_size;

        ASSERT(hook_size <= sig->patch_size);
        ASSERT(!(sig->hook) == ((sig->hook_type == HookType::None)
            || (sig->hook_type == HookType::Nop)));

        // Install the trampoline, if necessary.
        if (sig->return_trampoline) {
            ASSERT(sig->hook_type != HookType::None);
            ASSERT(sig->hook_type != HookType::Nop);
            *(sig->return_trampoline) = return_address;
        }

        // Install the hook/result.
        switch (sig->hook_type) {
            case HookType::None:
                ASSERT(sig->result);
                *(sig->result) = reinterpret_cast<void*>(real_address);
                break;
            case HookType::Jump5:
                ASSERT(g_branchTrampoline.Write5Branch(real_address, sig->hook));
                break;
            case HookType::Jump6:
                ASSERT(g_branchTrampoline.Write6Branch(real_address, sig->hook));
                break;
            case HookType::Call5:
                ASSERT(g_branchTrampoline.Write5Call(real_address, sig->hook));
                break;
            case HookType::Call6:
                ASSERT(g_branchTrampoline.Write6Call(real_address, sig->hook));
                break;
            case HookType::DirectCall:
                ASSERT(SafeWriteCall(real_address, sig->hook));
                break;
            case HookType::DirectJump:
                ASSERT(SafeWriteJump(real_address, sig->hook));
                break;
            case HookType::Nop:
                break;
            default:
                HALT("Cannot install a hook with an invalid type");
        }

        // Overwrite the rest of the instructions with NOPs. We do this with
        // every hook to ensure the best compatibility with other SKSE
        // plugins.
        SafeMemSet(return_address, kNop, sig->patch_size - hook_size);
    }

    _MESSAGE("Finished applying game patches!");
}

/**
 * @brief Gets a pointer to a float game setting.
 *
 * Always returns a valid pointer.
 */
float *
GetFloatGameSetting(
    const char *var
) {
    ASSERT(gameSettings);
    ASSERT(*gameSettings);
    ASSERT(GetGameSetting_Entry);
    Setting *setting = GetGameSetting_Entry(*gameSettings, var);
    ASSERT(setting);
    ASSERT(setting->name[0] == 'f');
    return &setting->data.f32;
}

/**
* @brief Gets the level of the player.
*/
UInt16
GetPlayerLevel() {
    ASSERT(playerObject);
    ASSERT(*playerObject);
    ASSERT(GetLevel_Entry);
    return GetLevel_Entry(*playerObject);
}

/**
 * @brief Gets the base value of an attribute of the player.
 * @param attr The attribute to get the base value of.
 */
float
PlayerAVOGetBase(
    ActorAttribute::t attr
) {
    ASSERT(PlayerAVOGetBase_Entry);
    return PlayerAVOGetBase_Entry(GetPlayerActorValueOwner(), attr);
}

/**
 * @brief Gets the (uncapped) current value of an attribute of the player.
 * 
 * This function takes a special path, as the charge formula needs to use it
 * even if skill formula caps are disabled.
 *
 * @param av The player actor value owner.
 * @param attr The attribute to get the value of.
 */
float
PlayerAVOGetCurrent_Original(
    void* av,
    ActorAttribute::t attr
) {
    ASSERT(PlayerAVOGetCurrent_Entry);
    if (settings.IsSkillFormulaCapEnabled()) {
        // Patch installed, so we need to use the wrapper.
        ASSERT(PlayerAVOGetCurrent_ReturnTrampoline);
        return PlayerAVOGetCurrent_OriginalWrapper(av, attr);
    } else {
        // No patch installed, so we can just call the original function
        // (and must, since we don't have a trampoline).
        ASSERT(PlayerAVOGetCurrent_ReturnTrampoline == 0);
        return PlayerAVOGetCurrent_Entry(av, attr);
    }
}

/**
 * @brief Modifies the base value of a player attribute.
 * @param attr The attribute to modify.
 * @param val The value to add to it.
 */
void
PlayerAVOModBase(
    ActorAttribute::t attr,
    float val
) {
    ASSERT(PlayerAVOModBase_Entry);
    PlayerAVOModBase_Entry(GetPlayerActorValueOwner(), attr, val);
}

/**
 * @brief Modifies the current value of a player attribute.
 * @param unk1 unknown.
 * @param attr The attribute to modify.
 * @param val The value to add to the attribute.
 */
void
PlayerAVOModCurrent(
    UInt32 unk1,
    ActorAttribute::t attr,
    float val
) {
    ASSERT(PlayerAVOModCurrent_Entry);
    PlayerAVOModCurrent_Entry(GetPlayerActorValueOwner(), unk1, attr, val);
}

/**
 * @brief Applies all of this plugins patches to the skyrim AE binary.
 * @param img_base The base of the skyrim module.
 * @param runtime_version The running version of skyrim.
 * @return 0 if the patches could be applied, a negative integer otherwise.
 */
int
ApplyGamePatches(
    void *img_base,
    unsigned int runtime_version
) {
    ASSERT(runtime_version >= RUNTIME_VERSION_1_6_317); // AE.
    runningSkyrimVersion = runtime_version;

    uintptr_t real_addrs[kNumSigs];

    size_t alloc_size = LocateSignatures(real_addrs);
    if (alloc_size < 0) {
        return -1;
    }

    if (alloc_size > 0) {
        _MESSAGE(
            "Creating a branch trampoline buffer with %zu bytes of space...",
            alloc_size
        );
        if (!g_branchTrampoline.Create(alloc_size, img_base)) {
            _MESSAGE("Failed to allocate branch trampoline.");
            return -1;
        }
        _MESSAGE("Done!");
    } else {
        _MESSAGE("Everything is disabled...");
    }

    PatchGameCode(real_addrs);

    return 0;
}
