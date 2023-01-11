/**
 * @file RelocPatch.cpp
 * @author Andrew Spaulding (Kasplat)
 * @author Kassant
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
 * signature for GetEffectiveSkillLevel() was found and DisplayTrueSkillLevel
 * was added.
 */

#include "RelocFn.h"
#include "RelocPatch.h"

#include <cstddef>
#include <cstdint>

#include "GameSettings.h"
#include "BranchTrampoline.h"
#include "SafeWrite.h"
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
};

/// @brief Describes a patch to be applied by a RelocPatch<T>.
// FIXME: Bad constructor comments.
struct CodeSignature {
    const char* name;
    HookType::t hook_type;
    uintptr_t hook;
    unsigned long long id;
    size_t patch_size;
    ptrdiff_t offset;
    uintptr_t *return_trampoline;
    void **result;

    // Optional argument for finding new addresses.
#ifdef _DEBUG
    uintptr_t known_offset;
#endif

    /**
     * @brief Creates a new patch signature structure.
     * @param name The name of the patch signature.
     * @param hook_type The non-none type of hook to be inserted.
     * @param hook The hook to install with this patch.
     * @param sig The hex signature to search for.
     * @param patch_size The size of the code to be overwritten.
     * @param return_trampoline The trampoline to be filled with the patch
     *                          return address.
     * @param offset The offset from the signature to the hook address.
     * @param indirect_offset An indirection offset to use to dereference the
     *                        first hook address to get a second, or 0.
     * @param instr_size The size of the instruction being indirected through,
     *                   or 0.
     */
    CodeSignature(
        const char* name,
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
     * @param sig The signature to search for.
     * @param result The pointer to be filled with the found result.
     * @param offset The offset into the signature to inspect.
     * @param indirect_offset An indirection offset to use to dereference the
     *                        the first generated address.
     * @param instr_size The size of the instruction being indirected through,
     *                   or 0.
     */
    CodeSignature(
        const char *name,
        unsigned long long id,
        void **result
#ifdef _DEBUG
        , uintptr_t known_offset = 0
#endif
    ) : name(name),
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
    uintptr_t ImproveAttributeWhenLevelUp_ReturnTrampoline;
    uintptr_t GetEffectiveSkillLevel_ReturnTrampoline;
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
static SettingCollectionMap **gameSettings;
///@}

/**
 * @brief Holds the function pointers which we use to call the game functions.
 */
///@{
static float (*GetBaseActorValue_Entry)(void *, UInt32);
static UInt16 (*GetLevel_Entry)(void*);
static bool (*GetSkillCoefficients_Entry)(UInt32, float*, float*, float*, float*);
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
 * @brief The code signature used to find the games GetBaseActorValue() fn.
 */
static const CodeSignature kGetBaseActorValue_FunctionSig(
    /* name */   "GetBaseActorValue",
    /* id */     38464,
    /* result */ reinterpret_cast<void**>(&GetBaseActorValue_Entry)
);

/**
 * @brief The code signature used to find the games GetSkillCoefficients() fn.
 */
static const CodeSignature kGetSkillCoefficients_FunctionSig(
    /* name */   "GetSkillCoefficients",
    /* id */     27244,
    /* result */ reinterpret_cast<void**>(&GetSkillCoefficients_Entry)
);

/**
 * @brief The signature and offset used to hook into the perk pool modification
 *        routine.
 *
 * Upon entry into our hook, we run our function. We then reimplement the final
 * few instructions in the return path of the function we hooked into. This way,
 * we need only modify one instruction and can still use the RelocPatch interface.
 *
 * The assembly for this signature is as follows:
 * 48 85 c0        TEST       RAX,RAX
 * 74 34           JZ         LAB_1408f678f
 * LAB_1408f675b   XREF[2]:     1435b64c0(*), 1435b64c8(*)
 * 66 0f 6e c7     MOVD       XMM0,EDI
 * 0f 5b c0        CVTDQ2PS   XMM0,XMM0
 * f3 0f 58        ADDSS      XMM0,dword ptr [RAX + 0x34]
 * 40 34
 * f3 0f 11        MOVSS      dword ptr [RAX + 0x34],XMM0
 * 40 34
 * 48 83 c4 20     ADD        RSP,0x20
 * 5f              POP        RDI
 * c3              RET
 * LAB_1408f6772   XREF[1]:     1408f671f(j)
 * ### kHook_ModifyPerkPool (redirect; does not return here) ###
 * 48 8b 15        MOV        RDX,qword ptr [DAT_142fc19c8]
 * 4f b2 6c 02
 * 0f b6 8a        MOVZX      ECX,byte ptr [RDX + 0xb01]
 * 01 0b 00 00
 * 8b c1           MOV        EAX,ECX
 * 03 c7           ADD        EAX,EDI
 * 78 09           JS         LAB_1408f678f
 * 40 02 cf        ADD        CL,DIL
 * 88 8a 01        MOV        byte ptr [RDX + 0xb01],CL
 * 0b 00 00
 * LAB_1408f678f   XREF[2]:     1408f6759(j), 1408f6784(j)
 * 48 83 c4 20     ADD        RSP,0x20
 * 5f              POP        RDI
 * c3              RET
 */
static const CodeSignature kModifyPerkPool_PatchSig(
    /* name */       "ModifyPerkPool",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ModifyPerkPool_Wrapper),
    /* id */         52538,
    /* patch_size */ 7,
    /* trampoline */ nullptr,
    /* offset */     0x62
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
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(SkillCapPatch_Wrapper),
    /* id */         41561,
    /* patch_size */ 9,
    /* trampoline */ nullptr,
    /* offset */     0x76
);

/**
 * @brief Alters the reset level of legendarying a skill.
 *
 * Unfortunately, Kasplat has no idea why altering this particular jump makes
 * the change we want.
 */
static const CodeSignature kLegendaryResetSkillLevel_PatchSig(
    /* name */       "LegendaryResetSkillLevel",
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
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(CheckConditionForLegendarySkill_Wrapper),
    /* id */         52520,
    /* patch_size */ 0x13,
    /* trampoline */ &CheckConditionForLegendarySkill_ReturnTrampoline,
    /* offset */     0x14e
);

/**
 * @brief Hooks into the legendary button display code to allow it to be hidden.
 *
 * The assembly for this hook is as follows:
 * 48 8b 0d 2e d4 6b 02 	mov    0x26bd42e(%rip),%rcx        # 0x142fc1b78
 * 48 81 c1 b8 00 00 00 	add    $0xb8,%rcx
 * 48 8b 01             	mov    (%rcx),%rax
 * 41 8b d7             	mov    %r15d,%edx
 * ff 50 18             	callq  *0x18(%rax)
 * 0f 2f 05 bf 57 d1 00 	comiss 0xd157bf(%rip),%xmm0        # 0x141619f20
 * 72 6b                	jb     0x1409047ce
 * 48 8d 05 d6 b9 e9 00 	lea    0xe9b9d6(%rip),%rax        # 0x1417a0140
 * 48 89 85 c0 00 00 00 	mov    %rax,0xc0(%rbp)
 * 48 8d 3d 58 99 c2 ff 	lea    -0x3d66a8(%rip),%rdi
*/
static const CodeSignature kHideLegendaryButton_PatchSig(
    /* name */       "HideLegendaryButton",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(HideLegendaryButton_Wrapper),
    /* id */         52527,
    /* patch_size */ 0x1e,
    /* trampoline */ &HideLegendaryButton_ReturnTrampoline,
    /* offset */     0x153
);

/**
 * @brief Overwrites the original ImproveSkillByTraining() function to uncap it
 *        and ensure it improves skills without multipliers.
 */
static const CodeSignature kImproveSkillByTraining_PatchSig(
    /* name */       "ImproveSkillByTraining",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveSkillByTraining_Hook),
    /* id */         41562,
    /* patch_size */ 6
);

