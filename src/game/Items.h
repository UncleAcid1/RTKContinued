// Items: the item list (items.xml), the item packs (item_packs.xml) and buildings or decorations
// handed out as items. Port of Items (@0x1ad190..0x1b07e0, libkingdom.so 5.11).
//
// Every ItemInfo is in one list (0x613580, file order; EnumItemInfo). Items and the building /
// decoration items (id 0x4000 + building id, 0x8000 + decoration id) are also found by id
// (GetItemInfo, the hash 0x61356c); packs all have id 0x2000 and are found by pack id.
//
// Not ported: ChangeItemAttribute / ChangeItemsOfSlotAttributes / ChangeItemsOfTypeAttributes and
// SetTransactionTime (the server's item changes and its clock for timed discounts; online only).
// GetItemOffsetInHolder (the item art's offsets in UI holders) comes with the item windows.
#pragma once
#include <cstdint>

class MetaData;
class MetaExpression;
namespace Render { struct Texture; }
namespace Tags { struct Tag; }
namespace GUI { class Window; }

namespace Items {

struct ItemInfo {                      // 0xb4 bytes
    uint32_t id = 0;                   // +0x00 "item_id"
    const char* name = nullptr;        // +0x04 "name" (also the StringTable key)
    const char32_t* title = nullptr;   // +0x08 the name's text, with the quality word for item_level 1..4
    const char* iconName = nullptr;    // +0x0c "icon" (null when empty)
    Render::Texture* icon = nullptr;   // +0x10 GetIcon (GIVE_TROOPS items: the unit's side0 image)
    Render::Texture* dropIcon = nullptr;   // +0x14 GetDropIcon
    bool iconMissing = false;          // +0x18
    bool dropIconMissing = false;      // +0x19
    uint32_t type = 0;                 // +0x1c "type" (0x80 pack, 0x81 building, 0x82 decoration)
    uint32_t shopTab = 0;              // +0x20 "shop_tab"
    uint32_t slot = 0;                 // +0x24 "slot" (the equipment slot, external numbering)
    bool sellable = false;             // +0x28 "sellable"
    bool premium = false;              // +0x29 "premium"
    uint32_t cost = 0;                 // +0x2c "cost" gold
    uint32_t cost2 = 0;                // +0x30 "cost2" crystals
    uint32_t costItem = 0;             // +0x34 "cost_items" id=count
    uint32_t costItemCount = 0;        // +0x38
    uint32_t level = 0;                // +0x3c "level" the player level needed
    uint32_t reputationId = 0;         // +0x40 "reputation_id" (a profession)
    uint32_t reputationLevel = 0;      // +0x44 "reputation_level"
    uint32_t giftLevel = 0;            // +0x48 InitSpecial: the "gift_lvl_lock" setting
    uint32_t successRate = 0;          // +0x4c "success_rate"
    float successModifier = 0.f;       // +0x50 "success_modifier"
    bool isCraft = false;              // +0x54 "is_craft"
    uint32_t activatedByQuest = 0;     // +0x58 "activated_by_quest"
    uint32_t weight = 0;               // +0x5c "weight"
    uint32_t itemLevel = 0;            // +0x60 "item_level" 0..4 (5 is read as 4)
    MetaExpression* meta = nullptr;    // +0x64 "bonus" (only when longer than one character)
    uint32_t durability = 0;           // +0x68 "durability"
    uint32_t deathPenalty = 0;         // +0x6c "death_penalty"
    uint32_t packId = 0;               // +0x70 a pack's "id"
    MetaData* packItems = nullptr;     // +0x74 a pack's items: a list of (id, count) lists
    uint32_t sortId = 0;               // +0x78 "sort_id"
    uint32_t discount = 0;             // +0x7c (server) percent off
    uint32_t discountFrom = 0;         // +0x80 (server)
    uint32_t discountTo = 0;           // +0x84 (server)
    uint32_t discountType = 0xffffffff;  // +0x88 1 gold, 2 crystals, 4 item cost
    bool isNew = false;                // +0x8c (server "new")
    bool remains = false;              // +0x8d "remains"
    int16_t levelLimitMin = 0;         // +0x8e "level_limit" "a" or "a-b"
    int16_t levelLimitMax = 0;         // +0x90
    bool f92 = false;                  // +0x92
    int holderX = 0, holderY = 0;      // +0x94 +0x98 GetItemOffsetInHolder (large)
    bool f9c = false;                  // +0x9c
    int holderSmallX = 0, holderSmallY = 0;   // +0xa0 +0xa4 (small)
    int offsetX = 0, offsetY = 0;      // +0xa8 +0xac per-item corrections
    Tags::Tag* tag = nullptr;          // +0xb0 "tags"

