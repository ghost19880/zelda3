// Modern item menu (option ModernItemMenu), see hd_inv.h.
//
// Two pages over the darkened game: Items (the grid of the extended
// inventory, with the four bottles always shown in one column) and Quest
// (equipment, the dungeon items while Link is in a dungeon, pendants and
// crystals). Light stone panel with a thin border, rounded slots, the icons
// of the game (decoded from the HUD tiles in VRAM, as the modern HUD does)
// and the name of what the cursor is on, in capitals, centred, in one size
// for every name. Places of things Link does not have yet stay empty, without
// outlines, so they do not tell what they are. The text uses the font of the
// modern text, or the dialogue font of the game when that option is off.
//
// As in the menus of the GBA Zelda games, an arrow on each side of the panel
// shows the way to the other page, with L/R on it when those buttons are not
// used for items: L/R change page, or the arrows past the edge of the page
// put the cursor on the side arrow and pressing on (or A) changes page.
// Opening and closing follow the scroll of the original menu (made shorter,
// HdInv_MenuStep): the game fades darker and the panel fades in, coming down
// a few whole pixels. The pages slide sideways.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hd_inv.h"
#include "hd_hud.h"
#include "hd_fx.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "features.h"
#include "config.h"
#include "hud.h"
#include "hud_it.h"
#include "snes/ppu.h"
#include "util.h"

// ---------------------------------------------------------------------------
// State

enum { kPageItems, kPageQuest };
enum { kSlideFrames = 10 };
enum { kOpenFrames = 12 };  // frames of the opening (232 px of scroll)
enum { kPopFrames = 8 };    // frames of the push of a side arrow

static int g_page;              // page shown
static int g_slide;             // frames of the page slide still to go
static int g_slide_dir;         // +1: the new page comes from the right
static int g_qcur;              // cursor of the Quest page (index in kQuest)
static int g_qfrom = -1;        // place the Quest cursor came from
static int g_arrow;             // -1/+1: cursor on the left/right arrow, 0: on the page
static int g_icur;              // free cursor of the Items page (slot), see FreeCursor
static int g_pop, g_pop_dir;    // frames of the push of the arrow of side g_pop_dir
static bool g_was_open;
static uint8 g_last_frame;
static uint8 g_saved_tm[2];
static bool g_tm_saved;

static bool IsPlayModule(int mod) {
  return mod == 7 || mod == 9 || mod == 11;
}

bool HdInv_Option(void) {
  return g_config.modern_items;
}

bool HdInv_Enabled(void) {
  return g_config.modern_items && main_module_index == 14 && submodule_index == 1 &&
         IsPlayModule(saved_module_for_menu);
}

int HdInv_MenuStep(void) {
  return (232 + kOpenFrames - 1) / kOpenFrames;
}

// 0 = closed, 1 = open (the scroll of the original menu)
static float OpenAmount(void) {
  int d = -(int16)BG3VOFS_copy2;
  return d <= 0 ? 0.0f : d >= 232 ? 1.0f : d / 232.0f;
}

// ---------------------------------------------------------------------------
// Layout (SNES pixels)

enum {
  kSlot = 26, kGap = 6, kPad = 10,
  kGridW = 6 * kSlot + 5 * kGap,   // 186: also the width of the Quest page
  kGridH = 4 * kSlot + 3 * kGap,   // 122
  // Quest page: header, 3 rows of equipment, header, the dungeon items
  kHeadH = 8, kQRowGap = 5,
  kQDungeonHead = kHeadH + 3 * kSlot + 3 * kQRowGap,  // 101
  kPageH = kQDungeonHead + kHeadH + kSlot,            // 135
  kBarGap = 6, kBarH = 17,
  kPanelW = kGridW + 2 * kPad,
  kPanelH = kPad + kPageH + kBarGap + kBarH + kPad,
  kTabGap = 5,
  kQuestCols = 3,
  kQuestGridW = kQuestCols * kSlot + (kQuestCols - 1) * kGap,  // 90
  kArrowW = 16, kArrowH = 18,
};

// Quest page: the slots on the left and the round places on the right
typedef struct QuestPlace {
  int what;       // kHudQuest_*
  float x, y;     // centre, from the top left of the page area
  float r;        // radius of a round place, 0 = square slot
  bool dungeon;   // only shown in a dungeon
} QuestPlace;

static QuestPlace kQuest[11 + 3 + 7];
static int kQuestCount;

static void BuildQuestLayout(void) {
  if (kQuestCount)
    return;
  // left part: equipment in rows of 3, 3 and 2 (centred), dungeon items below
  static const struct { int what; float col; int row; } kGrid[11] = {
    { kHudQuest_Sword, 0, 0 }, { kHudQuest_Shield, 1, 0 }, { kHudQuest_Armor, 2, 0 },
    { kHudQuest_Boots, 0, 1 }, { kHudQuest_Gloves, 1, 1 }, { kHudQuest_Flippers, 2, 1 },
    { kHudQuest_MoonPearl, 0.5f, 2 }, { kHudQuest_HeartPieces, 1.5f, 2 },
    { kHudQuest_Map, 0, 3 }, { kHudQuest_Compass, 1, 3 }, { kHudQuest_BigKey, 2, 3 },
  };
  int n = 0;
  for (int i = 0; i < 11; i++) {
    int row = kGrid[i].row;
    float top = row < 3 ? kHeadH + row * (kSlot + kQRowGap) : kQDungeonHead + kHeadH;
    kQuest[n++] = (QuestPlace){ kGrid[i].what, kGrid[i].col * (kSlot + kGap) + kSlot / 2.0f,
                                top + kSlot / 2.0f, 0, row == 3 };
  }
  // right part: pendants in a triangle, crystals 2-3-2 (as the original boxes)
  float rx0 = kQuestGridW + 12, cx = (rx0 + kGridW) / 2;
  float py = kHeadH + 11;
  kQuest[n++] = (QuestPlace){ kHudQuest_Pendant0, cx, py, 10 };
  kQuest[n++] = (QuestPlace){ kHudQuest_Pendant1, cx - 13, py + 19, 10 };
  kQuest[n++] = (QuestPlace){ kHudQuest_Pendant2, cx + 13, py + 19, 10 };
  float cy = 85, dx = 19, dy = 17;
  const float kCry[7][2] = {
    { -dx / 2, 0 }, { dx / 2, 0 }, { -dx, dy }, { 0, dy }, { dx, dy }, { -dx / 2, 2 * dy }, { dx / 2, 2 * dy },
  };
  for (int i = 0; i < 7; i++)
    kQuest[n++] = (QuestPlace){ kHudQuest_Crystal0 + i, cx + kCry[i][0], cy + kCry[i][1], 8 };
  kQuestCount = n;
}

static bool InDungeon(void) {
  return cur_palace_index_x2 != 0xff;
}

// Pendants and crystals (header and places) appear once the game has told
// of them, by the map icons step: 3 when Sahasrahla marks the map after
// speaking of the pendants, 6 on arriving in the Dark World (his telepathic
// message on the crystals); or as soon as Link has one
static bool PendantsKnown(void) {
  return savegame_map_icons_indicator >= 3 || (link_which_pendants & 7);
}

static bool CrystalsKnown(void) {
  return savegame_map_icons_indicator >= 6 || (link_has_crystals & 0x7f);
}

