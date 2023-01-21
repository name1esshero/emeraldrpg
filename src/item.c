#include "global.h"
#include "item.h"
#include "berry.h"
#include "string_util.h"
#include "text.h"
#include "event_data.h"
#include "malloc.h"
#include "secret_base.h"
#include "item_menu.h"
#include "strings.h"
#include "load_save.h"
#include "item_use.h"
#include "random.h"
#include "overworld.h"
#include "battle_pyramid.h"
#include "battle_pyramid_bag.h"
#include "item_icon.h"
#include "pokemon_summary_screen.h"
#include "menu.h"
#include "party_menu.h"
#include "overworld.h"
#include "constants/items.h"
#include "constants/hold_effects.h"

// this file's functions
static bool8 CheckPyramidBagHasItem(u16 itemId, u16 count);
static bool8 CheckPyramidBagHasSpace(u16 itemId, u16 count);
static void ShowItemIconSprite(u16 item, bool8 firstTime, bool8 flash);
static void DestroyItemIconSprite(void);

// EWRAM variables
EWRAM_DATA struct BagPocket gBagPockets[POCKETS_COUNT] = {0};
EWRAM_DATA static u8 sHeaderBoxWindowId = 0;
EWRAM_DATA u8 sItemIconSpriteId = 0;
EWRAM_DATA u8 sItemIconSpriteId2 = 0;

// rodata
#include "data/text/item_descriptions.h"
#include "data/items.h"

// code
static u16 GetBagItemQuantity(u16 *quantity)
{
    return gSaveBlock2Ptr->encryptionKey ^ *quantity;
}

static void SetBagItemQuantity(u16 *quantity, u16 newValue)
{
    *quantity =  newValue ^ gSaveBlock2Ptr->encryptionKey;
}

static u16 GetPCItemQuantity(u16 *quantity)
{
    return *quantity;
}

static void SetPCItemQuantity(u16 *quantity, u16 newValue)
{
    *quantity = newValue;
}

void ApplyNewEncryptionKeyToBagItems(u32 newKey)
{
    u32 pocket, item;
    for (pocket = 0; pocket < POCKETS_COUNT; pocket++)
    {
        for (item = 0; item < gBagPockets[pocket].capacity; item++)
            ApplyNewEncryptionKeyToHword(&(gBagPockets[pocket].itemSlots[item].quantity), newKey);
    }
}

void ApplyNewEncryptionKeyToBagItems_(u32 newKey) // really GF?
{
    ApplyNewEncryptionKeyToBagItems(newKey);
}

void SetBagItemsPointers(void)
{
    gBagPockets[ITEMS_POCKET].itemSlots = gSaveBlock1Ptr->bagPocket_Items;
    gBagPockets[ITEMS_POCKET].capacity = BAG_ITEMS_COUNT;

    gBagPockets[KEYITEMS_POCKET].itemSlots = gSaveBlock1Ptr->bagPocket_KeyItems;
    gBagPockets[KEYITEMS_POCKET].capacity = BAG_KEYITEMS_COUNT;

    gBagPockets[BALLS_POCKET].itemSlots = gSaveBlock1Ptr->bagPocket_PokeBalls;
    gBagPockets[BALLS_POCKET].capacity = BAG_POKEBALLS_COUNT;

    gBagPockets[TMHM_POCKET].itemSlots = gSaveBlock1Ptr->bagPocket_TMHM;
    gBagPockets[TMHM_POCKET].capacity = BAG_TMHM_COUNT;

    gBagPockets[BERRIES_POCKET].itemSlots = gSaveBlock1Ptr->bagPocket_Berries;
    gBagPockets[BERRIES_POCKET].capacity = BAG_BERRIES_COUNT;
}

void CopyItemName(u16 itemId, u8 *dst)
{
    StringCopy(dst, ItemId_GetName(itemId));
}

void CopyItemNameHandlePlural(u16 itemId, u8 *dst, u32 quantity)
{
    if (itemId == ITEM_POKE_BALL)
    {
        if (quantity < 2)
            StringCopy(dst, ItemId_GetName(ITEM_POKE_BALL));
        else
            StringCopy(dst, gText_PokeBalls);
    }
    else
    {
        if (itemId >= FIRST_BERRY_INDEX && itemId <= LAST_BERRY_INDEX)
            GetBerryCountString(dst, gBerries[itemId - FIRST_BERRY_INDEX].name, quantity);
        else
            StringCopy(dst, ItemId_GetName(itemId));
    }
}

void GetBerryCountString(u8 *dst, const u8 *berryName, u32 quantity)
{
    const u8 *berryString;
    u8 *txtPtr;

    if (quantity < 2)
        berryString = gText_Berry;
    else
        berryString = gText_Berries;

    txtPtr = StringCopy(dst, berryName);
    *txtPtr = CHAR_SPACE;
    StringCopy(txtPtr + 1, berryString);
}

bool8 IsBagPocketNonEmpty(u8 pocket)
{
    u8 i;

    for (i = 0; i < gBagPockets[pocket - 1].capacity; i++)
    {
        if (gBagPockets[pocket - 1].itemSlots[i].itemId != 0)
            return TRUE;
    }
    return FALSE;
}

bool8 CheckBagHasItem(u16 itemId, u16 count)
{
    u8 i;
    u8 pocket;

    if (ItemId_GetPocket(itemId) == 0)
        return FALSE;
    if (InBattlePyramid() || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG) == TRUE)
        return CheckPyramidBagHasItem(itemId, count);
    pocket = ItemId_GetPocket(itemId) - 1;
    // Check for item slots that contain the item
    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            u16 quantity;
            // Does this item slot contain enough of the item?
            quantity = GetBagItemQuantity(&gBagPockets[pocket].itemSlots[i].quantity);
            if (quantity >= count)
                return TRUE;
            count -= quantity;
            // Does this item slot and all previous slots contain enough of the item?
            if (count == 0)
                return TRUE;
        }
    }
    return FALSE;
}

bool8 HasAtLeastOneBerry(void)
{
    u16 i;

    for (i = FIRST_BERRY_INDEX; i <= LAST_BERRY_INDEX; i++)
    {
        if (CheckBagHasItem(i, 1) == TRUE)
        {
            gSpecialVar_Result = TRUE;
            return TRUE;
        }
    }
    gSpecialVar_Result = FALSE;
    return FALSE;
}

bool8 CheckBagHasSpace(u16 itemId, u16 count)
{
    u8 i;
    u8 pocket;
    u16 slotCapacity;
    u16 ownedCount;

    if (ItemId_GetPocket(itemId) == POCKET_NONE)
        return FALSE;

    if (InBattlePyramid() || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG) == TRUE)
    {
        return CheckPyramidBagHasSpace(itemId, count);
    }

    pocket = ItemId_GetPocket(itemId) - 1;
    if (pocket != BERRIES_POCKET)
        slotCapacity = MAX_BAG_ITEM_CAPACITY;
    else
        slotCapacity = MAX_BERRY_CAPACITY;

    // Check space in any existing item slots that already contain this item
    for (i = 0; i < gBagPockets[pocket].capacity; i++)
    {
        if (gBagPockets[pocket].itemSlots[i].itemId == itemId)
        {
            ownedCount = GetBagItemQuantity(&gBagPockets[pocket].itemSlots[i].quantity);
            if (ownedCount + count <= slotCapacity)
                return TRUE;
            if (pocket == TMHM_POCKET || pocket == BERRIES_POCKET)
                return FALSE;
            count -= (slotCapacity - ownedCount);
            if (count == 0)
                break; //should be return TRUE, but that doesn't match
        }
    }

    // Check space in empty item slots
    if (count > 0)
    {
        for (i = 0; i < gBagPockets[pocket].capacity; i++)
        {
            if (gBagPockets[pocket].itemSlots[i].itemId == 0)
            {
                if (count > slotCapacity)
                {
                    if (pocket == TMHM_POCKET || pocket == BERRIES_POCKET)
                        return FALSE;
                    count -= slotCapacity;
                }
                else
                {
                    count = 0; //should be return TRUE, but that doesn't match
                    break;
                }
            }
        }
        if (count > 0)
            return FALSE; // No more item slots. The bag is full
    }

    return TRUE;
}

