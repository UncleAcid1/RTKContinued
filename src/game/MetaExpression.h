// MetaExpression: the "KEYWORD=value,KEYWORD=value" scripts of the game data (item bonuses, quest
// and map object behaviour). Port of MetaExpression (@0x1c78b8..0x1d6420, 0x10 bytes).
//
// An expression is a linked list with one node per comma-separated part. A node's data is a MetaData
// whose type is the keyword's ExpressionType, with one child: the parsed value (an integer, a string,
// or a ':' / '|' / ';' list; see MetaData.h). ParseSingle tries the keywords in the original's order.
#pragma once

class MetaData;

// The keyword types (MetaData::type of a node's data). Names follow the keywords; aliases share one.
enum ExpressionType {
    kExpInitiative = 5, kExpHpMax = 6, kExpHealthPc = 7, kExpMeleeAttack = 8, kExpMeleeDefense = 9,
    kExpMeleeAbsorb = 10, kExpMeleeDamagePc = 0xb, kExpRangedAttack = 0xc, kExpRangedDefense = 0xd,
    kExpRangedAbsorb = 0xe, kExpRangedDamagePc = 0xf, kExpMagicAttack = 0x10, kExpMagicDefense = 0x11,
    kExpMagicAbsorb = 0x12, kExpMagicDamagePc = 0x13, kExpAllDefense = 0x14, kExpCritChance = 0x15,
    kExpFuryBonus = 0x16, kExpParty = 0x17, kExpGold = 0x18, kExpHp = 0x19, kExpDmg = 0x1a,
    kExpMana = 0x1b, kExpOrbAttack = 0x1c, kExpOrbDefense = 0x1d, kExpOrbCritical = 0x1e,
    kExpOrbVampire = 0x1f, kExpOrbPhoenix = 0x20, kExpSpeed = 0x21, kExpHpRegen = 0x22,
    kExpBeltSlots = 0x23, kExpBeltSize = 0x24, kExpGatherChance = 0x25, kExpGatherTime = 0x26,
    kExpGiveTroops = 0x27, kExpLimitQuantity = 0x28, kExpLuck = 0x29, kExpSpeedGoblin = 0x2a,
    kExpSpeedBuild = 0x2b, kExpSpeedTime = 0x2c, kExpRestoreHp = 0x2d, kExpBuffTime = 0x2e,
    kExpRandomSpell = 0x2f, kExpFoundInPvp = 0x30, kExpAddCustom = 0x31, kExpHideBind = 0x32,
    kExpHideCustom = 0x33, kExpKeepHair = 0x34,
    kExpReplaceObj = 0x35, kExpReplaceAfterQuest = 0x36, kExpReplaceAfterSubQuest = 0x37,
    kExpReplaceAfterItem = 0x38, kExpReplaceAfterInteract = 0x39, kExpMessage = 0x3a,
    kExpInteractMessage = 0x3b, kExpMsgBeforeQuest = 0x3c, kExpMsgAfterQuest = 0x3d, kExpDrop = 0x3e,
    kExpProgressMsg = 0x3f, kExpReqUnlockQuest = 0x40, kExpReqUnlockSubQuest = 0x41,
    kExpReqUnlockItem = 0x42, kExpKill = 0x43, kExpUnhideQuest = 0x44, kExpUnhideSubQuest = 0x45,
    kExpUnhideItem = 0x46, kExpUnhideInteract = 0x47, kExpHideQuest = 0x48, kExpHideSubQuest = 0x49,
    kExpHideItem = 0x4a, kExpHideInteract = 0x4b, kExpUnhideAfterQuest = 0x4c,
    kExpUnhideAfterSubQuest = 0x4d, kExpUnhideAfterInteract = 0x4e, kExpGiveItem = 0x4f, kExpTp = 0x50,
    kExpAp = 0x51, kExpBt = 0x52, kExpNoFade = 0x53, kExpCam = 0x54, kExpTransferTo = 0x55,
    kExpGoblin = 0x56, kExpPortal = 0x57, kExpStartQuestTile = 0x58, kExpEndQuestTile = 0x59,
    kExpEndSubQuestTile = 0x5a, kExpDisableAfterQuest = 0x5b, kExpDisableAfterSubQuest = 0x5c,
    kExpSound = 0x5e, kExpSoundPortal = 0x5f, kExpParticleIdle = 0x60,
    kExpParticleAfterInteract = 0x61, kExpParticleTrap = 0x62, kExpParticleAfterQuest = 0x63,
    kExpParticleAfterSubQuest = 0x64, kExpCam2 = 0x65, kExpCamAfterInteract = 0x66,
    kExpCamAfterQuest = 0x67, kExpCamAfterSubQuest = 0x68, kExpShakeAfterInteract = 0x69,
    kExpShakeAfterQuest = 0x6a, kExpShakeAfterSubQuest = 0x6b, kExpInteractIcon = 0x6c,
    kExpInteractAnim = 0x6d, kExpCraft = 0x6e, kExpTrapDamage = 0x6f, kExpTrapSound = 0x70,
    kExpTrapPattern = 0x71, kExpTrapAnimated = 0x72, kExpChestDrop = 0x73, kExpChestDropType = 0x74,
    kExpSpellAfterQuest = 0x75, kExpSpellAfterSubQuest = 0x76, kExpSpellAfterInteract = 0x77,
    kExpTeleportTile = 0x78, kExpSpellResist = 0x79, kExpHint = 0x7a, kExpGotoAfterQuest = 0x7b,
    kExpGotoAfterSubQuest = 0x7c, kExpHintDead = 0x7d, kExpAnimFinishQuest = 0x7e,
    kExpAnimAfterQuest = 0x7f, kExpAnimAfterSubQuest = 0x80, kExpDirection = 0x81,
    kExpQuestFinishMsg = 0x82, kExpAnimIdle = 0x83, kExpPatrol = 0x84, kExpPatrolTimes = 0x85,
    kExpBoss = 0x86, kExpDropOnce = 0x87, kExpGroup = 0x88, kExpStartQuest = 0x89, kExpEndQuest = 0x8a,
    kExpMapId = 0x8b, kExpGiveQuest = 0x8c, kExpFog = 0x8d, kExpBat = 0x8e, kExpBossStr = 0x8f,
    kExpDropAfterQuest = 0x90, kExpDropBeforeQuest = 0x91, kExpRandom = 0x92, kExpBossTask = 0x93,
    kExpStatsReplaceAfterQuest = 0x94, kExpArenaSpawn = 0x96, kExpMobSpawn = 0x97, kExpPvpSpawn = 0x98,
    kExpUnhideTag = 0x99, kExpUpgrade = 0x9a, kExpGrow = 0x9b, kExpEnergy = 0x9c,
};