static bool QuestVisible(int i) {
  int w = kQuest[i].what;
  if (w >= kHudQuest_Pendant0 && w <= kHudQuest_Pendant2)
    return PendantsKnown();
  if (w >= kHudQuest_Crystal0 && w < kHudQuest_Crystal0 + 7)
    return CrystalsKnown();
  return !kQuest[i].dungeon || InDungeon();
}

// ---------------------------------------------------------------------------
// Names

static const char *BottleNameEn(int b) {
  static const char *const k[9] = {
    "MUSHROOM", "BOTTLE", "RED POTION", "GREEN POTION", "BLUE POTION",
    "FAIRY", "BEE", "GOOD BEE", "GOLD POTION",
  };
  return b >= 1 && b <= 9 ? k[b - 1] : "";
}

static const char *ItemNameEn(int item) {
  bool ext = Hud_ExtendedInventory();
  if (ext && item >= kHudItem_Bottle1 && item <= kHudItem_Bottle4)
    return BottleNameEn(link_bottle_info[item - kHudItem_Bottle1]);
  if (!ext && item == kHudItem_BottleOld) {
    int i = link_item_bottle_index;
    return i >= 1 && i <= 4 ? BottleNameEn(link_bottle_info[i - 1]) : "";
  }
  switch (item) {
  case 1: return link_item_bow == 2 ? "BOW AND ARROWS" : link_item_bow == 4 ? "BOW AND SILVER ARROWS" : "BOW";
  case 2: return link_item_boomerang == 2 ? "MAGICAL BOOMERANG" : "BOOMERANG";
  case 3: return "HOOKSHOT";
  case 4: return "BOMBS";
  case 5: return link_item_mushroom != 1 ? "MAGIC POWDER" : "MUSHROOM";
  case 6: return "FIRE ROD";
  case 7: return "ICE ROD";
  case 8: return "BOMBOS MEDALLION";
  case 9: return "ETHER MEDALLION";
  case 10: return "QUAKE MEDALLION";
  case 11: return "LAMP";
  case 12: return "MAGIC HAMMER";
  case 13: return link_item_flute != 1 ? "FLUTE" : "SHOVEL";
  case 14: return "BUG-CATCHING NET";
  case 15: return "BOOK OF MUDORA";
  case 16: return "SHOVEL";
  case 17: return "CANE OF SOMARIA";
  case 18: return "CANE OF BYRNA";
  case 19: return "MAGIC CAPE";
  case 20: return link_item_mirror != 1 ? "MAGIC MIRROR" : "";
  }
  return "";
}

// Name of an item, in one line
static void ItemName(int item, char *out, int size) {
  const char *s = HudIt_Active() ? HudIt_ItemNameFor(item) : ItemNameEn(item);
  int n = 0;
  for (; *s && n < size - 1; s++)
    out[n++] = *s == '\n' ? ' ' : *s;
  out[n] = 0;
}

static const char *QuestName(int what) {
  bool it = HudIt_Active();
  switch (what) {
  case kHudQuest_Sword: {
    static const char *const kIt[4] = { "SPADA DEL GUERRIERO", "SPADA SUPREMA", "SPADA TEMPRATA", "SPADA D'ORO" };
    static const char *const kEn[4] = { "FIGHTER'S SWORD", "MASTER SWORD", "TEMPERED SWORD", "GOLDEN SWORD" };
    int t = link_sword_type;
    return t >= 1 && t <= 4 ? (it ? kIt : kEn)[t - 1] : "";
  }
  case kHudQuest_Shield: {
    static const char *const kIt[3] = { "SCUDO DEL GUERRIERO", "SCUDO ROSSO", "SCUDO SPECCHIO" };
    static const char *const kEn[3] = { "FIGHTER'S SHIELD", "RED SHIELD", "MIRROR SHIELD" };
    int t = link_shield_type;
    return t >= 1 && t <= 3 ? (it ? kIt : kEn)[t - 1] : "";
  }
  case kHudQuest_Armor: {
    static const char *const kIt[3] = { "TUNICA VERDE", "CORAZZA BLU", "CORAZZA ROSSA" };
    static const char *const kEn[3] = { "GREEN CLOTHES", "BLUE MAIL", "RED MAIL" };
    return (it ? kIt : kEn)[link_armor <= 2 ? link_armor : 0];
  }
  case kHudQuest_Boots: return !link_item_boots ? "" : it ? "STIVALI DI PEGASO" : "PEGASUS BOOTS";
  case kHudQuest_Gloves:
    if (!link_item_gloves) return "";
    return link_item_gloves >= 2 ? (it ? "GUANTI DEL TITANO" : "TITAN'S MITT") : (it ? "GUANTI DEL POTERE" : "POWER GLOVE");
  case kHudQuest_Flippers: return !link_item_flippers ? "" : it ? "PINNE DI ZORA" : "ZORA'S FLIPPERS";
  case kHudQuest_MoonPearl: return !link_item_moon_pearl ? "" : it ? "PERLA LUNARE" : "MOON PEARL";
  case kHudQuest_HeartPieces: return it ? "PEZZI DI CUORE" : "PIECES OF HEART";
  case kHudQuest_Map: return !Hud_GetQuestIconTiles(what) ? "" : it ? "MAPPA" : "MAP";
  case kHudQuest_Compass: return !Hud_GetQuestIconTiles(what) ? "" : it ? "BUSSOLA" : "COMPASS";
  case kHudQuest_BigKey: return !Hud_GetQuestIconTiles(what) ? "" : it ? "GRANDE CHIAVE" : "BIG KEY";
  // bits of link_which_pendants: 1 Tower of Hera, 2 Desert Palace, 4 Eastern Palace
  case kHudQuest_Pendant0: return !Hud_GetQuestIconTiles(what) ? "" : it ? "AMULETO DELLA SAGGEZZA" : "PENDANT OF WISDOM";
  case kHudQuest_Pendant1: return !Hud_GetQuestIconTiles(what) ? "" : it ? "AMULETO DELLA FORZA" : "PENDANT OF POWER";
  case kHudQuest_Pendant2: return !Hud_GetQuestIconTiles(what) ? "" : it ? "AMULETO DEL CORAGGIO" : "PENDANT OF COURAGE";
  }
  return "";
}

// Crystals: all named after their dungeon ("Crystal <dungeon>") when every
// one of those names fits in the name bar, otherwise all "Crystal <n>" (the
// number of the dungeon). d: dungeon index (cur_palace_index_x2 / 2)
static const char *const kCrystalIt[13] = {
  [5] = "CRISTALLO DEDALO D'ACQUA", [6] = "CRISTALLO PALAZZO CUPO",
  [7] = "CRISTALLO MALAPALUDE", [8] = "CRISTALLO BOSCO D'OSSA",
  [9] = "CRISTALLO FORTE GHIACCIO", [11] = "CRISTALLO TANA DEI LADRONI",
  [12] = "CRISTALLO ROCCA TARTARUGA",
};
static const char *const kCrystalEn[13] = {
  [5] = "SWAMP PALACE CRYSTAL", [6] = "PALACE OF DARKNESS CRYSTAL",
  [7] = "MISERY MIRE CRYSTAL", [8] = "SKULL WOODS CRYSTAL",
  [9] = "ICE PALACE CRYSTAL", [11] = "THIEVES' TOWN CRYSTAL",
  [12] = "TURTLE ROCK CRYSTAL",
};
static const uint8 kCrystalNumber[13] = { [5] = 2, [6] = 1, [7] = 6, [8] = 3, [9] = 5, [11] = 4, [12] = 7 };