bool8 AddBagItem(u16 itemId, u16 count)
{
    u8 i;

    if (ItemId_GetPocket(itemId) == POCKET_NONE)
        return FALSE;

    // check Battle Pyramid Bag
    if (InBattlePyramid() || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG) == TRUE)
    {
        return AddPyramidBagItem(itemId, count);
    }
    else
    {
        struct BagPocket *itemPocket;
        struct ItemSlot *newItems;
        u16 slotCapacity;
        u16 ownedCount;
        u8 pocket = ItemId_GetPocket(itemId) - 1;

        itemPocket = &gBagPockets[pocket];
        newItems = AllocZeroed(itemPocket->capacity * sizeof(struct ItemSlot));
        memcpy(newItems, itemPocket->itemSlots, itemPocket->capacity * sizeof(struct ItemSlot));

        if (pocket != BERRIES_POCKET)
            slotCapacity = MAX_BAG_ITEM_CAPACITY;
        else
            slotCapacity = MAX_BERRY_CAPACITY;

        for (i = 0; i < itemPocket->capacity; i++)
        {
            if (newItems[i].itemId == itemId)
            {
                ownedCount = GetBagItemQuantity(&newItems[i].quantity);
                // check if won't exceed max slot capacity
                if (ownedCount + count <= slotCapacity)
                {
                    // successfully added to already existing item's count
                    SetBagItemQuantity(&newItems[i].quantity, ownedCount + count);
                    memcpy(itemPocket->itemSlots, newItems, itemPocket->capacity * sizeof(struct ItemSlot));
                    Free(newItems);
                    return TRUE;
                }
                else
                {
                    // try creating another instance of the item if possible
                    if (pocket == TMHM_POCKET || pocket == BERRIES_POCKET)
                    {
                        Free(newItems);
                        return FALSE;
                    }
                    else
                    {
                        count -= slotCapacity - ownedCount;
                        SetBagItemQuantity(&newItems[i].quantity, slotCapacity);
                        // don't create another instance of the item if it's at max slot capacity and count is equal to 0
                        if (count == 0)
                        {
                            break;
                        }
                    }
                }
            }
        }

        // we're done if quantity is equal to 0
        if (count > 0)
        {
            // either no existing item was found or we have to create another instance, because the capacity was exceeded
            for (i = 0; i < itemPocket->capacity; i++)
            {
                if (newItems[i].itemId == ITEM_NONE)
                {
                    newItems[i].itemId = itemId;
                    if (count > slotCapacity)
                    {
                        // try creating a new slot with max capacity if duplicates are possible
                        if (pocket == TMHM_POCKET || pocket == BERRIES_POCKET)
                        {
                            Free(newItems);
                            return FALSE;
                        }
                        count -= slotCapacity;
                        SetBagItemQuantity(&newItems[i].quantity, slotCapacity);
                    }
                    else
                    {
                        // created a new slot and added quantity
                        SetBagItemQuantity(&newItems[i].quantity, count);
                        count = 0;
                        break;
                    }
                }
            }

            if (count > 0)
            {
                Free(newItems);
                return FALSE;
            }
        }
        memcpy(itemPocket->itemSlots, newItems, itemPocket->capacity * sizeof(struct ItemSlot));
        Free(newItems);
        return TRUE;
    }
}

bool8 RemoveBagItem(u16 itemId, u16 count)
{
    u8 i;
    u16 totalQuantity = 0;

    if (ItemId_GetPocket(itemId) == POCKET_NONE || itemId == ITEM_NONE)
        return FALSE;

    // check Battle Pyramid Bag
    if (InBattlePyramid() || FlagGet(FLAG_STORING_ITEMS_IN_PYRAMID_BAG) == TRUE)
    {
        return RemovePyramidBagItem(itemId, count);
    }
    else
    {
        u8 pocket;
        u8 var;
        u16 ownedCount;
        struct BagPocket *itemPocket;

        pocket = ItemId_GetPocket(itemId) - 1;
        itemPocket = &gBagPockets[pocket];

        for (i = 0; i < itemPocket->capacity; i++)
        {
            if (itemPocket->itemSlots[i].itemId == itemId)
                totalQuantity += GetBagItemQuantity(&itemPocket->itemSlots[i].quantity);
        }

        if (totalQuantity < count)
            return FALSE;   // We don't have enough of the item

        if (CurMapIsSecretBase() == TRUE)
        {
            VarSet(VAR_SECRET_BASE_LOW_TV_FLAGS, VarGet(VAR_SECRET_BASE_LOW_TV_FLAGS) | SECRET_BASE_USED_BAG);
            VarSet(VAR_SECRET_BASE_LAST_ITEM_USED, itemId);
        }

        var = GetItemListPosition(pocket);
        if (itemPocket->capacity > var
         && itemPocket->itemSlots[var].itemId == itemId)
        {
            ownedCount = GetBagItemQuantity(&itemPocket->itemSlots[var].quantity);
            if (ownedCount >= count)
            {
                SetBagItemQuantity(&itemPocket->itemSlots[var].quantity, ownedCount - count);
                count = 0;
            }
            else
            {
                count -= ownedCount;
                SetBagItemQuantity(&itemPocket->itemSlots[var].quantity, 0);
            }

            if (GetBagItemQuantity(&itemPocket->itemSlots[var].quantity) == 0)
                itemPocket->itemSlots[var].itemId = ITEM_NONE;

            if (count == 0)
                return TRUE;
        }

        for (i = 0; i < itemPocket->capacity; i++)
        {
            if (itemPocket->itemSlots[i].itemId == itemId)
            {
                ownedCount = GetBagItemQuantity(&itemPocket->itemSlots[i].quantity);
                if (ownedCount >= count)
                {
                    SetBagItemQuantity(&itemPocket->itemSlots[i].quantity, ownedCount - count);
                    count = 0;
                }
                else
                {
                    count -= ownedCount;
                    SetBagItemQuantity(&itemPocket->itemSlots[i].quantity, 0);
                }

                if (GetBagItemQuantity(&itemPocket->itemSlots[i].quantity) == 0)
                    itemPocket->itemSlots[i].itemId = ITEM_NONE;

                if (count == 0)
                    return TRUE;
            }
        }
        return TRUE;
    }
}

u8 GetPocketByItemId(u16 itemId)
{
    return ItemId_GetPocket(itemId);
}

void ClearItemSlots(struct ItemSlot *itemSlots, u8 itemCount)
{
    u16 i;

    for (i = 0; i < itemCount; i++)
    {
        itemSlots[i].itemId = ITEM_NONE;
        SetBagItemQuantity(&itemSlots[i].quantity, 0);
    }
}

static s32 FindFreePCItemSlot(void)
{
    s8 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == ITEM_NONE)
            return i;
    }
    return -1;
}

u8 CountUsedPCItemSlots(void)
{
    u8 usedSlots = 0;
    u8 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId != ITEM_NONE)
            usedSlots++;
    }
    return usedSlots;
}

bool8 CheckPCHasItem(u16 itemId, u16 count)
{
    u8 i;

    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (gSaveBlock1Ptr->pcItems[i].itemId == itemId && GetPCItemQuantity(&gSaveBlock1Ptr->pcItems[i].quantity) >= count)
            return TRUE;
    }
    return FALSE;
}

bool8 AddPCItem(u16 itemId, u16 count)
{
    u8 i;
    s8 freeSlot;
    u16 ownedCount;
    struct ItemSlot *newItems;

    // Copy PC items
    newItems = AllocZeroed(sizeof(gSaveBlock1Ptr->pcItems));
    memcpy(newItems, gSaveBlock1Ptr->pcItems, sizeof(gSaveBlock1Ptr->pcItems));

    // Use any item slots that already contain this item
    for (i = 0; i < PC_ITEMS_COUNT; i++)
    {
        if (newItems[i].itemId == itemId)
        {
            ownedCount = GetPCItemQuantity(&newItems[i].quantity);
            if (ownedCount + count <= MAX_PC_ITEM_CAPACITY)
            {
                SetPCItemQuantity(&newItems[i].quantity, ownedCount + count);
                memcpy(gSaveBlock1Ptr->pcItems, newItems, sizeof(gSaveBlock1Ptr->pcItems));
                Free(newItems);
                return TRUE;
            }
            count += ownedCount - MAX_PC_ITEM_CAPACITY;
            SetPCItemQuantity(&newItems[i].quantity, MAX_PC_ITEM_CAPACITY);
            if (count == 0)
            {
                memcpy(gSaveBlock1Ptr->pcItems, newItems, sizeof(gSaveBlock1Ptr->pcItems));
                Free(newItems);
                return TRUE;
            }
        }
    }

    // Put any remaining items into a new item slot.
    if (count > 0)
    {
        freeSlot = FindFreePCItemSlot();
        if (freeSlot == -1)
        {
            Free(newItems);
            return FALSE;
        }
        else
        {
            newItems[freeSlot].itemId = itemId;
            SetPCItemQuantity(&newItems[freeSlot].quantity, count);
        }
    }

    // Copy items back to the PC
    memcpy(gSaveBlock1Ptr->pcItems, newItems, sizeof(gSaveBlock1Ptr->pcItems));
    Free(newItems);
    return TRUE;
}

void RemovePCItem(u8 index, u16 count)
{
    gSaveBlock1Ptr->pcItems[index].quantity -= count;
    if (gSaveBlock1Ptr->pcItems[index].quantity == 0)
    {
        gSaveBlock1Ptr->pcItems[index].itemId = ITEM_NONE;
        CompactPCItems();
    }
}

