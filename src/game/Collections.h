// Collections: the item improvements (collections.xml): an item ("from") and up to four other
// items in amounts make a better one ("to"). Chains: an improved item can be improved again.
// Port of Collections (libkingdom.so 5.11, @0x12d6ec..0x12e384; statics 0x611850..0x61185c).
#pragma once
#include <cstdint>

namespace Collections {

struct Collection {                        // 0x48 bytes
    uint32_t id = 0;                       // +0x00 "id"
    uint32_t type = 0;                     // +0x04 "type" (0 improvement, 1 crafting)
    uint32_t from = 0;                     // +0x08 "item_id_from"
    uint32_t to = 0;                       // +0x0c "item_id"
    struct Part { uint32_t item = 0, amount = 0; } parts[4];   // +0x10 <Item slot="1..4" item_id amount>
    uint32_t reputationId = 0;             // +0x30 "reputation_id"
    uint32_t reputationLevel = 0;          // +0x34 "reputation_level"
    uint32_t sort = 0;                     // +0x38 "sort"
    uint32_t tab = 0;                      // +0x3c "tab"
    Collection* next = nullptr;            // +0x40 the one that improves "to" further
    Collection* prev = nullptr;            // +0x44 the one that makes "from"
};

bool Init(const char* file);               // @0x12dbd0 (active collections only)
Collection* GetCollection(uint32_t id);    // @0x12d6ec
Collection* EnumCollection(unsigned i);    // @0x12d750
Collection* GetCollectionWithSource(uint32_t item);   // @0x12d774 the improvement (type 0) of item
Collection* GetCollectionWithResult(uint32_t item);   // @0x12d7d8 the one that makes item
Collection* GetCollectionStart(uint32_t item);        // @0x12d83c the first of the chain to item
Collection* GetCollectionEnd(uint32_t item);          // @0x12d874 the last of the chain from item
Collection* GetCraftingCollectionForItem(uint32_t item);   // @0x12d8ac a crafting one (type 1) using it
// @0x12d998 (the admin console): one attribute of one collection changed.
void ChangeCollectionAttribute(uint32_t id, unsigned part, const char* attr, const char* value);
uint32_t GetMaxItemLevel(uint32_t item);   // @0x12dad8 the best item level its chain reaches
bool CanImproveItem(uint32_t item);        // @0x12db20 the player has every part

}  // namespace Collections