/**
 * @brief TODO
 *
 * FIXME
 * kHook_SkillCapPatch_Ent is inside this function
                             FUN_14070ec10                                   XREF[3]:     FUN_1406cac00:1406cac25(c),
                                                                                          FUN_1406cac40:14070f098(c),
                     14359bfd8(*)
       14070ec10 48 8b c4        MOV        RAX,RSP
       14070ec13 57              PUSH       RDI
       14070ec14 41 54           PUSH       R12
       14070ec16 41 55           PUSH       R13
       14070ec18 41 56           PUSH       R14
       14070ec1a 41 57           PUSH       R15
       14070ec1c 48 81 ec        SUB        RSP,0x180
                 80 01 00 00
       14070ec23 48 c7 44        MOV        qword ptr [RSP + local_160],-0x2
                 24 48 fe
                 ff ff ff
*/
static const CodeSignature kImprovePlayerSkillPoints_PatchSig(
    /* name */       "ImprovePlayerSkillPoints",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImprovePlayerSkillPoints_Hook),
    /* id */         41561,
    /* patch_size */ 6,
    /* trampoline */ &ImprovePlayerSkillPoints_ReturnTrampoline
);

/**
 * @brief Passes the EXP gain originally calculated by the game to our hook for
 *        further modification.
 */