void CompactPCItems(void)
{
    u16 i;
    u16 j;

    for (i = 0; i < PC_ITEMS_COUNT - 1; i++)
    {
        for (j = i + 1; j < PC_ITEMS_COUNT; j++)
        {
            if (gSaveBlock1Ptr->pcItems[i].itemId == 0)
            {
                struct ItemSlot temp = gSaveBlock1Ptr->pcItems[i];
                gSaveBlock1Ptr->pcItems[i] = gSaveBlock1Ptr->pcItems[j];
                gSaveBlock1Ptr->pcItems[j] = temp;
            }
        }
    }
}

void SwapRegisteredBike(void)
{
    switch (gSaveBlock1Ptr->registeredItem)
    {
    case ITEM_MACH_BIKE:
        gSaveBlock1Ptr->registeredItem = ITEM_ACRO_BIKE;
        break;
    case ITEM_ACRO_BIKE:
        gSaveBlock1Ptr->registeredItem = ITEM_MACH_BIKE;
        break;
    }
}

u16 BagGetItemIdByPocketPosition(u8 pocketId, u16 pocketPos)
{
    return gBagPockets[pocketId - 1].itemSlots[pocketPos].itemId;
}

u16 BagGetQuantityByPocketPosition(u8 pocketId, u16 pocketPos)
{
    return GetBagItemQuantity(&gBagPockets[pocketId - 1].itemSlots[pocketPos].quantity);
}

static void SwapItemSlots(struct ItemSlot *a, struct ItemSlot *b)
{
    struct ItemSlot temp;
    SWAP(*a, *b, temp);
}

void CompactItemsInBagPocket(struct BagPocket *bagPocket)
{
    u16 i, j;

    for (i = 0; i < bagPocket->capacity - 1; i++)
    {
        for (j = i + 1; j < bagPocket->capacity; j++)
        {
            if (GetBagItemQuantity(&bagPocket->itemSlots[i].quantity) == 0)
                SwapItemSlots(&bagPocket->itemSlots[i], &bagPocket->itemSlots[j]);
        }
    }
}

void SortBerriesOrTMHMs(struct BagPocket *bagPocket)
{
    u16 i, j;

    for (i = 0; i < bagPocket->capacity - 1; i++)
    {
        for (j = i + 1; j < bagPocket->capacity; j++)
        {
            if (GetBagItemQuantity(&bagPocket->itemSlots[i].quantity) != 0)
            {
                if (GetBagItemQuantity(&bagPocket->itemSlots[j].quantity) == 0)
                    continue;
                if (bagPocket->itemSlots[i].itemId <= bagPocket->itemSlots[j].itemId)
                    continue;
            }
            SwapItemSlots(&bagPocket->itemSlots[i], &bagPocket->itemSlots[j]);
        }
    }
}

void MoveItemSlotInList(struct ItemSlot* itemSlots_, u32 from, u32 to_)
{
    // dumb assignments needed to match
    struct ItemSlot *itemSlots = itemSlots_;
    u32 to = to_;

    if (from != to)
    {
        s16 i, count;
        struct ItemSlot firstSlot = itemSlots[from];

        if (to > from)
        {
            to--;
            for (i = from, count = to; i < count; i++)
                itemSlots[i] = itemSlots[i + 1];
        }
        else
        {
            for (i = from, count = to; i > count; i--)
                itemSlots[i] = itemSlots[i - 1];
        }
        itemSlots[to] = firstSlot;
    }
}

void ClearBag(void)
{
    u16 i;

    for (i = 0; i < POCKETS_COUNT; i++)
    {
        ClearItemSlots(gBagPockets[i].itemSlots, gBagPockets[i].capacity);
    }
}

u16 CountTotalItemQuantityInBag(u16 itemId)
{
    u16 i;
    u16 ownedCount = 0;
    struct BagPocket *bagPocket = &gBagPockets[ItemId_GetPocket(itemId) - 1];

    for (i = 0; i < bagPocket->capacity; i++)
    {
        if (bagPocket->itemSlots[i].itemId == itemId)
            ownedCount += GetBagItemQuantity(&bagPocket->itemSlots[i].quantity);
    }

    return ownedCount;
}

static bool8 CheckPyramidBagHasItem(u16 itemId, u16 count)
{
    u8 i;
    u16 *items = gSaveBlock2Ptr->frontier.pyramidBag.itemId[gSaveBlock2Ptr->frontier.lvlMode];
    u8 *quantities = gSaveBlock2Ptr->frontier.pyramidBag.quantity[gSaveBlock2Ptr->frontier.lvlMode];

    for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; i++)
    {
        if (items[i] == itemId)
        {
            if (quantities[i] >= count)
                return TRUE;

            count -= quantities[i];
            if (count == 0)
                return TRUE;
        }
    }

    return FALSE;
}

static bool8 CheckPyramidBagHasSpace(u16 itemId, u16 count)
{
    u8 i;
    u16 *items = gSaveBlock2Ptr->frontier.pyramidBag.itemId[gSaveBlock2Ptr->frontier.lvlMode];
    u8 *quantities = gSaveBlock2Ptr->frontier.pyramidBag.quantity[gSaveBlock2Ptr->frontier.lvlMode];

    for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; i++)
    {
        if (items[i] == itemId || items[i] == ITEM_NONE)
        {
            if (quantities[i] + count <= MAX_BAG_ITEM_CAPACITY)
                return TRUE;

            count = (quantities[i] + count) - MAX_BAG_ITEM_CAPACITY;
            if (count == 0)
                return TRUE;
        }
    }

    return FALSE;
}

bool8 AddPyramidBagItem(u16 itemId, u16 count)
{
    u16 i;

    u16 *items = gSaveBlock2Ptr->frontier.pyramidBag.itemId[gSaveBlock2Ptr->frontier.lvlMode];
    u8 *quantities = gSaveBlock2Ptr->frontier.pyramidBag.quantity[gSaveBlock2Ptr->frontier.lvlMode];

    u16 *newItems = Alloc(PYRAMID_BAG_ITEMS_COUNT * sizeof(*newItems));
    u16 *newQuantities = Alloc(PYRAMID_BAG_ITEMS_COUNT * sizeof(*newQuantities));

    memcpy(newItems, items, PYRAMID_BAG_ITEMS_COUNT * sizeof(*newItems));
    memcpy(newQuantities, quantities, PYRAMID_BAG_ITEMS_COUNT * sizeof(*newQuantities));

    for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; i++)
    {
        if (newItems[i] == itemId && newQuantities[i] < MAX_BAG_ITEM_CAPACITY)
        {
            newQuantities[i] += count;
            if (newQuantities[i] > MAX_BAG_ITEM_CAPACITY)
            {
                count = newQuantities[i] - MAX_BAG_ITEM_CAPACITY;
                newQuantities[i] = MAX_BAG_ITEM_CAPACITY;
            }
            else
            {
                count = 0;
            }

            if (count == 0)
                break;
        }
    }

    if (count > 0)
    {
        for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; i++)
        {
            if (newItems[i] == ITEM_NONE)
            {
                newItems[i] = itemId;
                newQuantities[i] = count;
                if (newQuantities[i] > MAX_BAG_ITEM_CAPACITY)
                {
                    count = newQuantities[i] - MAX_BAG_ITEM_CAPACITY;
                    newQuantities[i] = MAX_BAG_ITEM_CAPACITY;
                }
                else
                {
                    count = 0;
                }

                if (count == 0)
                    break;
            }
        }
    }

    if (count == 0)
    {
        memcpy(items, newItems, PYRAMID_BAG_ITEMS_COUNT * sizeof(*items));
        memcpy(quantities, newQuantities, PYRAMID_BAG_ITEMS_COUNT * sizeof(*quantities));
        Free(newItems);
        Free(newQuantities);
        return TRUE;
    }
    else
    {
        Free(newItems);
        Free(newQuantities);
        return FALSE;
    }
}