static bool NameFits(const char *name);

static void CrystalName(int what, char *out, int size) {
  out[0] = 0;
  if (!Hud_GetQuestIconTiles(what))
    return;
  bool it = HudIt_Active();
  const char *const *names = it ? kCrystalIt : kCrystalEn;
  bool all_fit = true;
  for (int d = 5; d < 13; d++)
    if (names[d] && !NameFits(names[d]))
      all_fit = false;
  int bit = 1 << (what - kHudQuest_Crystal0);
  for (int d = 5; d < 13; d++)
    if (kDungeonCrystalPendantBit[d] == bit && kCrystalNumber[d]) {
      if (all_fit)
        snprintf(out, size, "%s", names[d]);
      else
        snprintf(out, size, it ? "CRISTALLO %d" : "CRYSTAL %d", kCrystalNumber[d]);
      return;
    }
}

// The longest names of each language, for the one size of the name bar
static const char *const kLongestIt[] = {
  "ARCO E FRECCE D'ARGENTO", "BACCHETTA DEL GHIACCIO", "AMULETO DELLA SAGGEZZA",
  "RETINO PER FARFALLE", "POZIONE CURATUTTO", "GUANTI DEL POTERE",
};
static const char *const kLongestEn[] = {
  "BOW AND SILVER ARROWS", "PENDANT OF WISDOM", "PENDANT OF COURAGE",
  "BUG-CATCHING NET", "MAGICAL BOOMERANG", "FIGHTER'S SHIELD",
};

// ---------------------------------------------------------------------------
// Drawing at the output resolution

typedef struct Dst {
  uint8 *pixels;
  int pitch, width, height, s;
  int cx0, cy0, cx1, cy1;  // clip (output pixels)
  float alpha;
} Dst;

static Dst g_d;

static void Blend(int x, int y, uint32 col, float a) {
  a *= g_d.alpha;
  if (a <= 0.0f || x < g_d.cx0 || y < g_d.cy0 || x >= g_d.cx1 || y >= g_d.cy1)
    return;
  uint32 *p = (uint32 *)(g_d.pixels + y * g_d.pitch) + x;
  if (a >= 1.0f) {
    *p = (col & 0xffffff) | 0x02000000;  // alpha 2 = HUD: the shader leaves it
    return;
  }
  uint32 c = *p;
  int k = (int)(a * 256);
  int r = (c >> 16 & 0xff) + (((int)(col >> 16 & 0xff) - (int)(c >> 16 & 0xff)) * k >> 8);
  int g = (c >> 8 & 0xff) + (((int)(col >> 8 & 0xff) - (int)(c >> 8 & 0xff)) * k >> 8);
  int b = (c & 0xff) + (((int)(col & 0xff) - (int)(c & 0xff)) * k >> 8);
  *p = (a >= 0.5f ? 0x02000000 : (c & 0xff000000)) | r << 16 | g << 8 | b;
}