static const CodeSignature kImproveLevelExpBySkillLevel_PatchSig(
    /* name */       "ImproveLevelExpBySkillLevel",
    /* hook_type */  HookType::Call6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveLevelExpBySkillLevel_Wrapper),
    /* id */         41561,
    /* patch_size */ 8,
    /* trampoline */ nullptr,
    /* offset */     0x2D7
);

/**
 * @brief TODO
 *
 * FIXME
 *
                             undefined ImproveAttributeWhenLevelUp()
             undefined         AL:1           <RETURN>
             undefined8        Stack[0x20]:8  local_res20                             XREF[2]:     1408c4719(W),
                                                                                                   1408c480a(R)
             undefined4        Stack[0x18]:4  local_res18                             XREF[1]:     1408c4769(W)
             undefined8        Stack[0x10]:8  local_res10                             XREF[2]:     1408c4714(W),
                                                                                                   1408c4805(R)
             undefined8        Stack[0x8]:8   local_res8                              XREF[2]:     1408c470f(W),
                                                                                                   1408c4800(R)
             undefined8        Stack[-0x18]:8 local_18                                XREF[1]:     1408c4706(W)
                             ImproveAttributeWhenLevelUp                     XREF[2]:     1417a7938(*), 1435b4908(*)
       1408c4700 40 57           PUSH       RDI
       1408c4702 48 83 ec 30     SUB        RSP,0x30
       1408c4706 48 c7 44        MOV        qword ptr [RSP + local_18],-0x2
                 24 20 fe
                 ff ff ff
       1408c470f 48 89 5c        MOV        qword ptr [RSP + local_res8],RBX
                 24 40
       1408c4714 48 89 6c        MOV        qword ptr [RSP + local_res10],RBP
                 24 48
       1408c4719 48 89 74        MOV        qword ptr [RSP + local_res20],RSI
                 24 58
       1408c471e 0f b6 da        MOVZX      EBX,DL
       1408c4721 48 8b f9        MOV        RDI,RCX
       1408c4724 48 8b 15        MOV        RDX,qword ptr [DAT_141f5b278]                    = ??
                 4d 6b 69 01
       1408c472b 48 81 c2        ADD        RDX,0x128
                 28 01 00 00
       1408c4732 48 8b 0d        MOV        RCX,qword ptr [DAT_141f59320]                    = ??
                 e7 4b 69 01
       1408c4739 e8 62 fd        CALL       FUN_140f044a0                                    undefined FUN_140f044a0()
                 63 00
       1408c473e 84 c0           TEST       AL,AL
       1408c4740 0f 84 ba        JZ         LAB_1408c4800
                 00 00 00
       1408c4746 84 db           TEST       BL,BL
       1408c4748 0f 85 aa        JNZ        LAB_1408c47f8
                 00 00 00
       1408c474e 8b 15 44        MOV        EDX,dword ptr [DAT_143531398]                    = ??
                 cc c6 02
       1408c4754 65 48 8b        MOV        RAX,qword ptr GS:[0x58]
                 04 25 58
                 00 00 00
       1408c475d bd 68 07        MOV        EBP,0x768
                 00 00
       1408c4762 48 8b 34 d0     MOV        RSI,qword ptr [RAX + RDX*0x8]
       1408c4766 8b 1c 2e        MOV        EBX,dword ptr [RSI + RBP*0x1]
       1408c4769 89 5c 24 50     MOV        dword ptr [RSP + local_res18],EBX
       1408c476d c7 04 2e        MOV        dword ptr [RSI + RBP*0x1],0x46
                 46 00 00 00
       1408c4774 48 8b 0d        MOV        RCX,qword ptr [DAT_142fc19c8]                    = ??
                 4d d2 6f 02
       1408c477b 48 81 c1        ADD        RCX,0xb0
                 b0 00 00 00
       1408c4782 48 8b 01        MOV        RAX,qword ptr [RCX]
       1408c4785 66 0f 6e        MOVD       XMM2,dword ptr [DAT_141e6a540]                   = 0000000Ah
                 15 b3 5d
                 5a 01
*/
static const CodeSignature kImproveAttributeWhenLevelUp_PatchSig(
    /* name */       "ImproveAttributeWhenLevelUp",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(ImproveAttributeWhenLevelUp_Hook),
    /* id */         51917,
    /* patch_size */ 6,
    /* trampoline */ &ImproveAttributeWhenLevelUp_ReturnTrampoline
);

