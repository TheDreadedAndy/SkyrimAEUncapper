// FIXME: Using the SKSE pointers for the player and game settings kills
// version independence.

#include "GameFormComponents.h"
#include "GameReferences.h"
#include "GameSettings.h"

#include "Hook_Skill.h"
#include "Settings.h"
#include "RelocFn.h"

using LevelData = PlayerSkills::StatData::LevelData;

/**
 * @brief Gets the base level of a skill on the player character.
 *
 * The player pointer offset here is a magic constant. It seems to be accessing
 * the actorState field in the object (which is private).
 *
 * @param skill_id The ID of the skill to get the base level of.
 */
static unsigned int
GetPlayerBaseSkillLevel(
    unsigned int skill_id
) {
    return static_cast<unsigned int>(
        GetBaseActorValue(reinterpret_cast<char*>(*g_thePlayer) + 0xB8, skill_id)
    );
}

/**
 * @brief Gets the current level of the player.
 */
static unsigned int
GetPlayerLevel() {
    return GetLevel(*g_thePlayer);
}

#if 0
float CalculateSkillExpForLevel(UInt32 skillID, float skillLevel)
{
    float result = 0.0f;
    float fSkillUseCurve = 1.95f;//0x01D88258;
    if ((*g_gameSettingCollection) != nullptr)
    {
        auto pSetting = (*g_gameSettingCollection)->Get("fSkillUseCurve");
        fSkillUseCurve = (pSetting != nullptr) ? pSetting->data.f32 : 1.95f;
    }
    if (skillLevel < settings.GetSkillCap(skillID))
    {
        result = pow(skillLevel, fSkillUseCurve);
        float a = 1.0f, b = 0.0f, c = 1.0f, d = 0.0f;
        if (GetSkillCoefficients(skillID, &a, &b, &c, &d))
            result = result * c + d;
    }
    return result;
}
#endif

#if 0
float CalculateChargePointsPerUse_Hook(float basePoints, float enchantingLevel)
{
    float fEnchantingCostExponent = 1.10f;// 0x01D8A058;             //1.10
    float fEnchantingSkillCostBase = 0.005f; // 0x01D8A010;         //1/200 = 0.005
    float fEnchantingSkillCostScale = 0.5f;// 0x01D8A040;         //0.5 sqrt
    //RelocPtr<float> unk0 = 0x014E8F78;             //1.00f
    float fEnchantingSkillCostMult = 3.00f;// 0x01D8A028;             //3.00

    if ((*g_gameSettingCollection) != nullptr)
    {
        Setting * pSetting = (*g_gameSettingCollection)->Get("fEnchantingCostExponent");
        fEnchantingCostExponent = (pSetting != nullptr) ? pSetting->data.f32 : 1.10f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostBase");
        fEnchantingSkillCostBase = (pSetting != nullptr) ? pSetting->data.f32 : 0.005f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostScale");
        fEnchantingSkillCostScale = (pSetting != nullptr) ? pSetting->data.f32 : 0.5f;
        pSetting = (*g_gameSettingCollection)->Get("fEnchantingSkillCostMult");
        fEnchantingSkillCostMult = (pSetting != nullptr) ? pSetting->data.f32 : 3.00f;
    }

    enchantingLevel = (enchantingLevel > 199.0f) ? 199.0f : enchantingLevel;
    float result = fEnchantingSkillCostMult * pow(basePoints, fEnchantingCostExponent) * (1.00f - pow((enchantingLevel * fEnchantingSkillCostBase), fEnchantingSkillCostScale));

    return result;
    //Charges Per Use = 3 * (base enchantment cost * magnitude / maximum magnitude)^1.1 * (1 - sqrt(skill/200))
}
#endif

#if 0
void ImproveSkillByTraining_Hook(void* pPlayer, UInt32 skillID, UInt32 count)
{
    PlayerSkills* skillData = *reinterpret_cast<PlayerSkills**>(reinterpret_cast<uintptr_t>(pPlayer)+0x9B0);
    if (count < 1)
        count = 1;
    if ((skillID >= 6) && (skillID <= 23))
    {
        LevelData* levelData = &(skillData->data->levelData[skillID - 6]);
        float skillProgression = 0.0f;
        if (levelData->pointsMax > 0.0f)
        {
            skillProgression = levelData->points / levelData->pointsMax;
#ifdef _DEBUG
            _MESSAGE("player:%p, skill:%d, points:%.2f, maxPoints:%.2f, level:%.2f", pPlayer, skillID, levelData->points, levelData->pointsMax, levelData->level);
#endif
            if (skillProgression >= 1.0f)
                skillProgression = 0.99f;
        }
        else
            skillProgression = 0.0f;
        for (UInt32 i = 0; i < count; ++i)
        {
            float skillLevel = GetBaseActorValue((char*)(*g_thePlayer) + 0xB0, skillID);
            float expRequired = CalculateSkillExpForLevel(skillID, skillLevel);
#ifdef _DEBUG
            _MESSAGE("maxPoints:%.2f, expRequired:%.2f", levelData->pointsMax, expRequired);
#endif
            if (levelData->pointsMax != expRequired)
                levelData->pointsMax = expRequired;
            if (levelData->points <= 0.0f)
                levelData->points = (levelData->pointsMax > 0.0f) ? 0.1f : 0.0f;
            if (levelData->points >= levelData->pointsMax)
                levelData->points = (levelData->pointsMax > 0.0f) ? (levelData->pointsMax - 0.1f) : 0.0f;
            float expNeeded = levelData->pointsMax - levelData->points;
            ImprovePlayerSkillPoints_Original(skillData, skillID, expNeeded, 0, 0, 0, (i < count - 1));
        }
        levelData->points += levelData->pointsMax * skillProgression;
    }
}
#endif