// Signed distance of a rounded rectangle (output pixels)
static float RRDist(float px, float py, float x, float y, float w, float h, float r) {
  float qx = fabsf(px - (x + w * 0.5f)) - (w * 0.5f - r), qy = fabsf(py - (y + h * 0.5f)) - (h * 0.5f - r);
  float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
  float in = qx > qy ? qx : qy;
  return sqrtf(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

// Rounded rectangle in SNES pixels: filled (t = 0) or its outline t thick.
// col2 != col: vertical gradient from col (top) to col2 (bottom)
static void RoundRect(float x, float y, float w, float h, float r, float t, uint32 col, uint32 col2, float a) {
  float s = (float)g_d.s;
  x *= s, y *= s, w *= s, h *= s, r *= s, t *= s;
  int x0 = (int)floorf(x - t - 1), y0 = (int)floorf(y - t - 1), x1 = (int)ceilf(x + w + t + 1), y1 = (int)ceilf(y + h + t + 1);
  x0 = IntMax(x0, g_d.cx0), y0 = IntMax(y0, g_d.cy0), x1 = IntMin(x1, g_d.cx1), y1 = IntMin(y1, g_d.cy1);
  for (int yy = y0; yy < y1; yy++) {
    uint32 c = col;
    if (col2 != col) {
      float f = (yy - y) / (h > 1 ? h : 1);
      f = f < 0 ? 0 : f > 1 ? 1 : f;
      int k = (int)(f * 256);
      c = 0;
      for (int sh = 0; sh <= 16; sh += 8) {
        int a0 = col >> sh & 0xff, a1 = col2 >> sh & 0xff;
        c |= (uint32)(a0 + ((a1 - a0) * k >> 8)) << sh;
      }
    }
    for (int xx = x0; xx < x1; xx++) {
      float d = RRDist(xx + 0.5f, yy + 0.5f, x, y, w, h, r);
      if (t > 0)
        d = fabsf(d) - t * 0.5f;
      float cov = 0.5f - d;
      if (cov > 0)
        Blend(xx, yy, c, a * (cov > 1 ? 1 : cov));
    }
  }
}

static void Circle(float cx, float cy, float r, float t, uint32 col, float a) {
  RoundRect(cx - r, cy - r, 2 * r, 2 * r, r, t, col, col, a);
}

// One SNES pixel at (x, y) (output pixels of its top left corner)
static void Block(int x, int y, int size, uint32 col, float a) {
  for (int yy = 0; yy < size; yy++)
    for (int xx = 0; xx < size; xx++)
      Blend(x + xx, y + yy, col, a);
}

// ---------------------------------------------------------------------------
// Icons from the HUD tiles (as in hd_hud.c)

static void DecodeTile(uint16 entry, uint32 *out, int stride) {
  Ppu *ppu = g_zenv.ppu;
  int tile = entry & 0x3ff, pal = entry >> 10 & 7;
  bool hf = entry & 0x4000, vf = entry & 0x8000;
  uint16 base = ppu->bgLayer[2].tileAdr + tile * 8;
  for (int r = 0; r < 8; r++) {
    uint16 w = ppu->vram[(base + (vf ? 7 - r : r)) & 0x7fff];
    for (int c = 0; c < 8; c++) {
      int bit = hf ? c : 7 - c;
      int v = (w >> bit & 1) | (w >> (8 + bit) & 1) << 1;
      out[r * stride + c] = v ? (HdFx_Bgr15ToRgb(ppu->cgram[pal * 4 + v]) | 0xff000000) : 0;
    }
  }
}

// 16x16 icon centred on (cx, cy) (SNES pixels, the centre of its visible
// pixels), with a dark outline of one pixel as in the modern HUD
static void DrawIcon(const uint16 *t, float cx, float cy) {
  uint32 img[16 * 16];
  DecodeTile(t[0], img, 16);
  DecodeTile(t[1], img + 8, 16);
  DecodeTile(t[2], img + 8 * 16, 16);
  DecodeTile(t[3], img + 8 * 16 + 8, 16);
  int bx0 = 16, by0 = 16, bx1 = -1, by1 = -1;
  for (int i = 0; i < 256; i++) {
    uint32 c = img[i];
    if (((c >> 16 & 0xff) + (c >> 8 & 0xff) + (c & 0xff)) < 40)
      img[i] = 0;
    if (img[i]) {
      int x = i & 15, y = i >> 4;
      bx0 = IntMin(bx0, x), bx1 = IntMax(bx1, x), by0 = IntMin(by0, y), by1 = IntMax(by1, y);
    }
  }
  if (bx1 < 0)
    return;
  int s = g_d.s;
  // top left of the icon, on whole SNES pixels of the panel
  int ox = (int)floorf((cx - (bx0 + bx1 + 1) * 0.5f) * s + 0.5f);
  int oy = (int)floorf((cy - (by0 + by1 + 1) * 0.5f) * s + 0.5f);
  for (int r = -1; r <= 16; r++)
    for (int c = -1; c <= 16; c++) {
      if (r >= 0 && r < 16 && c >= 0 && c < 16 && img[r * 16 + c])
        continue;
      bool near = false;
      for (int k = 0; k < 4 && !near; k++) {
        int rr = r + (k == 0) - (k == 1), cc = c + (k == 2) - (k == 3);
        near = rr >= 0 && rr < 16 && cc >= 0 && cc < 16 && img[rr * 16 + cc];
      }
      if (near)
        Block(ox + c * s, oy + r * s, s, 0x181418, 1);
    }
  for (int i = 0; i < 256; i++)
    if (img[i])
      Block(ox + (i & 15) * s, oy + (i >> 4) * s, s, img[i], 1);
}

// ---------------------------------------------------------------------------
// Text

// Code of a character in the dialogue alphabet (modern font and game font)
static int FontCode(char ch) {
  if (ch >= 'A' && ch <= 'Z') return ch - 'A';
  if (ch >= 'a' && ch <= 'z') return 26 + ch - 'a';
  if (ch >= '0' && ch <= '9') return 52 + ch - '0';
  switch (ch) {
  case '!': return 62;
  case '?': return 63;
  case '-': return 64;
  case '.': return 65;
  case ',': return 66;
  case '\'': return 81;
  }
  return -1;
}

enum { kSpaceW = 3 };

// Modern text off: the dialogue font of the game (16 rows, 2 bits a pixel)
static const uint8 *g_gf_data, *g_gf_width;
static int g_gf_count, g_gf_top, g_gf_h;

// Loads the game font and finds the rows its capitals use (with the shadow)
static bool GameFontLoad(void) {
  MemBlk data = FindIndexInMemblk(g_zenv.dialogue_font_blk, 0);
  MemBlk width = FindIndexInMemblk(g_zenv.dialogue_font_blk, 1);
  if (!data.ptr || !width.ptr || width.size < 26)
    return false;
  if (data.ptr != g_gf_data) {
    g_gf_data = data.ptr, g_gf_width = width.ptr, g_gf_count = (int)width.size;
    int top = 16, bottom = -1;
    for (int c = 0; c < 26; c++)
      for (int r = 0; r < 16; r++)
        for (int i = 0; i < 8; i++)
          if (HdFx_GameGlyphPixel(g_gf_data, c, r, i))
            top = IntMin(top, r), bottom = IntMax(bottom, r);
    g_gf_top = bottom >= 0 ? top : 0;
    g_gf_h = bottom >= 0 ? bottom - top + 1 : 16;
  }
  return true;
}

static bool UseGameFont(void) {
  return !g_config.modern_text && GameFontLoad();
}

// Advance of a character of the game font (code < 0: a space)
static int GameAdvance(int code) {
  if (code < 0)
    return g_gf_count > 89 && g_gf_width[89] ? g_gf_width[89] : 4;
  return code < g_gf_count ? IntMin(g_gf_width[code], 8) : 0;
}

// Height of the capitals in font pixels
static int TextH(void) {
  return UseGameFont() ? g_gf_h : 7;
}

// Width in font pixels
static int TextW(const char *s) {
  int w = 0;
  if (UseGameFont()) {
    for (; *s; s++)
      w += GameAdvance(FontCode(*s));
    return w ? w - 1 : 0;
  }
  for (; *s; s++) {
    const uint16 *bits;
    int gw = FontCode(*s) >= 0 ? HdText_ModernGlyph(FontCode(*s), &bits) : 0;
    w += (gw ? gw : kSpaceW) + 1;
  }
  return w ? w - 1 : 0;
}

// Text with font pixels of fp output pixels, top left of the capitals at
// (x, y) output pixels. Game font: only the body of the letters (its color
// 1, the dark outline of the dialogues, is left out)
static void Text(const char *s, int x, int y, int fp, uint32 col) {
  if (UseGameFont()) {
    for (; *s; s++) {
      int code = FontCode(*s);
      if (code >= 0 && code < g_gf_count)
        for (int r = 0; r < g_gf_h; r++)
          for (int i = 0; i < 8; i++) {
            int v = HdFx_GameGlyphPixel(g_gf_data, code, g_gf_top + r, i);
            if (v >= 2)
              Block(x + i * fp, y + r * fp, fp, col, 1);
          }
      x += GameAdvance(code) * fp;
    }
    return;
  }
  for (; *s; s++) {
    const uint16 *bits;
    int code = FontCode(*s), gw = code >= 0 ? HdText_ModernGlyph(code, &bits) : 0;
    if (!gw) {
      x += (kSpaceW + 1) * fp;
      continue;
    }
    for (int r = 0; r < 9; r++)
      for (int c = 0; c < gw; c++)
        if (bits[r] >> (gw - 1 - c) & 1)
          Block(x + c * fp, y + r * fp, fp, col, 1);
    x += (gw + 1) * fp;
  }
}

// Small capitals of the inventory headers (3x5, HudIt_SmallGlyph)
static int SmallW(const char *s) {
  int w = 0;
  for (; *s; s++) {
    const char *const *g = HudIt_SmallGlyph(*s);
    w += (g ? (int)strlen(g[0]) : 3) + 1;
  }
  return w ? w - 1 : 0;
}

static void SmallText(const char *s, float x, float y, uint32 col) {
  int s_ = g_d.s, ox = (int)floorf(x * s_ + 0.5f), oy = (int)floorf(y * s_ + 0.5f);
  for (; *s; s++) {
    const char *const *g = HudIt_SmallGlyph(*s);
    if (!g) {
      ox += 4 * s_;
      continue;
    }
    int w = (int)strlen(g[0]);
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < w; c++)
        if (g[r][c] == '#')
          Block(ox + c * s_, oy + r * s_, s_, col, 1);
    ox += (w + 1) * s_;
  }
}

// Digits 3x5 with a dark outline (as the counts of the modern HUD buttons)
static void Digits(const char *s, float right, float bottom, uint32 col) {
  static const uint8 kDigits[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {6,1,2,4,7}, {6,1,2,1,6}, {5,5,7,1,1},
    {7,4,6,1,6}, {3,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,6},
  };
  static const uint8 kSlash[5] = { 1, 1, 2, 4, 4 };
  int n = (int)strlen(s), sc = g_d.s;
  int ox = (int)floorf((right - (n * 4 - 1)) * sc + 0.5f), oy = (int)floorf((bottom - 5) * sc + 0.5f);
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < n; i++) {
      const uint8 *g = s[i] == '/' ? kSlash : s[i] >= '0' && s[i] <= '9' ? kDigits[s[i] - '0'] : NULL;
      if (!g)
        continue;
      for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++) {
          if (!(g[r] >> (2 - c) & 1))
            continue;
          int px = ox + (i * 4 + c) * sc, py = oy + r * sc;
          if (pass == 0) {
            for (int dy = -1; dy <= 1; dy++)
              for (int dx = -1; dx <= 1; dx++)
                Block(px + dx * sc, py + dy * sc, sc, 0x181418, 1);
          } else {
            Block(px, py, sc, col, 1);
          }
        }
    }
}