/**
 * @brief Allows health and magicka level ups to improve carry weight.
 * 
 * This patch simply overwrites the branch instruction which would skip carry
 * weight improvement for health/magicka with NOPs.
 */
static const CodeSignature kAllowAllAttrImproveCarryWeight_PatchSig(
    /* name */       "AllowAllAttrImproveCarryWeight",
    /* hook_type */  HookType::Nop,
    /* hook */       0,
    /* id */         51917,
    /* patch_size */ 2,
    /* trampoline */ nullptr,
    /* offset */     0x9a
);

/**
 * @brief Replaces the original charge point calculation function call with a
 *        call to our modified function which caps the enchant level at 199.
 *
 * This is a bug fix for the original equation, which gives invalid results for
 * enchantment levels at 200 or above.
 */
static const CodeSignature kCalculateChargePointsPerUse_PatchSig(
    /* name */       "CalculateChargePointsPerUse",
    /* hook_type */  HookType::Call5,
    /* hook */       reinterpret_cast<uintptr_t>(CalculateChargePointsPerUse_Hook),
    /* id */         51449,
    /* patch_size */ 5,
    /* trampoline */ nullptr,
    /* offset */     0x333
);

/**
 * @brief Caps the effective skill level in calculations by always returning
 *        a damaged result.
 *
 * This patch redirects to our hook, with an assembly wrapper allowing the
 * hook to call the original implementation. The assembly wrapper reimplements
 * the first 6 bytes, then jumps to the instruction after the hook.
 */
static const CodeSignature kGetEffectiveSkillLevel_PatchSig(
    /* name */       "GetEffectiveSkillLevel",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(GetEffectiveSkillLevel_Hook),
    /* id */         38462,
    /* patch_size */ 6,
    /* trampoline */ &GetEffectiveSkillLevel_ReturnTrampoline
);

/**
 * @brief Overwrites the skill display GetEffectiveSkillLevel() call to display
 *        the actual, non-damaged, skill level.
 *
 * The function that is overwritten by our GetEffectiveSkillLevel() hook is also
 * used to display the skill level in the skills menu.
 *
 * So as to not confuse players, this hook is used to force the skills menu to
 * show the actual skill level, not the damaged value.
 *
 * This hook replaces the call instruction which would call
 * GetEffectiveSkillLevel() with a call to our reimplemented
 * GetEffectiveSkillLevel_Original().
 */
