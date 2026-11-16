#pragma once
#include "types.h"

enum kHudItems {

  kHudItem_Bombs = 4,
  kHudItem_Mushroom = 5,
  kHudItem_Hammer = 12,
  kHudItem_Flute = 13,
  kHudItem_BookMudora = 15,
  kHudItem_BottleOld = 16,
  
  kHudItem_Shovel = 16,
  kHudItem_Bottle1 = 21,
  kHudItem_Bottle2 = 22,
  kHudItem_Bottle3 = 23,
  kHudItem_Bottle4 = 24,
};

void Hud_RefreshIcon();
// Inventario esteso acceso o spento: converte gli oggetti dei tasti
void Hud_ChangeInventoryStyle(bool extended);
// true con l'inventario esteso a 6 colonne
bool Hud_ExtendedInventory();
uint8 CheckPalaceItemPosession();
void Hud_FloorIndicator();
void Hud_RemoveSuperBombIndicator();
void Hud_SuperBombIndicator();
void Hud_RefillLogic();
void Hud_Module_Run();
void Hud_ClearTileMap();
void Hud_Init();
void Hud_BringMenuDown();
void Hud_ChooseNextMode();
void Hud_NormalMenu();
void Hud_UpdateHud();
uint8 Hud_LookupInventoryItem(uint8 item);
void Hud_UpdateEquippedItem();
void Hud_CloseMenu();
void Hud_GotoBottleMenu();
void Hud_InitBottleMenu();
void Hud_ExpandBottleMenu();
void Hud_BottleMenu();
void Hud_DrawBottleMenu_Update();
void Hud_EraseBottleMenu();
void Hud_RestoreNormalMenu();
void Hud_SearchForEquippedItem();
void Hud_DrawYButtonItems();
void Hud_DrawAbilityBox();
void Hud_DrawProgressIcons();
void Hud_DrawProgressIcons_Pendants();
void Hud_DrawProgressIcons_Crystals();
void Hud_DrawSelectedYButtonItem();
void Hud_DrawEquipmentBox();
void Hud_DrawBottleMenu();
bool Hud_RefillHealth();
void Hud_AnimateHeartRefill();
bool Hud_RefillMagicPower();
void Hud_RestoreTorchBackground();
void Hud_RebuildIndoor();
void Hud_Rebuild();
const uint16 *Hud_GetItemBoxPtr(int item);
int GetCurrentItemButtonIndex();
bool Hud_ItemOnX(void);
bool Hud_ItemOnLR(void);
uint8 *GetCurrentItemButtonPtr(int i);

void Hud_HandleItemSwitchInputs();

// For the modern item menu (hd_inv.c): columns of the item grid (6 with the
// extended inventory, 5 in the original), item in a slot and slot of an item
const uint16 *Hud_GetItemIconTiles(int item);
bool Hud_HaveItem(int item);
int Hud_MenuColumns(void);
int Hud_MenuSlotCount(void);
int Hud_MenuItemAt(int slot);
int Hud_MenuSlotOf(int item);
// Icon tiles of a bottle content (link_bottle_info value)
const uint16 *Hud_GetBottleIconTiles(int contents);
// Puts item on button i (0 Y, 1 X, 2 L, 3 R), swapping with the button it
// was on; false if nothing changed
bool Hud_AssignItem(int i, int item);
// Quest page: icon tiles of what Link has, NULL when he does not have it
enum {
  kHudQuest_Sword, kHudQuest_Shield, kHudQuest_Armor,
  kHudQuest_Boots, kHudQuest_Gloves, kHudQuest_Flippers,
  kHudQuest_MoonPearl, kHudQuest_HeartPieces,
  kHudQuest_Map, kHudQuest_Compass, kHudQuest_BigKey,
  kHudQuest_Pendant0, kHudQuest_Pendant1, kHudQuest_Pendant2,
  kHudQuest_Crystal0,  // + 0..6, the bits of link_has_crystals
};
const uint16 *Hud_GetQuestIconTiles(int what);