// ---------------------------------------------------------------------------
// Style ("light stone")

enum {
  kColPanelTop = 0xe2dcd6, kColPanelBottom = 0xccc5be, kColBorder = 0x786c64,
  kColInner = 0xfaf6f0, kColSlot = 0xb6aea8, kColSlotEdge = 0x968d86,
  kColSlotHi = 0xece7e1, kColInk = 0x463a34, kColInkSoft = 0x766860,
  kColAccent = 0xe87028, kColTabOff = 0xc4bdb6, kColTabInk = 0x60544e,
  kColTabOn = 0x786c64, kColTabOnInk = 0xfffaf4, kColBadgeInk = 0xffffff,
};

static void Slot(float x, float y, float w, float h) {
  RoundRect(x, y, w, h, 3, 0, kColSlot, kColSlot, 1);
  RoundRect(x + 0.25f, y + 0.25f, w - 0.5f, h - 0.5f, 3, 0.5f, kColSlotEdge, kColSlotEdge, 1);
  RoundRect(x + 1, y + h - 1.2f, w - 2, 0.6f, 0.3f, 0, kColSlotHi, kColSlotHi, 0.8f);
}

static void RoundPlace(float cx, float cy, float r) {
  Circle(cx, cy, r, 0, kColSlot, 1);
  Circle(cx, cy, r - 0.25f, 0.5f, kColSlotEdge, 1);
}

// Cursor: four corners just outside the slot
static void Cursor(float x, float y, float w, float h) {
  float g = 2, L = 5, t = 1.5f;
  for (int i = 0; i < 4; i++) {
    float cx = (i & 1) ? x + w + g : x - g, cy = (i & 2) ? y + h + g : y - g;
    float sx = (i & 1) ? -1 : 1, sy = (i & 2) ? -1 : 1;
    RoundRect(sx > 0 ? cx : cx - L, sy > 0 ? cy : cy - t, L, t, 0.6f, 0, kColAccent, kColAccent, 1);
    RoundRect(sx > 0 ? cx : cx - t, sy > 0 ? cy : cy - L, t, L, 0.6f, 0, kColAccent, kColAccent, 1);
  }
}

// Button an item is on (Y, X, L, R): a small badge in the top right corner
static void Badge(float x, float y, char ch) {
  Circle(x, y, 4, 0, kColAccent, 1);
  char s[2] = { ch, 0 };
  SmallText(s, x - SmallW(s) / 2.0f, y - 2.5f, kColBadgeInk);
}

// Size of the names (output pixels a font pixel): the one that fits the
// longest name of the language. The game font stays at its own size
static int NameFontPixel(void) {
  int s = g_d.s;
  if (UseGameFont())
    return s;
  int longest = 1;
  const char *const *l = HudIt_Active() ? kLongestIt : kLongestEn;
  for (int i = 0; i < 6; i++)
    longest = IntMax(longest, TextW(l[i]));
  int fp = IntMin((kGridW - 10) * s / longest, (kBarH - 6) * s / 7);
  return IntMax(fp, 1);
}

static bool NameFits(const char *name) {
  return TextW(name) * NameFontPixel() <= (kGridW - 12) * g_d.s;
}

// Name bar: the name in capitals, centred, in the size of the longest name
static void NameBar(float x, float y, const char *name) {
  Slot(x, y, kGridW, kBarH);
  if (!*name)
    return;
  int s = g_d.s, fp = NameFontPixel();
  int w = TextW(name) * fp, h = TextH() * fp;
  int ox = (int)floorf((x + kGridW / 2.0f) * s - w / 2.0f + 0.5f);
  int oy = (int)floorf((y + kBarH / 2.0f) * s - h / 2.0f + 0.5f);
  Text(name, ox, oy, fp, kColInk);
}

// ---------------------------------------------------------------------------
// Pages (x, y = top left of the page area inside the panel)

// With X and/or L and R for items the cursor moves freely on the grid and
// the button pressed (Y, X, L, R) puts the item under it on that button;
// with Y alone the cursor is the item on Y, as in the game
static bool FreeCursor(void) {
  return Hud_ItemOnX() || Hud_ItemOnLR();
}

// Slot of the cursor on the Items page
static int CursorSlot(void) {
  if (FreeCursor())
    return g_icur >= 0 && g_icur < Hud_MenuSlotCount() ? g_icur : 0;
  return Hud_MenuSlotOf(*GetCurrentItemButtonPtr(GetCurrentItemButtonIndex()));
}