bool8 RemovePyramidBagItem(u16 itemId, u16 count)
{
    u16 i;

    u16 *items = gSaveBlock2Ptr->frontier.pyramidBag.itemId[gSaveBlock2Ptr->frontier.lvlMode];
    u8 *quantities = gSaveBlock2Ptr->frontier.pyramidBag.quantity[gSaveBlock2Ptr->frontier.lvlMode];

    i = gPyramidBagMenuState.cursorPosition + gPyramidBagMenuState.scrollPosition;
    if (items[i] == itemId && quantities[i] >= count)
    {
        quantities[i] -= count;
        if (quantities[i] == 0)
            items[i] = ITEM_NONE;
        return TRUE;
    }
    else
    {
        u16 *newItems = Alloc(PYRAMID_BAG_ITEMS_COUNT * sizeof(*newItems));
        u8 *newQuantities = Alloc(PYRAMID_BAG_ITEMS_COUNT * sizeof(*newQuantities));

        memcpy(newItems, items, PYRAMID_BAG_ITEMS_COUNT * sizeof(*newItems));
        memcpy(newQuantities, quantities, PYRAMID_BAG_ITEMS_COUNT * sizeof(*newQuantities));

        for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; i++)
        {
            if (newItems[i] == itemId)
            {
                if (newQuantities[i] >= count)
                {
                    newQuantities[i] -= count;
                    count = 0;
                    if (newQuantities[i] == 0)
                        newItems[i] = ITEM_NONE;
                }
                else
                {
                    count -= newQuantities[i];
                    newQuantities[i] = 0;
                    newItems[i] = ITEM_NONE;
                }

                if (count == 0)
                    break;
            }
        }

        if (count == 0)
        {
            memcpy(items, newItems, PYRAMID_BAG_ITEMS_COUNT * sizeof(*items));
            memcpy(quantities, newQuantities, PYRAMID_BAG_ITEMS_COUNT * sizeof(*quantities));
            Free(newItems);
            Free(newQuantities);
            return TRUE;
        }
        else
        {
            Free(newItems);
            Free(newQuantities);
            return FALSE;
        }
    }
}

static u16 SanitizeItemId(u16 itemId)
{
    if (itemId >= ITEMS_COUNT)
        return ITEM_NONE;
    else
        return itemId;
}

const u8 *ItemId_GetName(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].name;
}

u16 ItemId_GetPrice(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].price;
}

u8 ItemId_GetHoldEffect(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].holdEffect;
}

u8 ItemId_GetHoldEffectParam(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].holdEffectParam;
}

const u8 *ItemId_GetDescription(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].description;
}

u8 ItemId_GetImportance(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].importance;
}

// Unused
u8 ItemId_GetRegistrability(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].registrability;
}

u8 ItemId_GetPocket(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].pocket;
}

u8 ItemId_GetType(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].type;
}

ItemUseFunc ItemId_GetFieldFunc(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].fieldUseFunc;
}

u8 ItemId_GetBattleUsage(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].battleUsage;
}

ItemUseFunc ItemId_GetBattleFunc(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].battleUseFunc;
}

u8 ItemId_GetSecondaryId(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].secondaryId;
}

u8 ItemId_GetFlingPower(u16 itemId)
{
    return gItems[SanitizeItemId(itemId)].flingPower;
}
// Item Description Header
bool8 GetSetItemObtained(u16 item, u8 caseId)
{
    u8 index;
    u8 bit;
    u8 mask;

    index = item / 8;
    bit = item % 8;
    mask = 1 << bit;
    switch (caseId)
    {
    case FLAG_GET_OBTAINED:
        return gSaveBlock2Ptr->itemFlags[index] & mask;
    case FLAG_SET_OBTAINED:
        gSaveBlock2Ptr->itemFlags[index] |= mask;
        return TRUE;
    }

    return FALSE;
}

static u8 ReformatItemDescription(u16 item, u8 *dest)
{
    u8 count = 0;
    u8 numLines = 1;
    u8 maxChars = 32;
    u8 *desc = (u8 *)gItems[item].description;

    while (*desc != EOS)
    {
        if (count >= maxChars)
        {
            while (*desc != CHAR_SPACE && *desc != CHAR_NEWLINE)
            {
                *dest = *desc;  //finish word
                dest++;
                desc++;
            }

            *dest = CHAR_NEWLINE;
            count = 0;
            numLines++;
            dest++;
            desc++;
            continue;
        }

        *dest = *desc;
        if (*desc == CHAR_NEWLINE)
        {
            *dest = CHAR_SPACE;
        }

        dest++;
        desc++;
        count++;
    }

    // finish string
    *dest = EOS;
    return numLines;
}

#define ITEM_ICON_X 26
#define ITEM_ICON_Y 24
void DrawHeaderBox(void)
{
    struct WindowTemplate template;
    u16 item = gSpecialVar_0x8006;
    u8 headerType = gSpecialVar_0x8009;
    u8 textY;
    u8 *dst;
    bool8 handleFlash = FALSE;
    
    if (GetFlashLevel() > 0 || InBattlePyramid_())
        handleFlash = TRUE;

    if (headerType == 1)
        dst = gStringVar3;
    else
        dst = gStringVar1;

    if (GetSetItemObtained(item, FLAG_GET_OBTAINED))
    {
        ShowItemIconSprite(item, FALSE, handleFlash);
        return; //no box if item obtained previously
    }

    SetWindowTemplateFields(&template, 0, 1, 1, 28, 3, 15, 8);
    sHeaderBoxWindowId = AddWindow(&template);
    FillWindowPixelBuffer(sHeaderBoxWindowId, PIXEL_FILL(0));
    PutWindowTilemap(sHeaderBoxWindowId);
    CopyWindowToVram(sHeaderBoxWindowId, 3);
    SetStandardWindowBorderStyle(sHeaderBoxWindowId, FALSE);
    DrawStdFrameWithCustomTileAndPalette(sHeaderBoxWindowId, FALSE, 0x214, 14);

    if (ReformatItemDescription(item, dst) == 1)
        textY = 4;
    else
        textY = 0;

    ShowItemIconSprite(item, TRUE, handleFlash);
    AddTextPrinterParameterized(sHeaderBoxWindowId, 0, dst, ITEM_ICON_X + 2, textY, 0, NULL);
}

void HideHeaderBox(void)
{
    DestroyItemIconSprite();

    if (!GetSetItemObtained(gSpecialVar_0x8006, FLAG_GET_OBTAINED))
    {
        //header box only exists if haven't seen item before
        GetSetItemObtained(gSpecialVar_0x8006, FLAG_SET_OBTAINED);
        ClearStdWindowAndFrameToTransparent(sHeaderBoxWindowId, FALSE);
        CopyWindowToVram(sHeaderBoxWindowId, 3);
        RemoveWindow(sHeaderBoxWindowId);
    }
}

#include "gpu_regs.h"

#define ITEM_TAG 0x2722 //same as money label
static void ShowItemIconSprite(u16 item, bool8 firstTime, bool8 flash)
{
    s16 x, y;
    u8 iconSpriteId;   
    u8 spriteId2 = MAX_SPRITES;

    if (flash)
    {
        SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_OBJWIN_ON);
        SetGpuRegBits(REG_OFFSET_WINOUT, WINOUT_WINOBJ_OBJ);
    }

    iconSpriteId = AddItemIconSprite(ITEM_TAG, ITEM_TAG, item);
    if (flash)
        spriteId2 = AddItemIconSprite(ITEM_TAG, ITEM_TAG, item);
    if (iconSpriteId != MAX_SPRITES)
    {
        if (!firstTime)
        {
            //show in message box
            x = 213;
            y = 140;
        }
        else
        {
            // show in header box
            x = ITEM_ICON_X;
            y = ITEM_ICON_Y;
        }

        gSprites[iconSpriteId].x2 = x;
        gSprites[iconSpriteId].y2 = y;
        gSprites[iconSpriteId].oam.priority = 0;
    }

    if (spriteId2 != MAX_SPRITES)
    {
        gSprites[spriteId2].x2 = x;
        gSprites[spriteId2].y2 = y;
        gSprites[spriteId2].oam.priority = 0;
        gSprites[spriteId2].oam.objMode = ST_OAM_OBJ_WINDOW;
        sItemIconSpriteId2 = spriteId2;
    }

    sItemIconSpriteId = iconSpriteId;
}

static void DestroyItemIconSprite(void)
{
    FreeSpriteTilesByTag(ITEM_TAG);
    FreeSpritePaletteByTag(ITEM_TAG);
    FreeSpriteOamMatrix(&gSprites[sItemIconSpriteId]);
    DestroySprite(&gSprites[sItemIconSpriteId]);

    if ((GetFlashLevel() > 0 || InBattlePyramid_()) && sItemIconSpriteId2 != MAX_SPRITES)
    {
        //FreeSpriteTilesByTag(ITEM_TAG);
        //FreeSpritePaletteByTag(ITEM_TAG);
        FreeSpriteOamMatrix(&gSprites[sItemIconSpriteId2]);
        DestroySprite(&gSprites[sItemIconSpriteId2]);
    }
}