static void
ImprovePlayerSkillPoints_Hook(
    PlayerSkills* skillData,
    UInt32 skillID,
    float exp,
    UInt64 unk1,
    UInt32 unk2,
    UInt8 unk3,
    bool unk4
) {
    if (settings.IsManagedSkill(skillID)) {
        exp *= settings.GetSkillExpGainMult(
            skillID,
            GetPlayerBaseSkillLevel(skillID),
            GetPlayerLevel()
        );
    }

    ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

extern "C" float
ImproveLevelExpBySkillLevel_Hook(
    float exp,
    UInt32 skill_id
) {
    if (settings.IsManagedSkill(skill_id)) {
        exp *= settings.GetLevelSkillExpMult(
            skill_id,
            GetPlayerBaseSkillLevel(skill_id),
            GetPlayerLevel()
        );
    }

    return exp;
}

UInt64
ImproveAttributeWhenLevelUp_Hook(
    void* unk0,
    UInt8 unk1
) {

    Setting *iAVDhmsLevelUp = (*g_gameSettingCollection)->Get("iAVDhmsLevelUp");
    Setting *fLevelUpCarryWeightMod = (*g_gameSettingCollection)->Get("fLevelUpCarryWeightMod");

    ASSERT(iAVDhmsLevelUp);
    ASSERT(fLevelUpCarryWeightMod);

    Settings::player_attr_e choice =
        *reinterpret_cast<Settings::player_attr_e*>(static_cast<char*>(unk0) + 0x18);

    settings.GetAttributeLevelUp(
        GetPlayerLevel(),
        choice,
        iAVDhmsLevelUp->data.u32,
        fLevelUpCarryWeightMod->data.f32
    );

    return ImproveAttributeWhenLevelUp_Original(unk0, unk1);
}

extern "C" void
ModifyPerkPool_Hook(
    SInt8 count
) {
    UInt8* points = &((*g_thePlayer)->numPerkPoints);
    if (count > 0) { // Add perk points
        UInt32 sum = settings.GetPerkDelta(GetPlayerLevel()) + *points;
        *points = (sum > 0xFF) ? 0xFF : static_cast<UInt8>(sum);
    } else { // Remove perk points
        SInt32 sum = *points + count;
        *points = (sum < 0) ? 0 : static_cast<UInt8>(sum);
    }
}

extern "C" float
GetSkillCap_Hook(
    UInt32 skill_id
) {
    return settings.GetSkillCap(skill_id);
}

float
GetEffectiveSkillLevel_Hook(
    void *av,
    uint32_t skill_id
) {
    float val = GetEffectiveSkillLevel_Original(av, skill_id);

    if (settings.IsManagedSkill(skill_id)) {
        float cap = settings.GetSkillFormulaCap(skill_id);
        val = (val <= 0) ? 0 : ((val >= cap) ? cap : val);
    }

    return val;
}

#if 0
void LegendaryResetSkillLevel_Hook(float baseLevel, UInt32 skillID)
{
    if ((*g_gameSettingCollection) != nullptr)
    {
        Setting * fLegendarySkillResetValue = (*g_gameSettingCollection)->Get("fLegendarySkillResetValue");
        if (fLegendarySkillResetValue != nullptr)
        {
            static float originalSetting = fLegendarySkillResetValue->data.f32;
            if ((skillID >= 6) && (skillID <= 23))
            {
                if (settings.settingsLegendarySkill.bLegendaryKeepSkillLevel)
                    fLegendarySkillResetValue->data.f32 = baseLevel;
                else
                {
                    UInt32 legendaryLevel = settings.settingsLegendarySkill.iSkillLevelAfterLegendary;
                    if ((legendaryLevel && legendaryLevel > baseLevel) || (!legendaryLevel && originalSetting > baseLevel))
                        fLegendarySkillResetValue->data.f32 = baseLevel;
                    else
                        fLegendarySkillResetValue->data.f32 = (!legendaryLevel) ? originalSetting : legendaryLevel;
                }
            }
            else
                fLegendarySkillResetValue->data.f32 = originalSetting;
        }
    }
}

bool CheckConditionForLegendarySkill_Hook(void* pActorValueOwner, UInt32 skillID)
{
    float skillLevel = GetBaseActorValue(*(char**)(g_thePlayer.GetPtr()) + 0xB0, skillID);
    return (skillLevel >= settings.settingsLegendarySkill.iSkillLevelEnableLegendary) ? true : false;
}
#endif

/**
 * @brief Determines if the legendary button should be displayed for the given
 *        skill based on the users settings.
 */
extern "C" bool
HideLegendaryButton_Hook(
    UInt32 skill_id
) {
    float skill_level = GetPlayerBaseSkillLevel(skill_id);
    return settings.IsLegendaryButtonVisible(skill_level);
}