static void DrawItemsPage(float x, float y) {
  int cols = Hud_MenuColumns(), count = Hud_MenuSlotCount();
  int gw = cols * kSlot + (cols - 1) * kGap;
  float gx = x + (kGridW - gw) / 2.0f, gy = y + (kPageH - kGridH) / 2;
  int cs = CursorSlot();
  int cur_item = cs >= 0 ? Hud_MenuItemAt(cs) : 0;
  if (cur_item && !Hud_HaveItem(cur_item))
    cur_item = 0;  // free cursor on an empty slot: no name
  bool bottle_menu = overworld_map_state >= 9 && overworld_map_state <= 10;
  // the four bottles in one column: one long slot instead of four
  int b0 = -1, b1 = -1;
  if (Hud_ExtendedInventory())
    for (int i = 0; i < count; i++) {
      int item = Hud_MenuItemAt(i);
      if (item >= kHudItem_Bottle1 && item <= kHudItem_Bottle4)
        b0 = b0 < 0 ? i : b0, b1 = i;
    }
  bool strip = b0 >= 0 && b1 - b0 == 3 * cols;
  if (strip)
    Slot(gx + (b0 % cols) * (kSlot + kGap), gy + (b0 / cols) * (kSlot + kGap), kSlot, 3 * (kSlot + kGap) + kSlot);
  for (int i = 0; i < count; i++) {
    float sx = gx + (i % cols) * (kSlot + kGap), sy = gy + (i / cols) * (kSlot + kGap);
    if (!(strip && i >= b0 && i <= b1 && i % cols == b0 % cols))
      Slot(sx, sy, kSlot, kSlot);
    int item = Hud_MenuItemAt(i);
    if (!Hud_HaveItem(item))
      continue;
    DrawIcon(Hud_GetItemIconTiles(item), sx + kSlot / 2.0f, sy + kSlot / 2.0f);
    // counts of arrows and bombs, as in the modern HUD
    int n = -1, max = -1;
    if (item == 1 && link_item_bow)
      n = link_num_arrows, max = kMaxArrowsForLevel[link_arrow_upgrades & 7];
    else if (item == kHudItem_Bombs)
      n = link_item_bombs, max = kMaxBombsForLevel[link_bomb_upgrades & 7];
    if (n >= 0) {
      char buf[4];
      n = n > 99 ? 99 : n;
      if (n >= 10) buf[0] = '0' + n / 10, buf[1] = '0' + n % 10, buf[2] = 0;
      else buf[0] = '0' + n, buf[1] = 0;
      bool yellow = (enhanced_features0 & kFeatures0_ShowMaxItemsInYellow) && n == max;
      Digits(buf, sx + kSlot - 2, sy + kSlot - 2, yellow ? 0xffde52 : 0xf8f8f8);
    }
    // buttons the item is on
    float bx = sx + kSlot - 2, by = sy + 2;
    if (item == hud_cur_item) Badge(bx, by, 'Y'), bx -= 9;
    if (Hud_ItemOnX() && item == hud_cur_item_x) Badge(bx, by, 'X'), bx -= 9;
    if (Hud_ItemOnLR() && item == hud_cur_item_l) Badge(bx, by, 'L'), bx -= 9;
    if (Hud_ItemOnLR() && item == hud_cur_item_r) Badge(bx, by, 'R');
  }
  if (cs >= 0 && !bottle_menu && !g_arrow)
    Cursor(gx + (cs % cols) * (kSlot + kGap), gy + (cs / cols) * (kSlot + kGap), kSlot, kSlot);
  // original inventory (only for a moment, the option brings the extended
  // one): the bottle menu, a column of the four bottles beside the slot
  if (bottle_menu && cs >= 0) {
    float bx = gx + (cs % cols + 1) * (kSlot + kGap) - kGap / 2.0f, by = gy;
    RoundRect(bx - 3, by - 3, kSlot + 6, kGridH + 6, 5, 0, kColPanelTop, kColPanelBottom, 1);
    RoundRect(bx - 3, by - 3, kSlot + 6, kGridH + 6, 5, 1, kColBorder, kColBorder, 1);
    for (int i = 0; i < 4; i++) {
      float sy = by + i * (kSlot + kGap);
      Slot(bx, sy, kSlot, kSlot);
      if (link_bottle_info[i])
        DrawIcon(Hud_GetBottleIconTiles(link_bottle_info[i]), bx + kSlot / 2.0f, sy + kSlot / 2.0f);
      if (i == link_item_bottle_index - 1)
        Cursor(bx, sy, kSlot, kSlot);
    }
  }
  if (g_arrow)
    return;
  char name[48];
  ItemName(cur_item, name, sizeof(name));
  NameBar(x, y + kPageH + kBarGap, name);
}

static void DrawQuestPage(float x, float y) {
  BuildQuestLayout();
  bool it = HudIt_Active();
  float rx0 = x + kQuestGridW + 12, rcx = (rx0 + x + kGridW) / 2, lcx = x + kQuestGridW / 2.0f;
  const char *le = it ? "EQUIPAGGIAMENTO" : "EQUIPMENT", *ld = it ? "OGGETTI LABIRINTO" : "DUNGEON ITEMS";
  const char *lp = it ? "AMULETI" : "PENDANTS", *lc = it ? "CRISTALLI" : "CRYSTALS";
  SmallText(le, lcx - SmallW(le) / 2.0f, y, kColInkSoft);
  if (InDungeon())
    SmallText(ld, lcx - SmallW(ld) / 2.0f, y + kQDungeonHead, kColInkSoft);
  if (PendantsKnown())
    SmallText(lp, rcx - SmallW(lp) / 2.0f, y, kColInkSoft);
  // crystals header just above the first row of crystals
  if (CrystalsKnown())
    SmallText(lc, rcx - SmallW(lc) / 2.0f, y + kQuest[kQuestCount - 7].y - 8 - kHeadH - 1, kColInkSoft);
  for (int i = 0; i < kQuestCount; i++) {
    const QuestPlace *q = &kQuest[i];
    if (!QuestVisible(i))
      continue;
    float cx = x + q->x, cy = y + q->y;
    if (q->r > 0)
      RoundPlace(cx, cy, q->r);
    else
      Slot(cx - kSlot / 2.0f, cy - kSlot / 2.0f, kSlot, kSlot);
    const uint16 *t = Hud_GetQuestIconTiles(q->what);
    if (t)
      DrawIcon(t, cx, cy);
    if (q->what == kHudQuest_HeartPieces) {
      char buf[8] = { '0' + (link_heart_pieces & 3), '/', '4', 0 };
      Digits(buf, cx + kSlot / 2.0f - 2, cy + kSlot / 2.0f - 2, 0xf8f8f8);
    }
  }
  if (g_arrow)
    return;
  if (g_qcur >= kQuestCount || !QuestVisible(g_qcur))
    g_qcur = 0;
  const QuestPlace *c = &kQuest[g_qcur];
  float cx = x + c->x, cy = y + c->y;
  if (c->r > 0)
    Cursor(cx - c->r, cy - c->r, 2 * c->r, 2 * c->r);
  else
    Cursor(cx - kSlot / 2.0f, cy - kSlot / 2.0f, kSlot, kSlot);
  const char *name = QuestName(c->what);
  char crystal[48];
  if (c->what >= kHudQuest_Crystal0 && c->what < kHudQuest_Crystal0 + 7) {
    CrystalName(c->what, crystal, sizeof(crystal));
    name = crystal;
  }
  NameBar(x, y + kPageH + kBarGap, name);
}

static void DrawPage(int page, float x, float y) {
  if (page == kPageItems)
    DrawItemsPage(x, y);
  else
    DrawQuestPage(x, y);
}

static const char *PageName(int page) {
  bool it = HudIt_Active();
  return page == kPageItems ? (it ? "OGGETTI" : "ITEMS") : (it ? "MISSIONE" : "QUEST");
}

static int TabH(void) {
  return TextH() + 5;
}

// Tabs above the panel: Items, Quest
static void DrawTabs(float cx, float y) {
  int s = g_d.s, th = TabH();
  float pad = 7, gap = 5, w[2];
  for (int i = 0; i < 2; i++)
    w[i] = TextW(PageName(i)) + 2 * pad;
  float x = cx - (w[0] + w[1] + gap) / 2;
  for (int i = 0; i < 2; i++) {
    bool on = i == g_page;
    uint32 fill = on ? kColTabOn : kColTabOff;
    RoundRect(x, y, w[i], th, th / 2.0f, 0, fill, fill, on ? 1.0f : 0.92f);
    int tx = (int)floorf((x + pad) * s + 0.5f), ty = (int)floorf((y + (th - TextH()) / 2.0f) * s + 0.5f);
    Text(PageName(i), tx, ty, s, on ? kColTabOnInk : kColTabInk);
    x += w[i] + gap;
  }
}

// Arrow on one side of the panel (dir -1 left, +1 right) to the other page,
// with L or R on it when those buttons change page. x: the panel edge,
// cy: the middle of the page area
static float ArrowX(float x, int dir) {
  return dir < 0 ? x - kArrowW + 5 : x - 5;
}