static const CodeSignature kDisplayTrueSkillLevel_PatchSig(
    /* name */       "DisplayTrueSkillLevel",
    /* hook_type */  HookType::Jump6,
    /* hook */       reinterpret_cast<uintptr_t>(DisplayTrueSkillLevel_Hook),
    /* id */         52525,
    /* patch_size */ 7,
    /* trampoline */ &DisplayTrueSkillLevel_ReturnTrampoline,
    /* offset */     0x120
);

/**
 * @brief Lists all the code signatures to be resolved/applied
 *        by ApplyGamePatches().
 */
static const CodeSignature *const kGameSignatures[] = {
    &kThePlayer_ObjectSig,
    &kGameSettingCollection_ObjectSig,
    &kGetLevel_FunctionSig,
    &kGetBaseActorValue_FunctionSig,
    &kGetSkillCoefficients_FunctionSig,
    &kModifyPerkPool_PatchSig,
    &kSkillCapPatch_PatchSig,
    &kLegendaryResetSkillLevel_PatchSig,
    &kCheckConditionForLegendarySkill_PatchSig,
    &kHideLegendaryButton_PatchSig,
    &kImproveSkillByTraining_PatchSig,
    &kImprovePlayerSkillPoints_PatchSig,
    &kImproveLevelExpBySkillLevel_PatchSig,
    &kImproveAttributeWhenLevelUp_PatchSig,
    &kCalculateChargePointsPerUse_PatchSig,
    &kGetEffectiveSkillLevel_PatchSig,
    &kDisplayTrueSkillLevel_PatchSig
};

/// @brief The opcode for an x86 NOP.
static const uint8_t kNop = 0x90;

/**
 * @brief Gets a pointer to the player object.
 */
PlayerCharacter *
GetPlayer() {
    ASSERT(playerObject);
    ASSERT(*playerObject);
    return *playerObject;
}

/**
 * @brief Gets a pointer to the game settings object.
 */
SettingCollectionMap *
GetGameSettings() {
    ASSERT(gameSettings);
    ASSERT(*gameSettings);
    return *gameSettings;
}

/**
 * @brief Gets the base value of the attribute for the given actor.
 * @param actor The actor to get the base value of.
 * @param skill_id The ID of the skill to get the base level of.
 */
float
GetBaseActorValue(
    void *actor,
    UInt32 skill_id
) {
    ASSERT(GetBaseActorValue_Entry);
    return GetBaseActorValue_Entry(actor, skill_id);
}

/**
 * @brief Gets the level of the given actor.
 * @param actor The actor to get the level of.
 */
UInt16
GetLevel(
    void *actor
) {
    ASSERT(GetLevel_Entry);
    return GetLevel_Entry(actor);
}

/**
 * @brief Gets the coefficents for the given skill.
 * @param skill_id The ID of the skill.
 * @param a The first coefficient.
 * @param b The second coefficient.
 * @param c The third coefficient.
 * @param d The fourth coefficient.
 * @return Unknown Bool (success?).
 */
bool
GetSkillCoefficients(
    UInt32 skill_id,
    float &a,
    float &b,
    float &c,
    float &d
) {
    ASSERT(GetSkillCoefficients_Entry);
    return GetSkillCoefficients_Entry(skill_id, &a, &b, &c, &d);
}

/**
 * @brief Applies all of this plugins patches to the skyrim AE binary.
 */
void
ApplyGamePatches() {
    const size_t kNumSigs = sizeof(kGameSignatures) / sizeof(void*);

    _MESSAGE("Applying game patches...");

    auto db = VersionDb();
    db.Load();

    for (size_t i = 0; i < kNumSigs; i++) {
        auto sig = kGameSignatures[i];
        unsigned long long id = sig->id;

#ifdef _DEBUG
        if (sig->known_offset) {
            ASSERT(db.FindIdByOffset(sig->known_offset, id));
        }
#endif

        void *addr = db.FindAddressById(id);
        ASSERT(addr);
        uintptr_t real_address = reinterpret_cast<uintptr_t>(addr) + sig->offset;

        _MESSAGE(
            "Signature %s ([ID: %zu] + 0x%zx) is at offset 0x%zx",
            sig->name,
            id,
            sig->offset,
            real_address - RelocationManager::s_baseAddr
        );

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