class MetaExpression {
public:
    // @0x1d63f8: parses text; with ownsText the destructor frees it (delete[]).
    MetaExpression(const char* text, bool ownsText);
    ~MetaExpression();                                    // @0x1d234c (the rest of the list too)

    MetaExpression* GetNext() const { return next; }     // @0x1c7910
    MetaData* GetData() const { return data; }           // @0x1c7918
    const char* GetExpression() const { return text; }   // @0x1c7eb8
    // @0x1c78e8: the first node from this one whose data has type t, else nullptr.
    MetaExpression* FindByType(int t);
    // @0x1c8598: this node's value if its data has type t, else nullptr.
    const MetaData* GetDataWithType(int t) const;
    // @0x1c85c0: the value of the first node of type t (FindByType), else nullptr.
    const MetaData* FindChildData(int t);

private:
    MetaExpression(MetaExpression* prev, const char* text);   // @0x1d63a0 the node after prev
    void Parse(const char* p);                            // @0x1d6264
    bool ParseSingle(const char*& p);                     // @0x1d2f98

    MetaExpression* next = nullptr;   // +0x00
    MetaData* data = nullptr;         // +0x04
    bool ownsText = false;            // +0x08
    const char* text = nullptr;       // +0x0c the whole text (a later node: the text from its part)
};