static void SideArrow(float x, float cy, int dir) {
  float push = g_pop > 0 && g_pop_dir == dir ? floorf(2.0f * g_pop / kPopFrames + 0.5f) : 0;
  float ax = ArrowX(x, dir) + dir * push, ay = cy - kArrowH / 2.0f;
  bool sel = g_arrow == dir;
  uint32 fill = sel ? kColAccent : kColTabOn;
  RoundRect(ax, ay, kArrowW, kArrowH, 5, 0, fill, fill, 1);
  int s = g_d.s;
  // "<L" on the left, "R>" on the right; only the triangle when L/R are
  // used for items
  const char *l = !Hud_ItemOnLR() ? (dir < 0 ? "L" : "R") : NULL;
  int lw = l ? SmallW(l) + 1 : 0, w = 4 + lw;
  int x0 = (int)floorf(ax + (kArrowW - w) / 2.0f + 0.5f), cyi = (int)floorf(cy);
  int tx = dir < 0 ? x0 : x0 + lw;
  for (int r = -3; r <= 3; r++)
    for (int c = 0; c <= 3 - abs(r); c++)
      Block((tx + (dir > 0 ? c : 3 - c)) * s, (cyi + r) * s, s, kColTabOnInk, 1);
  if (l)
    SmallText(l, dir < 0 ? x0 + 5 : x0, cyi - 2, kColTabOnInk);
}

// ---------------------------------------------------------------------------

static float EaseInOut(float t) {
  t = t < 0 ? 0 : t > 1 ? 1 : t;
  return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2;
}

void HdInv_Draw(uint8 *pixels, int pitch, int width, int height, int scale) {
  if (!HdInv_Enabled())
    return;
  float open = OpenAmount();
  // state 0/1: the menu is being built, nothing to show yet
  if (open <= 0)
    return;
  // the page slide and the arrow push go on one step per game frame
  if (g_last_frame != frame_counter) {
    if (g_slide > 0)
      g_slide--;
    if (g_pop > 0)
      g_pop--;
    g_last_frame = frame_counter;
  }
  // even fade over the whole opening (an ease out made it all happen in
  // the first few frames)
  float e = open * open * (3 - 2 * open);
  // the game behind gets darker (the menu blur, if on, adds its own)
  int keep = (int)(256 - 92 * e);
  for (int y = 0; y < height; y++) {
    uint32 *row = (uint32 *)(pixels + y * pitch);
    for (int x = 0; x < width; x++) {
      uint32 c = row[x];
      row[x] = (c & 0xff000000) | ((c >> 16 & 0xff) * keep >> 8) << 16 |
               ((c >> 8 & 0xff) * keep >> 8) << 8 | ((c & 0xff) * keep >> 8);
    }
  }
  g_d = (Dst){ pixels, pitch, width, height, scale, 0, 0, width, height, e };
  float cw = (float)width / scale, ch = (float)height / scale;
  float total = TabH() + kTabGap + kPanelH;
  // the panel comes down a few whole pixels: icons (on whole pixels) and
  // slots move together
  float top = floorf((ch - total) / 2) - floorf((1 - e) * 6 + 0.5f);
  float px = floorf((cw - kPanelW) / 2), py = top + TabH() + kTabGap;
  DrawTabs(cw / 2, top);
  // soft shadow under the panel
  for (int i = 4; i >= 1; i--)
    RoundRect(px - i * 0.7f, py + 2 - i * 0.7f + i * 0.5f, kPanelW + i * 1.4f, kPanelH + i * 1.4f, 5 + i * 0.7f, 0, 0x000000, 0x000000, 0.10f);
  RoundRect(px, py, kPanelW, kPanelH, 5, 0, kColPanelTop, kColPanelBottom, 1);
  RoundRect(px + 0.5f, py + 0.5f, kPanelW - 1, kPanelH - 1, 4.5f, 1, kColBorder, kColBorder, 1);
  RoundRect(px + 1.6f, py + 1.6f, kPanelW - 3.2f, kPanelH - 3.2f, 3.5f, 1.0f / scale, kColInner, kColInner, 0.8f);
  float ax = px + kPad, ay = py + kPad;
  // the pages, clipped to the inside of the panel while they slide
  g_d.cx0 = (int)((px + 2) * scale), g_d.cx1 = (int)((px + kPanelW - 2) * scale);
  g_d.cy0 = (int)((py + 2) * scale), g_d.cy1 = (int)((py + kPanelH - 2) * scale);
  g_d.cx0 = IntMax(g_d.cx0, 0), g_d.cy0 = IntMax(g_d.cy0, 0);
  g_d.cx1 = IntMin(g_d.cx1, width), g_d.cy1 = IntMin(g_d.cy1, height);
  if (g_slide > 0) {
    float f = EaseInOut(1 - (float)g_slide / kSlideFrames);
    float off = floorf(f * kPanelW + 0.5f);
    DrawPage(g_page ^ 1, ax - g_slide_dir * off, ay);
    DrawPage(g_page, ax + g_slide_dir * (kPanelW - off), ay);
  } else {
    DrawPage(g_page, ax, ay);
  }
  // on an arrow: the name of the page it leads to
  if (g_arrow && g_slide == 0)
    NameBar(ax, ay + kPageH + kBarGap, PageName(g_page ^ 1));
  // arrows to the other page, on the sides of the page area, over the
  // pages (they come into the panel a little: drawn before, the sliding
  // pages covered them)
  g_d.cx0 = 0, g_d.cy0 = 0, g_d.cx1 = width, g_d.cy1 = height;
  float acy = floorf(ay + kPageH / 2.0f);
  SideArrow(px, acy, -1);
  SideArrow(px + kPanelW, acy, 1);
  if (g_arrow)
    Cursor(ArrowX(g_arrow < 0 ? px : px + kPanelW, g_arrow), acy - kArrowH / 2.0f, kArrowW, kArrowH);
}

// ---------------------------------------------------------------------------
// Keys

// Quest page: the nearest place in the direction pressed (dx, dy), -1 if none
// How near a place at (vx, vy) from the cursor is for the direction pressed
// (dx, dy): its distance, if it lies within 60 degrees of that direction
// (a little more for what is off the straight line, so that of two at the
// same distance the straighter wins); < 0 if it is not that way. Any place
// is within 45 degrees of one of the four directions: all can be reached
static float StepScore(float vx, float vy, int dx, int dy) {
  float along = vx * dx + vy * dy, across = fabsf(dx ? vy : vx);
  if (along < 4 || across > along * 1.75f)
    return -1;
  return vx * vx + vy * vy + 1.5f * across * across;
}

static int QuestStep(int from, int dx, int dy) {
  const QuestPlace *a = &kQuest[from];
  int best = -1;
  float best_d = 1e9f;
  for (int i = 0; i < kQuestCount; i++) {
    if (i == from || !QuestVisible(i))
      continue;
    float d = StepScore(kQuest[i].x - a->x, kQuest[i].y - a->y, dx, dy);
    // of two places just as near (the two lower pendants from the upper
    // one), the one the cursor came from: going back retraces the step
    if (d >= 0 && i == g_qfrom)
      d -= 0.5f;
    if (d >= 0 && d < best_d)
      best_d = d, best = i;
  }
  return best;
}