//tx_randomizer_and_challenges
#define RANDOM_ITEM_COUNT ARRAY_COUNT(sRandomValidItems)
static const u16 sRandomValidItems[] =
{
#ifndef ITEM_EXPANSION
    // Balls,
    ITEM_MASTER_BALL,
    ITEM_ULTRA_BALL,
    ITEM_GREAT_BALL,
    ITEM_POKE_BALL,
    ITEM_SAFARI_BALL,
    ITEM_NET_BALL,
    ITEM_DIVE_BALL,
    ITEM_NEST_BALL,
    ITEM_REPEAT_BALL,
    ITEM_TIMER_BALL,
    ITEM_LUXURY_BALL,
    ITEM_PREMIER_BALL,
    // Pokemon Items,
    ITEM_POTION,
    ITEM_ANTIDOTE,
    ITEM_BURN_HEAL,
    ITEM_ICE_HEAL,
    ITEM_AWAKENING,
    ITEM_PARALYZE_HEAL,
    ITEM_FULL_RESTORE,
    ITEM_MAX_POTION,
    ITEM_HYPER_POTION,
    ITEM_SUPER_POTION,
    ITEM_FULL_HEAL,
    ITEM_REVIVE,
    ITEM_MAX_REVIVE,
    ITEM_FRESH_WATER,
    ITEM_SODA_POP,
    ITEM_LEMONADE,
    ITEM_MOOMOO_MILK,
    ITEM_ENERGY_POWDER,
    ITEM_ENERGY_ROOT,
    ITEM_HEAL_POWDER,
    ITEM_REVIVAL_HERB,
    ITEM_ETHER,
    ITEM_MAX_ETHER,
    ITEM_ELIXIR,
    ITEM_MAX_ELIXIR,
    ITEM_LAVA_COOKIE,
    ITEM_BLUE_FLUTE,
    ITEM_YELLOW_FLUTE,
    ITEM_RED_FLUTE,
    ITEM_BLACK_FLUTE,
    ITEM_WHITE_FLUTE,
    ITEM_BERRY_JUICE,
    ITEM_SACRED_ASH,
    ITEM_SHOAL_SALT,
    ITEM_SHOAL_SHELL,
    ITEM_RED_SHARD,
    ITEM_BLUE_SHARD,
    ITEM_YELLOW_SHARD,
    ITEM_GREEN_SHARD,
    ITEM_HP_UP,
    ITEM_PROTEIN,
    ITEM_IRON,
    ITEM_CARBOS,
    ITEM_CALCIUM,
    ITEM_RARE_CANDY,
    ITEM_PP_UP,
    ITEM_ZINC,
    ITEM_PP_MAX,
    ITEM_GUARD_SPEC,
    ITEM_DIRE_HIT,
    ITEM_X_ATTACK,
    ITEM_X_DEFEND,
    ITEM_X_SPEED,
    ITEM_X_ACCURACY,
    ITEM_X_SPECIAL,
    ITEM_POKE_DOLL,
    ITEM_FLUFFY_TAIL,
    ITEM_SUPER_REPEL,
    ITEM_MAX_REPEL,
    ITEM_ESCAPE_ROPE,
    ITEM_REPEL,
    ITEM_SUN_STONE,
    ITEM_MOON_STONE,
    ITEM_FIRE_STONE,
    ITEM_THUNDER_STONE,
    ITEM_WATER_STONE,
    ITEM_LEAF_STONE,
    // Mails,
    ITEM_ORANGE_MAIL,
    ITEM_HARBOR_MAIL,
    ITEM_GLITTER_MAIL,
    ITEM_MECH_MAIL,
    ITEM_WOOD_MAIL,
    ITEM_WAVE_MAIL,
    ITEM_BEAD_MAIL,
    ITEM_SHADOW_MAIL,
    ITEM_TROPIC_MAIL,
    ITEM_DREAM_MAIL,
    ITEM_FAB_MAIL,
    ITEM_RETRO_MAIL,
    // Berries,
    ITEM_CHERI_BERRY,
    ITEM_CHESTO_BERRY,
    ITEM_PECHA_BERRY,
    ITEM_RAWST_BERRY,
    ITEM_ASPEAR_BERRY,
    ITEM_LEPPA_BERRY,
    ITEM_ORAN_BERRY,
    ITEM_PERSIM_BERRY,
    ITEM_LUM_BERRY,
    ITEM_SITRUS_BERRY,
    ITEM_FIGY_BERRY,
    ITEM_WIKI_BERRY,
    ITEM_MAGO_BERRY,
    ITEM_AGUAV_BERRY,
    ITEM_IAPAPA_BERRY,
    ITEM_RAZZ_BERRY,
    ITEM_BLUK_BERRY,
    ITEM_NANAB_BERRY,
    ITEM_WEPEAR_BERRY,
    ITEM_PINAP_BERRY,
    ITEM_POMEG_BERRY,
    ITEM_KELPSY_BERRY,
    ITEM_QUALOT_BERRY,
    ITEM_HONDEW_BERRY,
    ITEM_GREPA_BERRY,
    ITEM_TAMATO_BERRY,
    ITEM_CORNN_BERRY,
    ITEM_MAGOST_BERRY,
    ITEM_RABUTA_BERRY,
    ITEM_NOMEL_BERRY,
    ITEM_SPELON_BERRY,
    ITEM_PAMTRE_BERRY,
    ITEM_WATMEL_BERRY,
    ITEM_DURIN_BERRY,
    ITEM_BELUE_BERRY,
    ITEM_LIECHI_BERRY,
    ITEM_GANLON_BERRY,
    ITEM_SALAC_BERRY,
    ITEM_PETAYA_BERRY,
    ITEM_APICOT_BERRY,
    ITEM_LANSAT_BERRY,
    ITEM_STARF_BERRY,
    ITEM_ENIGMA_BERRY,
    // Battle Held items,
    ITEM_BRIGHT_POWDER,
    ITEM_WHITE_HERB,
    ITEM_MACHO_BRACE,
    ITEM_EXP_SHARE,
    ITEM_QUICK_CLAW,
    ITEM_SOOTHE_BELL,
    ITEM_MENTAL_HERB,
    ITEM_CHOICE_BAND,
    ITEM_KINGS_ROCK,
    ITEM_SILVER_POWDER,
    ITEM_AMULET_COIN,
    ITEM_CLEANSE_TAG,
    ITEM_SOUL_DEW,
    ITEM_DEEP_SEA_TOOTH,
    ITEM_DEEP_SEA_SCALE,
    ITEM_SMOKE_BALL,
    ITEM_EVERSTONE,
    ITEM_FOCUS_BAND,
    ITEM_LUCKY_EGG,
    ITEM_SCOPE_LENS,
    ITEM_METAL_COAT,
    ITEM_LEFTOVERS,
    ITEM_DRAGON_SCALE,
    ITEM_LIGHT_BALL,
    ITEM_SOFT_SAND,
    ITEM_HARD_STONE,
    ITEM_MIRACLE_SEED,
    ITEM_BLACK_GLASSES,
    ITEM_BLACK_BELT,
    ITEM_MAGNET,
    ITEM_MYSTIC_WATER,
    ITEM_SHARP_BEAK,
    ITEM_POISON_BARB,
    ITEM_NEVER_MELT_ICE,
    ITEM_SPELL_TAG,
    ITEM_TWISTED_SPOON,
    ITEM_CHARCOAL,
    ITEM_DRAGON_FANG,
    ITEM_SILK_SCARF,
    ITEM_UP_GRADE,
    ITEM_SHELL_BELL,
    ITEM_SEA_INCENSE,
    ITEM_LAX_INCENSE,
    ITEM_LUCKY_PUNCH,
    ITEM_METAL_POWDER,
    ITEM_THICK_CLUB,
    ITEM_STICK,
    // Contest held items,
    ITEM_RED_SCARF,
    ITEM_BLUE_SCARF,
    ITEM_PINK_SCARF,
    ITEM_GREEN_SCARF,
    ITEM_YELLOW_SCARF,
#else
    // Poké Balls
    ITEM_POKE_BALL,
    ITEM_GREAT_BALL,
    ITEM_ULTRA_BALL,
    ITEM_MASTER_BALL,
    ITEM_PREMIER_BALL,
    ITEM_HEAL_BALL,
    ITEM_NET_BALL,
    ITEM_NEST_BALL,
    ITEM_DIVE_BALL,
    ITEM_DUSK_BALL,
    ITEM_TIMER_BALL,
    ITEM_QUICK_BALL,
    ITEM_REPEAT_BALL,
    ITEM_LUXURY_BALL,
    ITEM_LEVEL_BALL,
    ITEM_LURE_BALL,
    ITEM_MOON_BALL,
    ITEM_FRIEND_BALL,
    ITEM_LOVE_BALL,
    ITEM_FAST_BALL,
    ITEM_HEAVY_BALL,
    ITEM_DREAM_BALL,
    ITEM_SAFARI_BALL,
    //ITEM_SPORT_BALL,
    //ITEM_PARK_BALL,
    ITEM_BEAST_BALL,
    ITEM_CHERISH_BALL,
    // Medicine
    ITEM_POTION,
    ITEM_SUPER_POTION,
    ITEM_HYPER_POTION,
    ITEM_MAX_POTION,
    ITEM_FULL_RESTORE,
    ITEM_REVIVE,
    ITEM_MAX_REVIVE,
    ITEM_FRESH_WATER,
    ITEM_SODA_POP,
    ITEM_LEMONADE,
    ITEM_MOOMOO_MILK,
    ITEM_ENERGY_POWDER,
    ITEM_ENERGY_ROOT,
    ITEM_HEAL_POWDER,
    ITEM_REVIVAL_HERB,
    ITEM_ANTIDOTE,
    ITEM_PARALYZE_HEAL,
    ITEM_BURN_HEAL,
    ITEM_ICE_HEAL,
    ITEM_AWAKENING,
    ITEM_FULL_HEAL,
    ITEM_ETHER,
    ITEM_MAX_ETHER,
    ITEM_ELIXIR,
    ITEM_MAX_ELIXIR,
    ITEM_BERRY_JUICE,
    ITEM_SACRED_ASH,
    ITEM_SWEET_HEART,
    ITEM_MAX_HONEY,
    // Regional Specialties
    ITEM_PEWTER_CRUNCHIES,
    ITEM_RAGE_CANDY_BAR,
    ITEM_LAVA_COOKIE,
    ITEM_OLD_GATEAU,
    ITEM_CASTELIACONE,
    ITEM_LUMIOSE_GALETTE,
    ITEM_SHALOUR_SABLE,
    ITEM_BIG_MALASADA,
    // Vitamins
    ITEM_HP_UP,
    ITEM_PROTEIN,
    ITEM_IRON,
    ITEM_CALCIUM,
    ITEM_ZINC,
    ITEM_CARBOS,
    ITEM_PP_UP,
    ITEM_PP_MAX,
    // EV Feathers
    ITEM_HEALTH_FEATHER,
    ITEM_MUSCLE_FEATHER,
    ITEM_RESIST_FEATHER,
    ITEM_GENIUS_FEATHER,
    ITEM_CLEVER_FEATHER,
    ITEM_SWIFT_FEATHER,
    // Ability Modifiers
    ITEM_ABILITY_CAPSULE,
    ITEM_ABILITY_PATCH,
    // Mints
    //ITEM_LONELY_MINT,
    //ITEM_ADAMANT_MINT,
    //ITEM_NAUGHTY_MINT,
    //ITEM_BRAVE_MINT,
    //ITEM_BOLD_MINT,
    //ITEM_IMPISH_MINT,
    //ITEM_LAX_MINT,
    //ITEM_RELAXED_MINT,
    //ITEM_MODEST_MINT,
    //ITEM_MILD_MINT,
    //ITEM_RASH_MINT,
    //ITEM_QUIET_MINT,
    //ITEM_CALM_MINT,
    //ITEM_GENTLE_MINT,
    //ITEM_CAREFUL_MINT,
    //ITEM_SASSY_MINT,
    //ITEM_TIMID_MINT,
    //ITEM_HASTY_MINT,
    //ITEM_JOLLY_MINT,
    //ITEM_NAIVE_MINT,
    //ITEM_SERIOUS_MINT,
    // Candy
    ITEM_RARE_CANDY,
    //ITEM_EXP_CANDY_XS,
    //ITEM_EXP_CANDY_S,
    //ITEM_EXP_CANDY_M,
    //ITEM_EXP_CANDY_L,
    //ITEM_EXP_CANDY_XL,
    //ITEM_DYNAMAX_CANDY,
    // Medicinal Flutes
    ITEM_BLUE_FLUTE,
    ITEM_YELLOW_FLUTE,
    ITEM_RED_FLUTE,
    // Encounter-modifying Flutes
    ITEM_BLACK_FLUTE,
    ITEM_WHITE_FLUTE,
    // Encounter Modifiers
    ITEM_REPEL,
    ITEM_SUPER_REPEL,
    ITEM_MAX_REPEL,
    //ITEM_LURE,
    //ITEM_SUPER_LURE,
    //ITEM_MAX_LURE,
    ITEM_ESCAPE_ROPE,
    // X Items
    ITEM_X_ATTACK,
    ITEM_X_DEFENSE,
    ITEM_X_SP_ATK,
    ITEM_X_SP_DEF,
    ITEM_X_SPEED,
    ITEM_X_ACCURACY,
    ITEM_DIRE_HIT,
    ITEM_GUARD_SPEC,
    // Escape Items
    ITEM_POKE_DOLL,
    ITEM_FLUFFY_TAIL,
    ITEM_POKE_TOY,
    //ITEM_MAX_MUSHROOMS,
    // Treasures
    //ITEM_BOTTLE_CAP,
    //ITEM_GOLD_BOTTLE_CAP,
    ITEM_NUGGET,
    ITEM_BIG_NUGGET,
    ITEM_TINY_MUSHROOM,
    ITEM_BIG_MUSHROOM,
    ITEM_BALM_MUSHROOM,
    ITEM_PEARL,
    ITEM_BIG_PEARL,
    ITEM_PEARL_STRING,
    ITEM_STARDUST,
    ITEM_STAR_PIECE,
    ITEM_COMET_SHARD,
    ITEM_SHOAL_SALT,
    ITEM_SHOAL_SHELL,
    ITEM_RED_SHARD,
    ITEM_BLUE_SHARD,
    ITEM_YELLOW_SHARD,
    ITEM_GREEN_SHARD,
    ITEM_HEART_SCALE,
    ITEM_HONEY,
    ITEM_RARE_BONE,
    ITEM_ODD_KEYSTONE,
    ITEM_PRETTY_FEATHER,
    //ITEM_RELIC_COPPER,
    //ITEM_RELIC_SILVER,
    //ITEM_RELIC_GOLD,
    //ITEM_RELIC_VASE,
    //ITEM_RELIC_BAND,
    //ITEM_RELIC_STATUE,
    //ITEM_RELIC_CROWN,
    ITEM_STRANGE_SOUVENIR,
    // Fossils
    //ITEM_HELIX_FOSSIL,
    //ITEM_DOME_FOSSIL,
    //ITEM_OLD_AMBER,
    ITEM_ROOT_FOSSIL,
    ITEM_CLAW_FOSSIL,
    //ITEM_ARMOR_FOSSIL,
    //ITEM_SKULL_FOSSIL,
    //ITEM_COVER_FOSSIL,
    //ITEM_PLUME_FOSSIL,
    //ITEM_JAW_FOSSIL,
    //ITEM_SAIL_FOSSIL,
    //ITEM_FOSSILIZED_BIRD,
    //ITEM_FOSSILIZED_FISH,
    //ITEM_FOSSILIZED_DRAKE,
    //ITEM_FOSSILIZED_DINO,
    // Mulch
    //ITEM_GROWTH_MULCH,
    //ITEM_DAMP_MULCH,
    //ITEM_STABLE_MULCH,
    //ITEM_GOOEY_MULCH,
    //ITEM_RICH_MULCH,
    //ITEM_SURPRISE_MULCH,
    //ITEM_BOOST_MULCH,
    //ITEM_AMAZE_MULCH,
    // Apricorns
    //ITEM_RED_APRICORN,
    //ITEM_BLUE_APRICORN,
    //ITEM_YELLOW_APRICORN,
    //ITEM_GREEN_APRICORN,
    //ITEM_PINK_APRICORN,
    //ITEM_WHITE_APRICORN,
    //ITEM_BLACK_APRICORN,
    //ITEM_WISHING_PIECE,
    //ITEM_GALARICA_TWIG,
    //ITEM_ARMORITE_ORE,
    //ITEM_DYNITE_ORE,
    // Mail
    ITEM_ORANGE_MAIL,
    ITEM_HARBOR_MAIL,
    ITEM_GLITTER_MAIL,
    ITEM_MECH_MAIL,
    ITEM_WOOD_MAIL,
    ITEM_WAVE_MAIL,
    ITEM_BEAD_MAIL,
    ITEM_SHADOW_MAIL,
    ITEM_TROPIC_MAIL,
    ITEM_DREAM_MAIL,
    ITEM_FAB_MAIL,
    ITEM_RETRO_MAIL,
    // Evolution Items
    ITEM_FIRE_STONE,
    ITEM_WATER_STONE,
    ITEM_THUNDER_STONE,
    ITEM_LEAF_STONE,
    ITEM_ICE_STONE,
    ITEM_SUN_STONE,
    ITEM_MOON_STONE,
    #ifdef POKEMON_EXPANSION
    ITEM_SHINY_STONE,
    ITEM_DUSK_STONE,
    ITEM_DAWN_STONE,
    ITEM_SWEET_APPLE,
    ITEM_TART_APPLE,
    ITEM_CRACKED_POT,
    ITEM_CHIPPED_POT,
    ITEM_GALARICA_CUFF,
    ITEM_GALARICA_WREATH,
    #endif
    ITEM_DRAGON_SCALE,
    ITEM_UPGRADE,
    #ifdef POKEMON_EXPANSION
    ITEM_PROTECTOR,
    ITEM_ELECTIRIZER,
    ITEM_MAGMARIZER,
    ITEM_DUBIOUS_DISC,
    ITEM_REAPER_CLOTH,
    ITEM_PRISM_SCALE,
    ITEM_WHIPPED_DREAM,
    ITEM_SACHET,
    ITEM_OVAL_STONE,
    ITEM_STRAWBERRY_SWEET,
    ITEM_LOVE_SWEET,
    ITEM_BERRY_SWEET,
    ITEM_CLOVER_SWEET,
    ITEM_FLOWER_SWEET,
    ITEM_STAR_SWEET,
    ITEM_RIBBON_SWEET,
    #endif
    ITEM_EVERSTONE,
    // Nectars
    #ifdef POKEMON_EXPANSION
    ITEM_RED_NECTAR,
    ITEM_YELLOW_NECTAR,
    ITEM_PINK_NECTAR,
    ITEM_PURPLE_NECTAR,
    // Plates
    ITEM_FLAME_PLATE,
    ITEM_SPLASH_PLATE,
    ITEM_ZAP_PLATE,
    ITEM_MEADOW_PLATE,
    ITEM_ICICLE_PLATE,
    ITEM_FIST_PLATE,
    ITEM_TOXIC_PLATE,
    ITEM_EARTH_PLATE,
    ITEM_SKY_PLATE,
    ITEM_MIND_PLATE,
    ITEM_INSECT_PLATE,
    ITEM_STONE_PLATE,
    ITEM_SPOOKY_PLATE,
    ITEM_DRACO_PLATE,
    ITEM_DREAD_PLATE,
    ITEM_IRON_PLATE,
    ITEM_PIXIE_PLATE,
    // Drives
    ITEM_DOUSE_DRIVE,
    ITEM_SHOCK_DRIVE,
    ITEM_BURN_DRIVE,
    ITEM_CHILL_DRIVE,
    // Memories
    ITEM_FIRE_MEMORY,
    ITEM_WATER_MEMORY,
    ITEM_ELECTRIC_MEMORY,
    ITEM_GRASS_MEMORY,
    ITEM_ICE_MEMORY,
    ITEM_FIGHTING_MEMORY,
    ITEM_POISON_MEMORY,
    ITEM_GROUND_MEMORY,
    ITEM_FLYING_MEMORY,
    ITEM_PSYCHIC_MEMORY,
    ITEM_BUG_MEMORY,
    ITEM_ROCK_MEMORY,
    ITEM_GHOST_MEMORY,
    ITEM_DRAGON_MEMORY,
    ITEM_DARK_MEMORY,
    ITEM_STEEL_MEMORY,
    ITEM_FAIRY_MEMORY,
    ITEM_RUSTED_SWORD,
    ITEM_RUSTED_SHIELD,
    #endif
    // Colored Orbs
    ITEM_RED_ORB,
    ITEM_BLUE_ORB,
    #if defined(BATTLE_ENGINE) && defined(POKEMON_EXPANSION)
    // Mega Stones
    ITEM_VENUSAURITE,
    ITEM_CHARIZARDITE_X,
    ITEM_CHARIZARDITE_Y,
    ITEM_BLASTOISINITE,
    ITEM_BEEDRILLITE,
    ITEM_PIDGEOTITE,
    ITEM_ALAKAZITE,
    ITEM_SLOWBRONITE,
    ITEM_GENGARITE,
    ITEM_KANGASKHANITE,
    ITEM_PINSIRITE,
    ITEM_GYARADOSITE,
    ITEM_AERODACTYLITE,
    ITEM_MEWTWONITE_X,
    ITEM_MEWTWONITE_Y,
    ITEM_AMPHAROSITE,
    ITEM_STEELIXITE,
    ITEM_SCIZORITE,
    ITEM_HERACRONITE,
    ITEM_HOUNDOOMINITE,
    ITEM_TYRANITARITE,
    ITEM_SCEPTILITE,
    ITEM_BLAZIKENITE,
    ITEM_SWAMPERTITE,
    ITEM_GARDEVOIRITE,
    ITEM_SABLENITE,
    ITEM_MAWILITE,
    ITEM_AGGRONITE,
    ITEM_MEDICHAMITE,
    ITEM_MANECTITE,
    ITEM_SHARPEDONITE,
    ITEM_CAMERUPTITE,
    ITEM_ALTARIANITE,
    ITEM_BANETTITE,
    ITEM_ABSOLITE,
    ITEM_GLALITITE,
    ITEM_SALAMENCITE,
    ITEM_METAGROSSITE,
    ITEM_LATIASITE,
    ITEM_LATIOSITE,
    ITEM_LOPUNNITE,
    ITEM_GARCHOMPITE,
    ITEM_LUCARIONITE,
    ITEM_ABOMASITE,
    ITEM_GALLADITE,
    ITEM_AUDINITE,
    ITEM_DIANCITE,
    #endif
    // Gems
    ITEM_NORMAL_GEM,
    ITEM_FIRE_GEM,
    ITEM_WATER_GEM,
    ITEM_ELECTRIC_GEM,
    ITEM_GRASS_GEM,
    ITEM_ICE_GEM,
    ITEM_FIGHTING_GEM,
    ITEM_POISON_GEM,
    ITEM_GROUND_GEM,
    ITEM_FLYING_GEM,
    ITEM_PSYCHIC_GEM,
    ITEM_BUG_GEM,
    ITEM_ROCK_GEM,
    ITEM_GHOST_GEM,
    ITEM_DRAGON_GEM,
    ITEM_DARK_GEM,
    ITEM_STEEL_GEM,
    ITEM_FAIRY_GEM,
    #ifdef BATTLE_ENGINE
    // Z-Crystals
    //ITEM_NORMALIUM_Z,
    //ITEM_FIRIUM_Z,
    //ITEM_WATERIUM_Z,
    //ITEM_ELECTRIUM_Z,
    //ITEM_GRASSIUM_Z,
    //ITEM_ICIUM_Z,
    //ITEM_FIGHTINIUM_Z,
    //ITEM_POISONIUM_Z,
    //ITEM_GROUNDIUM_Z,
    //ITEM_FLYINIUM_Z,
    //ITEM_PSYCHIUM_Z,
    //ITEM_BUGINIUM_Z,
    //ITEM_ROCKIUM_Z,
    //ITEM_GHOSTIUM_Z,
    //ITEM_DRAGONIUM_Z,
    //ITEM_DARKINIUM_Z,
    //ITEM_STEELIUM_Z,
    //ITEM_FAIRIUM_Z,
    //ITEM_PIKANIUM_Z,
    //ITEM_EEVIUM_Z,
    //ITEM_SNORLIUM_Z,
    //ITEM_MEWNIUM_Z,
    //ITEM_DECIDIUM_Z,
    //ITEM_INCINIUM_Z,
    //ITEM_PRIMARIUM_Z,
    //ITEM_LYCANIUM_Z,
    //ITEM_MIMIKIUM_Z,
    //ITEM_KOMMONIUM_Z,
    //ITEM_TAPUNIUM_Z,
    //ITEM_SOLGANIUM_Z,
    //ITEM_LUNALIUM_Z,
    //ITEM_MARSHADIUM_Z,
    //ITEM_ALORAICHIUM_Z,
    //ITEM_PIKASHUNIUM_Z,
    //ITEM_ULTRANECROZIUM_Z,
    #endif
    // Species-specific Held Items
    ITEM_LIGHT_BALL,
    ITEM_LEEK,
    ITEM_THICK_CLUB,
    ITEM_LUCKY_PUNCH,
    ITEM_METAL_POWDER,
    ITEM_QUICK_POWDER,
    ITEM_DEEP_SEA_SCALE,
    ITEM_DEEP_SEA_TOOTH,
    ITEM_SOUL_DEW,
    #if defined(BATTLE_ENGINE) && defined(POKEMON_EXPANSION)
    ITEM_ADAMANT_ORB,
    ITEM_LUSTROUS_ORB,
    ITEM_GRISEOUS_ORB,
    #endif
    // Incenses
    ITEM_SEA_INCENSE,
    ITEM_LAX_INCENSE,
    #ifdef POKEMON_EXPANSION
    ITEM_ODD_INCENSE,
    ITEM_ROCK_INCENSE,
    ITEM_FULL_INCENSE,
    ITEM_WAVE_INCENSE,
    ITEM_ROSE_INCENSE,
    ITEM_LUCK_INCENSE,
    ITEM_PURE_INCENSE,
    #endif
    // Contest Scarves
    ITEM_RED_SCARF,
    ITEM_BLUE_SCARF,
    ITEM_PINK_SCARF,
    ITEM_GREEN_SCARF,
    ITEM_YELLOW_SCARF,
    // EV Gain Modifiers
    ITEM_MACHO_BRACE,
    ITEM_POWER_WEIGHT,
    ITEM_POWER_BRACER,
    ITEM_POWER_BELT,
    ITEM_POWER_LENS,
    ITEM_POWER_BAND,
    ITEM_POWER_ANKLET,
    // Type-boosting Held Items
    ITEM_SILK_SCARF,
    ITEM_CHARCOAL,
    ITEM_MYSTIC_WATER,
    ITEM_MAGNET,
    ITEM_MIRACLE_SEED,
    ITEM_NEVER_MELT_ICE,
    ITEM_BLACK_BELT,
    ITEM_POISON_BARB,
    ITEM_SOFT_SAND,
    ITEM_SHARP_BEAK,
    ITEM_TWISTED_SPOON,
    ITEM_SILVER_POWDER,
    ITEM_HARD_STONE,
    ITEM_SPELL_TAG,
    ITEM_DRAGON_FANG,
    ITEM_BLACK_GLASSES,
    ITEM_METAL_COAT,
    #ifdef BATTLE_ENGINE
    // Choice Items
    ITEM_CHOICE_BAND,
    ITEM_CHOICE_SPECS,
    ITEM_CHOICE_SCARF,
    // Status Orbs
    ITEM_FLAME_ORB,
    ITEM_TOXIC_ORB,
    // Weather Rocks
    ITEM_DAMP_ROCK,
    ITEM_HEAT_ROCK,
    ITEM_SMOOTH_ROCK,
    ITEM_ICY_ROCK,
    // Terrain Seeds
    ITEM_ELECTRIC_SEED,
    ITEM_PSYCHIC_SEED,
    ITEM_MISTY_SEED,
    ITEM_GRASSY_SEED,
    // Type-activated Stat Modifiers
    ITEM_ABSORB_BULB,
    ITEM_CELL_BATTERY,
    ITEM_LUMINOUS_MOSS,
    ITEM_SNOWBALL,
    #endif
    // Misc. Held Items
    ITEM_BRIGHT_POWDER,
    ITEM_WHITE_HERB,
    ITEM_EXP_SHARE,
    ITEM_QUICK_CLAW,
    ITEM_SOOTHE_BELL,
    ITEM_MENTAL_HERB,
    ITEM_KINGS_ROCK,
    ITEM_AMULET_COIN,
    ITEM_CLEANSE_TAG,
    ITEM_SMOKE_BALL,
    ITEM_FOCUS_BAND,
    ITEM_LUCKY_EGG,
    ITEM_SCOPE_LENS,
    ITEM_LEFTOVERS,
    ITEM_SHELL_BELL,
    #ifdef BATTLE_ENGINE
    ITEM_WIDE_LENS,
    ITEM_MUSCLE_BAND,
    ITEM_WISE_GLASSES,
    ITEM_EXPERT_BELT,
    ITEM_LIGHT_CLAY,
    ITEM_LIFE_ORB,
    ITEM_POWER_HERB,
    ITEM_FOCUS_SASH,
    ITEM_ZOOM_LENS,
    ITEM_METRONOME,
    ITEM_IRON_BALL,
    ITEM_LAGGING_TAIL,
    ITEM_DESTINY_KNOT,
    ITEM_BLACK_SLUDGE,
    ITEM_GRIP_CLAW,
    ITEM_STICKY_BARB,
    ITEM_SHED_SHELL,
    ITEM_BIG_ROOT,
    ITEM_RAZOR_CLAW,
    ITEM_RAZOR_FANG,
    ITEM_EVIOLITE,
    ITEM_FLOAT_STONE,
    ITEM_ROCKY_HELMET,
    ITEM_AIR_BALLOON,
    ITEM_RED_CARD,
    ITEM_RING_TARGET,
    ITEM_BINDING_BAND,
    ITEM_EJECT_BUTTON,
    ITEM_WEAKNESS_POLICY,
    ITEM_ASSAULT_VEST,
    ITEM_SAFETY_GOGGLES,
    ITEM_ADRENALINE_ORB,
    ITEM_TERRAIN_EXTENDER,
    ITEM_PROTECTIVE_PADS,
    ITEM_THROAT_SPRAY,
    ITEM_EJECT_PACK,
    ITEM_HEAVY_DUTY_BOOTS,
    ITEM_BLUNDER_POLICY,
    ITEM_ROOM_SERVICE,
    ITEM_UTILITY_UMBRELLA,
    #endif
    // Berries
    ITEM_CHERI_BERRY,
    ITEM_CHESTO_BERRY,
    ITEM_PECHA_BERRY,
    ITEM_RAWST_BERRY,
    ITEM_ASPEAR_BERRY,
    ITEM_LEPPA_BERRY,
    ITEM_ORAN_BERRY,
    ITEM_PERSIM_BERRY,
    ITEM_LUM_BERRY,
    ITEM_SITRUS_BERRY,
    ITEM_FIGY_BERRY,
    ITEM_WIKI_BERRY,
    ITEM_MAGO_BERRY,
    ITEM_AGUAV_BERRY,
    ITEM_IAPAPA_BERRY,
    ITEM_RAZZ_BERRY,
    ITEM_BLUK_BERRY,
    ITEM_NANAB_BERRY,
    ITEM_WEPEAR_BERRY,
    ITEM_PINAP_BERRY,
    ITEM_POMEG_BERRY,
    ITEM_KELPSY_BERRY,
    ITEM_QUALOT_BERRY,
    ITEM_HONDEW_BERRY,
    ITEM_GREPA_BERRY,
    ITEM_TAMATO_BERRY,
    ITEM_CORNN_BERRY,
    ITEM_MAGOST_BERRY,
    ITEM_RABUTA_BERRY,
    ITEM_NOMEL_BERRY,
    ITEM_SPELON_BERRY,
    ITEM_PAMTRE_BERRY,
    ITEM_WATMEL_BERRY,
    ITEM_DURIN_BERRY,
    ITEM_BELUE_BERRY,
    ITEM_CHILAN_BERRY,
    ITEM_OCCA_BERRY,
    ITEM_PASSHO_BERRY,
    ITEM_WACAN_BERRY,
    ITEM_RINDO_BERRY,
    ITEM_YACHE_BERRY,
    ITEM_CHOPLE_BERRY,
    ITEM_KEBIA_BERRY,
    ITEM_SHUCA_BERRY,
    ITEM_COBA_BERRY,
    ITEM_PAYAPA_BERRY,
    ITEM_TANGA_BERRY,
    ITEM_CHARTI_BERRY,
    ITEM_KASIB_BERRY,
    ITEM_HABAN_BERRY,
    ITEM_COLBUR_BERRY,
    ITEM_BABIRI_BERRY,
    ITEM_ROSELI_BERRY,
    ITEM_LIECHI_BERRY,
    ITEM_GANLON_BERRY,
    ITEM_SALAC_BERRY,
    ITEM_PETAYA_BERRY,
    ITEM_APICOT_BERRY,
    ITEM_LANSAT_BERRY,
    ITEM_STARF_BERRY,
    ITEM_ENIGMA_BERRY,
    ITEM_MICLE_BERRY,
    ITEM_CUSTAP_BERRY,
    ITEM_JABOCA_BERRY,
    ITEM_ROWAP_BERRY,
    ITEM_KEE_BERRY,
    ITEM_MARANGA_BERRY,
#endif
};