    int GetDiscountSize(uint32_t type) const;   // @0x1ad190 100, or 100 - discount in the window
    uint32_t GetCostGold(bool full) const;      // @0x1ad1d4 (full: without the discount)
    uint32_t GetCostCrystals(bool full) const;  // @0x1ad21c
    uint32_t GetCostItem(bool full) const;      // @0x1ad264
    bool IsLimited() const;                     // @0x1ad5c0 has LIMIT_QUANTITY
    bool CanBeActivated() const;                // @0x1ad5e4 an orb (ORB_ATTACK..ORB_VAMPIRE)
    int GetItemCountLimit() const;              // @0x1ad760 LIMIT_QUANTITY, else 10000
    int GetAttackHealthReturn() const;          // @0x1adcac ORB_VAMPIRE's first value
    int GetCriticalChanceBonus() const;         // @0x1adce4 ORB_CRITICAL's first value
    int GetDamageAbsorption() const;            // @0x1add1c ORB_DEFENSE's first value
    int GetDamageBoost() const;                 // @0x1add54 ORB_ATTACK's, else ORB_VAMPIRE's second
    bool IsProfessionTool() const;              // @0x1adfb8
    const char32_t* GetItemTypeName(uint32_t context) const;   // @0x1ae01c
    const char* GetItemTypeIcon(uint32_t context) const;       // @0x1ae2e0
    Render::Texture* GetBuffIcon(int size) const;              // @0x1ae424 "<name>_16" / "<name>_24"
    void PrepareItemImage();                    // @0x1ae484 building / decoration items
    Render::Texture* GetIcon();                 // @0x1ae51c
    void PutImageInHolder(GUI::Window* w);      // @0x1ae58c
    Render::Texture* GetDropIcon();             // @0x1ae610 "<icon>_drop", else the icon
    int ApplyLimitToAmount(int amount) const;   // @0x1ae6f0
    bool IsLockedByReputation(bool any) const;  // @0x1ae734
    bool IsLockedByLevel() const;               // @0x1ae790
    bool IsLocked() const;                      // @0x1ae7ac
    bool IsOnSale() const;                      // @0x1ae7d4 sellable and its tag (if any) active
    int GetUseCount() const;                    // @0x1ae7f8 an orb's charges
};

bool Init(const char* file);           // @0x1b07e0
bool InitPacks(const char* file);      // @0x1b0118
void InitSpecial();                    // @0x1addbc
void Deinit();                         // @0x1aec0c

ItemInfo* GetItemInfo(uint32_t id);    // @0x1ad2f4
ItemInfo* GetItemInfo(const char* name);   // @0x1aea94
ItemInfo* GetItemPackInfo(uint32_t packId);   // @0x1aea2c
ItemInfo* GetItemPackWithItem(uint32_t id);   // @0x1aeed4 the first pack holding item id
ItemInfo* EnumItemInfo(unsigned i);    // @0x1aefd8
ItemInfo* GetRandomItemForSlot(uint32_t slot);   // @0x1afb84 (not customisations, type 0x11)
void AddDecorationAsItem(uint32_t decorId);      // @0x1aff64 item 0x8000 + id
void AddBuildingAsItem(uint32_t buildingId);     // @0x1b0038 item 0x4000 + id
// @0x1ad650: the id of the player's best gathering tool (the highest GATHER_CHANCE among items
// without GATHER_TIME), else 0.
uint32_t GetPlayersBestNet();
// @0x1ad6e0: how many boost items (0x263 goblin, 0x269 time, else build speed-up) cover seconds.
int GetAmountToBoostTime(uint32_t id, uint32_t seconds);

}  // namespace Items