// Entering the Quest page from one side: the place nearest to row `row` (0-3)
// of the item grid, on the left edge or on the right edge
static int QuestEnter(bool from_left, int row) {
  float ty = (kPageH - kGridH) / 2 + row * (kSlot + kGap) + kSlot / 2.0f;
  int best = 0;
  float best_d = 1e9f;
  for (int i = 0; i < kQuestCount; i++) {
    if (!QuestVisible(i))
      continue;
    float edge = from_left ? kQuest[i].x : kGridW - kQuest[i].x;
    float d = edge * 4 + fabsf(kQuest[i].y - ty);
    if (d < best_d)
      best_d = d, best = i;
  }
  return best;
}

// Row of the item grid at the height of the Quest page cursor
static int QuestRow(void) {
  float top = (kPageH - kGridH) / 2.0f;
  int row = (int)((kQuest[g_qcur < kQuestCount ? g_qcur : 0].y - top) / (kSlot + kGap));
  return row < 0 ? 0 : row > 3 ? 3 : row;
}

// Row of the item grid the cursor is on
static int ItemsRow(void) {
  int slot = CursorSlot();
  return slot < 0 ? 0 : slot / Hud_MenuColumns();
}

// Nearest slot with an item Link has from slot in direction (dx, dy), by
// position; -1 if there is none that way
static int ItemStep(int slot, int dx, int dy) {
  int cols = Hud_MenuColumns(), count = Hud_MenuSlotCount(), best = -1;
  float best_d = 1e9f;
  for (int k = 0; slot >= 0 && k < count; k++) {
    if (k == slot || !Hud_HaveItem(Hud_MenuItemAt(k)))
      continue;
    float d = StepScore((float)(k % cols - slot % cols) * (kSlot + kGap),
                        (float)(k / cols - slot / cols) * (kSlot + kGap), dx, dy);
    if (d >= 0 && d < best_d)
      best_d = d, best = k;
  }
  return best;
}

// Entering the Items page from one side: the item Link has nearest to that
// edge and to grid row row; -1 if he has none
static int ItemEnter(bool from_left, int row) {
  int cols = Hud_MenuColumns(), count = Hud_MenuSlotCount(), best = -1, best_d = 1 << 30;
  for (int k = 0; k < count; k++) {
    if (!Hud_HaveItem(Hud_MenuItemAt(k)))
      continue;
    int edge = from_left ? k % cols : cols - 1 - k % cols, dr = k / cols - row;
    int d = edge * 4 + (dr < 0 ? -dr : dr);
    if (d < best_d)
      best_d = d, best = k;
  }
  return best;
}

// Changes page; the new one comes in from side dir
static void SwitchPage(int dir) {
  int row = g_page == kPageItems ? ItemsRow() : QuestRow();
  g_page ^= 1;
  g_slide = kSlideFrames;
  g_slide_dir = dir;
  g_arrow = 0;
  g_pop = kPopFrames, g_pop_dir = dir;
  if (g_page == kPageQuest)
    g_qcur = QuestEnter(dir > 0, row), g_qfrom = -1;
  else if (FreeCursor()) {
    // the free cursor comes in on the item nearest to the side it enters from
    int k = ItemEnter(dir > 0, row);
    if (k >= 0)
      g_icur = k;
  }
  sound_effect_2 = 32;
}

bool HdInv_HandleInput(void) {
  if (!HdInv_Enabled())
    return false;
  BuildQuestLayout();
  if (g_qcur >= kQuestCount || !QuestVisible(g_qcur))
    g_qcur = 0;
  uint8 h = filtered_joypad_H, l = filtered_joypad_L;
  // L and R change page when no item is on them
  if (!Hud_ItemOnLR() && (l & (kJoypadL_L | kJoypadL_R))) {
    SwitchPage((l & kJoypadL_R) ? 1 : -1);
    return true;
  }
  int dx = (h & kJoypadH_Right) ? 1 : (h & kJoypadH_Left) ? -1 : 0;
  int dy = (h & kJoypadH_Down) ? 1 : (h & kJoypadH_Up) ? -1 : 0;
  // on a side arrow: the same way again (or A) changes page, the other way
  // goes back to the page
  if (g_arrow) {
    if (dx == g_arrow || (l & kJoypadL_A)) {
      SwitchPage(g_arrow);
    } else if (dx == -g_arrow) {
      g_arrow = 0;
      sound_effect_2 = 32;
    }
    return true;
  }
  if (g_page == kPageItems && FreeCursor()) {
    int cur = CursorSlot();
    // the button pressed takes the item under the cursor
    int btn = (h & kJoypadH_Y) ? 0 : Hud_ItemOnX() && (l & kJoypadL_X) ? 1 :
              Hud_ItemOnLR() && (l & kJoypadL_L) ? 2 : Hud_ItemOnLR() && (l & kJoypadL_R) ? 3 : -1;
    if (btn >= 0) {
      if (Hud_AssignItem(btn, Hud_MenuItemAt(cur)))
        sound_effect_2 = 32;
      return true;
    }
    // the arrows go to the nearest item Link has that way, as with Y alone,
    // without changing the buttons; with none that way, left and right go
    // on the side arrow
    int n = dx || dy ? ItemStep(cur, dx, dy) : -1;
    if (n >= 0) {
      g_icur = n;
      sound_effect_2 = 32;
    } else if (dx) {
      g_arrow = dx;
      sound_effect_2 = 32;
    }
    return true;
  }
  if (g_page == kPageItems) {
    // Y alone: the arrows put on Y the nearest item Link has that way (by
    // position, as on the Quest page); with none that way, left and right
    // go on the side arrow
    if (!dx && !dy)
      return false;
    int best = ItemStep(CursorSlot(), dx, dy);
    if (best >= 0) {
      hud_cur_item = Hud_MenuItemAt(best);
      timer_for_flashing_circle = 16;
      sound_effect_2 = 32;
    } else if (dx) {
      g_arrow = dx;
      sound_effect_2 = 32;
    }
    return true;
  }
  // Quest page: the arrows move the cursor; past the side, on the side arrow
  if (dx || dy) {
    int n = QuestStep(g_qcur, dx, dy);
    if (n >= 0) {
      g_qfrom = g_qcur, g_qcur = n;
      sound_effect_2 = 32;
    } else if (dx) {
      g_arrow = dx;
      sound_effect_2 = 32;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------

void HdInv_BeforeRender(Ppu *ppu) {
  g_tm_saved = false;
  bool on = HdInv_Enabled();
  if (on && !g_was_open)
    g_arrow = 0, g_slide = 0, g_pop = 0;  // the menu opens: cursor on the page
  if (on && !g_was_open) {
    // the free cursor starts on the item on Y
    g_icur = Hud_MenuSlotOf(hud_cur_item);
    if (g_icur < 0)
      g_icur = 0;
  }
  g_was_open = on;
  if (!on)
    return;
  // the original menu (and the classic HUD, in the same layer) is not drawn
  g_saved_tm[0] = ppu->screenEnabled[0];
  g_saved_tm[1] = ppu->screenEnabled[1];
  ppu->screenEnabled[0] &= ~4;
  ppu->screenEnabled[1] &= ~4;
  g_tm_saved = true;
}

void HdInv_AfterRender(Ppu *ppu) {
  if (!g_tm_saved)
    return;
  ppu->screenEnabled[0] = g_saved_tm[0];
  ppu->screenEnabled[1] = g_saved_tm[1];
  g_tm_saved = false;
}