u16 RandomItemId(u16 itemId)
{
    u8 mapId = NuzlockeGetCurrentRegionMapSectionId();
    if (ItemId_GetPocket(itemId) == POCKET_TM_HM)
    {
        if (itemId != ITEM_HM01
            && itemId != ITEM_HM02
            && itemId != ITEM_HM03
            && itemId != ITEM_HM04
            && itemId != ITEM_HM05
            && itemId != ITEM_HM06
            && itemId != ITEM_HM07
            && itemId != ITEM_HM08)
        {
            u8 i;
            itemId = ITEM_TM01 + RandomSeededModulo(itemId, 50);
            for (i = 0; i < 255; i++)
            {
                if (!CheckBagHasItem(itemId, 1))
                    break;
                itemId = ITEM_TM01 + RandomSeededModulo(itemId, 50);
            }
        }
    }
    else if (ItemId_GetPocket(itemId) != POCKET_KEY_ITEMS)
        itemId = sRandomValidItems[RandomSeededModulo(itemId + mapId, RANDOM_ITEM_COUNT)];
    
    return itemId;
}

u16 RandomItem(void)
{
    u16 itemId = RandomItemId(gSpecialVar_0x8000);

    gSpecialVar_0x8000 = itemId;
    return itemId;
}

u16 RandomItemHidden(void) //same as normal hidden item, but differen special var
{
    u16 itemId = RandomItemId(gSpecialVar_0x8005);

    gSpecialVar_0x8005 = itemId;
    return itemId;
}


