// Modern item menu (option ModernItemMenu): the START menu drawn in HD over
// the game in two pages, Items (the item grid) and Quest (equipment, pendants
// and crystals), with the name of what the cursor is on. The game logic of
// the menu stays the original one (hud.c); only the drawing is new and the
// original menu (BG3) is not shown.
#ifndef ZELDA3_HD_INV_H_
#define ZELDA3_HD_INV_H_

#include "types.h"

typedef struct Ppu Ppu;

// Option ModernItemMenu on (it brings the extended inventory with it)
bool HdInv_Option(void);
// Option on and START menu open during the game (also while it opens/closes)
bool HdInv_Enabled(void);
// Pixels of BG3VOFS per frame while the menu opens and closes (the scroll of
// the original menu only times the opening)
int HdInv_MenuStep(void);
// From Hud_NormalMenu: page change and the Quest page; true = keys used
bool HdInv_HandleInput(void);
// Around the PPU drawing: the original menu (BG3) is hidden
void HdInv_BeforeRender(Ppu *ppu);
void HdInv_AfterRender(Ppu *ppu);
// Draws the menu in the frame (at the SNES resolution times scale)
void HdInv_Draw(uint8 *pixels, int pitch, int width, int height, int scale);

#endif  // ZELDA3_HD_INV_H_
