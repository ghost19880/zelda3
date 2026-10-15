// HUD moderno (opzione ModernHud).
//
// L'HUD originale e' una piccola mappa di caselle dello strato 3 che il gioco
// carica in memoria video: durante il gioco le caselle che coincidono ancora
// con l'HUD vengono svuotate (quelle che un dialogo ci ha scritto sopra
// restano), e i pezzi vengono ridisegnati qui con la loro grafica originale
// (cuori, rupie, chiavi, cifre, icone degli oggetti) in una nuova disposizione.
// I pixel sono marcati come HUD (alpha 2): lo shader non li tocca.
#include <math.h>
#include <string.h>
#include "hd_hud.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "features.h"
#include "config.h"
#include "hud.h"
#include "snes/ppu.h"
#include "player.h"

const uint16 *Hud_GetItemIconTiles(int item);


enum {
  kAction_None, kAction_Talk, kAction_Read, kAction_Open, kAction_Lift,
  kAction_Grab, kAction_Pull, kAction_Throw, kAction_Dash, kAction_Swim,
};

static bool g_hud_was_active;
static uint8 g_talk_frame = 0xff;  // ultimo frame in cui si poteva parlare

static bool ModernHudOn(void) {
  return (enhanced_features0 & kFeatures0_ModernHud) != 0;
}

// Durante il gioco e nei dialoghi (non nei menu, sulla mappa o nei titoli)
static bool HudContext(bool *in_dialog) {
  int mod = main_module_index;
  *in_dialog = false;
  if (mod == 7 || mod == 9)
    return true;
  if (mod == 14 && submodule_index == 2 && (saved_module_for_menu == 7 || saved_module_for_menu == 9)) {
    *in_dialog = true;
    return true;
  }
  return false;
}

void HdHud_BeforeRender(Ppu *ppu) {
  bool dlg;
  bool active = ModernHudOn() && HudContext(&dlg);
  uint16 *v = &ppu->vram[word_7E0219 & 0x7fff];
  if (active) {
    for (int i = 0; i < 165 && (word_7E0219 & 0x7fff) + i < 0x8000; i++)
      if (hud_tile_indices_buffer[i] != 0x207f && v[i] == hud_tile_indices_buffer[i])
        v[i] = 0x207f;
  } else if (g_hud_was_active && !ModernHudOn()) {
    flag_update_hud_in_nmi = 1;  // opzione appena spenta: il gioco ricarica l'HUD originale
  }
  g_hud_was_active = active;
}

void HdHud_CanTalk(void) {
  g_talk_frame = frame_counter;
}

// ---------------------------------------------------------------------------
// Azione del tasto A, calcolata senza cambiare lo stato del gioco

static int ComputeAction(void) {
  if (main_module_index != 7 && main_module_index != 9)
    return kAction_None;
  if (link_player_handler_state == kPlayerState_Swimming)
    return link_item_flippers ? kAction_Swim : kAction_None;
  if (link_state_bits & 0x80)
    return kAction_Throw;  // tiene qualcosa sopra la testa
  if (link_grabbing_wall)
    return kAction_Pull;
  if (link_item_in_hand || (link_position_mode & 0x1f) || link_incapacitated_timer)
    return kAction_None;
  if (g_talk_frame == frame_counter || (uint8)(frame_counter - g_talk_frame) == 1)
    return kAction_Talk;
  if (link_need_for_pullforrupees_sprite && !link_direction_facing)
    return kAction_Pull;
  if (link_is_near_moveable_statue)
    return kAction_Grab;
  if (flag_is_sprite_to_pick_up || flag_is_ancilla_to_pick_up)
    return (link_ability_flags & 0x40) ? kAction_Lift : kAction_None;

  // Stessa scelta che fa il gioco alla pressione di A, su una copia della RAM
  static uint8 saved[0x20000];
  memcpy(saved, g_ram, sizeof(saved));
  uint8 action = Link_HandleLiftables();
  memcpy(g_ram, saved, sizeof(saved));
  static const uint8 kAbility[] = { 0xE0, 0x40, 4, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0 };
  if (action > 7 || !(kAbility[action] & link_ability_flags))
    return kAction_None;
  switch (action) {
  case 0: return kAction_Read;   // preghiera davanti alla tavoletta
  case 1: return kAction_Lift;
  case 2: return link_item_boots ? kAction_Dash : kAction_None;
  case 3: return kAction_Grab;
  case 4: return kAction_Read;
  case 5: return kAction_Open;
  }
  return kAction_None;
}

static const char *ActionLabel(int a) {
  static const char *const kIt[] = { "", "PARLA", "LEGGI", "APRI", "SOLLEVA", "AFFERRA", "TIRA", "LANCIA", "CORRI", "NUOTA" };
  static const char *const kEn[] = { "", "TALK", "READ", "OPEN", "LIFT", "GRAB", "PULL", "THROW", "DASH", "SWIM" };
  bool it = g_config.language && (g_config.language[0] == 'i' || g_config.language[0] == 'I');
  return (it ? kIt : kEn)[a];
}

// ---------------------------------------------------------------------------
// Disegno

typedef struct Canvas {
  uint8 *pixels;
  int pitch, width, height, scale;
} Canvas;

static void Px(Canvas *cv, int x, int y, uint32 color) {
  if ((unsigned)x >= (unsigned)cv->width || (unsigned)y >= (unsigned)cv->height)
    return;
  int s = cv->scale;
  for (int yy = 0; yy < s; yy++) {
    uint32 *row = (uint32 *)(cv->pixels + (y * s + yy) * cv->pitch);
    for (int xx = 0; xx < s; xx++)
      row[x * s + xx] = (color & 0xffffff) | 0x02000000;  // alpha 2 = HUD
  }
}

static uint32 Bgr15ToRgb(uint16 c) {
  uint32 r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;
  return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

// Una casella 8x8 dello strato 3 (2 bit per pixel) come la disegnerebbe la PPU
static void DrawTile(Canvas *cv, int x, int y, uint16 entry) {
  Ppu *ppu = g_zenv.ppu;
  int tile = entry & 0x3ff, pal = entry >> 10 & 7;
  bool hf = entry & 0x4000, vf = entry & 0x8000;
  uint16 base = ppu->bgLayer[2].tileAdr + tile * 8;
  for (int r = 0; r < 8; r++) {
    uint16 w = ppu->vram[(base + (vf ? 7 - r : r)) & 0x7fff];
    for (int c = 0; c < 8; c++) {
      int bit = hf ? c : 7 - c;
      int v = (w >> bit & 1) | (w >> (8 + bit) & 1) << 1;
      if (v)
        Px(cv, x + c, y + r, Bgr15ToRgb(ppu->cgram[pal * 4 + v]));
    }
  }
}

// Una casella 8x8 decodificata nei colori (0 = trasparente)
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
      out[r * stride + c] = v ? (Bgr15ToRgb(ppu->cgram[pal * 4 + v]) | 0xff000000) : 0;
    }
  }
}

// Icona 16x16 di un oggetto (caselle: su-sx, su-dx, giu'-sx, giu'-dx). Lo
// sfondo nero del riquadro dell'HUD diventa trasparente (solo il nero
// collegato al bordo: i contorni interni restano). skip_br salta l'ultima
// casella (nella spada contiene il numero del livello).
static void DrawIcon(Canvas *cv, int x, int y, const uint16 *t, bool skip_br) {
  uint32 img[16 * 16];
  DecodeTile(t[0], img, 16);
  DecodeTile(t[1], img + 8, 16);
  DecodeTile(t[2], img + 8 * 16, 16);
  DecodeTile(t[3], img + 8 * 16 + 8, 16);
  if (skip_br)
    for (int r = 8; r < 16; r++)
      for (int c = 8; c < 16; c++)
        img[r * 16 + c] = 0;
  // riempimento dal bordo sui pixel quasi neri
  uint8 bg[16 * 16] = { 0 };
  int stack[16 * 16], n = 0;
  for (int i = 0; i < 16; i++) {
    stack[n++] = i, stack[n++] = 15 * 16 + i;
    stack[n++] = i * 16, stack[n++] = i * 16 + 15;
  }
  while (n) {
    int i = stack[--n];
    if (bg[i])
      continue;
    uint32 c = img[i];
    bool dark = c == 0 || ((c >> 16 & 0xff) + (c >> 8 & 0xff) + (c & 0xff)) < 72;
    if (!dark)
      continue;
    bg[i] = 1;
    int r = i / 16, col = i % 16;
    if (r > 0) stack[n++] = i - 16;
    if (r < 15) stack[n++] = i + 16;
    if (col > 0) stack[n++] = i - 1;
    if (col < 15) stack[n++] = i + 1;
  }
  for (int i = 0; i < 256; i++)
    if (!bg[i] && img[i])
      Px(cv, x + i % 16, y + i / 16, img[i]);
}

// Font 5x7 per le scritte e i numeri piccoli
static const uint8 *Glyph(char c) {
  static const uint8 kLetters[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},      {7,2,2,2,2,18,12},      {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30},   {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},  {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4},     {31,1,2,4,8,16,31},
  };
  static const uint8 kDigits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14},
    {2,6,10,18,31,2,2}, {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
  };
  if (c >= 'A' && c <= 'Z') return kLetters[c - 'A'];
  if (c >= '0' && c <= '9') return kDigits[c - '0'];
  static const uint8 kSlash[7] = { 1, 1, 2, 4, 8, 16, 16 };
  if (c == '/') return kSlash;
  return NULL;
}

static int SmallTextWidth(const char *s) {
  int n = (int)strlen(s);
  return n ? n * 6 - 1 : 0;
}

// Testo bianco con contorno scuro
static void DrawSmallText(Canvas *cv, int x, int y, const char *s, uint32 color) {
  for (int pass = 0; pass < 2; pass++) {
    int cx = x;
    for (const char *p = s; *p; p++, cx += 6) {
      const uint8 *g = Glyph(*p);
      if (!g)
        continue;
      for (int r = 0; r < 7; r++)
        for (int c = 0; c < 5; c++) {
          if (!(g[r] >> (4 - c) & 1))
            continue;
          if (pass == 0) {
            for (int dy = -1; dy <= 1; dy++)
              for (int dx = -1; dx <= 1; dx++)
                Px(cv, cx + c + dx, y + r + dy, 0x101010);
          } else {
            Px(cv, cx + c, y + r, color);
          }
        }
    }
  }
}

// Tasto del controller in pixel art: colore pieno, luce in alto a sinistra,
// ombra in basso a destra, bordo scuro
typedef struct ButtonColors {
  uint32 light, base, dark, rim;
} ButtonColors;

static const ButtonColors kButtonY = { 0x5ccf74, 0x2a9a46, 0x1b6a30, 0x0c2a14 };  // verde
static const ButtonColors kButtonA = { 0xe0525c, 0xb01c28, 0x7a1018, 0x2c080a };  // rosso
static const ButtonColors kButtonB = { 0xf0dc78, 0xc8aa36, 0x8e7420, 0x302808 };  // giallo

static void DrawButton(Canvas *cv, int cx, int cy, int r, const ButtonColors *bc) {
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      float d = sqrtf((float)(dx * dx + dy * dy));
      if (d > r + 0.4f)
        continue;
      uint32 col;
      if (d > r - 1.0f) {
        col = bc->rim;
      } else {
        float t = (dx + dy) / (2.0f * r);
        col = t < -0.42f ? bc->light : t > 0.38f ? bc->dark : bc->base;
      }
      Px(cv, cx + dx, cy + dy, col);
    }
  // riflesso
  Px(cv, cx - r / 2, cy - r / 2, 0xffffff);
  Px(cv, cx - r / 2 + 1, cy - r / 2, bc->light);
  Px(cv, cx - r / 2, cy - r / 2 + 1, bc->light);
}

// Spada in pixel art (le caselle dell'icona del menu durante il gioco non
// sono tutte caricate): lama, guardia e impugnatura con i colori del livello
static void DrawSword(Canvas *cv, int x, int y, int level) {
  static const char *const kSword[16] = {
    "..............o.", ".............obo", "............obBo", "...........obBo.",
    "..........obBo..", ".........obBo...", "........obBo....", ".......obBo.....",
    "..oo..obBo......", "..ogoobBo.......", "..oggBBo........", "...oggo.........",
    "..ohoggo........", ".oho.ogo........", "opo...oo........", "oo..............",
  };
  //                          lama chiara lama scura guardia   impugnatura pomo
  static const uint32 kColors[5][5] = {
    { 0xe8e8f0, 0x98a0b8, 0x8c6c3c, 0x7a4a20, 0xb89060 },  // (nessuna)
    { 0xe8e8f0, 0x98a0b8, 0x8c6c3c, 0x7a4a20, 0xb89060 },  // spada del guerriero
    { 0xf0f4ff, 0x90a8d8, 0x3858d0, 0x6a3a1a, 0xd8b040 },  // Spada Suprema
    { 0xffc8c0, 0xd05848, 0x3858d0, 0x6a3a1a, 0xd8b040 },  // spada temprata
    { 0xfff0a0, 0xd0a020, 0xe0b830, 0x6a3a1a, 0xf8e070 },  // spada d'oro
  };
  const uint32 *k = kColors[level > 4 ? 4 : level];
  for (int r = 0; r < 16; r++)
    for (int c = 0; c < 16; c++) {
      uint32 col;
      switch (kSword[r][c]) {
      case 'o': col = 0x101010; break;
      case 'b': col = k[0]; break;
      case 'B': col = k[1]; break;
      case 'g': col = k[2]; break;
      case 'h': col = k[3]; break;
      case 'p': col = k[4]; break;
      default: continue;
      }
      Px(cv, x + c, y + r, col);
    }
}

static void DrawNumberTiles(Canvas *cv, int x, int y, int value, int digits) {
  char buf[8];
  int n = 0;
  do {
    buf[n++] = value % 10;
    value /= 10;
  } while (value && n < 7);
  while (n < digits)
    buf[n++] = 0;
  for (int i = n - 1; i >= 0; i--, x += 8)
    DrawTile(cv, x, y, 0x2490 + buf[i]);
}

void HdHud_Draw(uint8 *pixels, int pitch, int width, int height, int scale) {
  bool in_dialog;
  if (!ModernHudOn() || !HudContext(&in_dialog))
    return;
  Ppu *ppu = g_zenv.ppu;
  if (!(ppu->screenEnabled[0] & 4) || ppu->mode == 7)
    return;  // il gioco stesso non mostra l'HUD (scene, mappa...)
  Canvas cv = { pixels, pitch, width / scale, height / scale, scale };
  int W = cv.width, H = cv.height;

  // --- Cuori (10 per riga) ---------------------------------------------
  int hearts = link_health_capacity >> 3;
  int hp = (link_health_current + 3) & ~3;
  int x0 = 8, y0 = 7;
  for (int i = 0; i < hearts; i++) {
    int left = hp - i * 8;
    uint16 t = left >= 8 ? 0x24A0 : left >= 4 ? 0x24A1 : 0x24A2;
    DrawTile(&cv, x0 + (i % 10) * 8, y0 + (i / 10) * 8, t);
  }
  int rows = hearts > 10 ? 2 : 1;

  // --- Magia: barra orizzontale sottile -----------------------------------
  {
    int bx = x0, by = y0 + rows * 8 + 2, bw = 80, bh = 3;
    int fill = (link_magic_power * bw + 0x7f) / 0x80;
    if (fill > bw) fill = bw;
    for (int x = bx - 1; x <= bx + bw; x++)
      for (int y = by - 1; y <= by + bh; y++) {
        bool edge = x == bx - 1 || x == bx + bw || y == by - 1 || y == by + bh;
        uint32 col = edge ? 0x101010 : (x - bx < fill ? (y == by ? 0x88f080 : 0x30b838) : 0x203020);
        Px(&cv, x, y, col);
      }
    if (link_magic_consumption >= 1)
      DrawSmallText(&cv, bx + bw + 4, by - 2, link_magic_consumption >= 2 ? "1/4" : "1/2", 0xf8f8f8);
  }

  // --- Tasti a destra: Y e A in alto, B sotto in mezzo ---------------------
  int R = 13;
  int ax = W - 20, ay = 30;
  int yx = W - 54, yy = 30;
  int bx = W - 37, by = 55;

  DrawButton(&cv, yx, yy, R, &kButtonY);
  DrawButton(&cv, ax, ay, R, &kButtonA);
  DrawButton(&cv, bx, by, R, &kButtonB);

  // Y: oggetto selezionato, con il numero se e' un consumabile
  if (hud_cur_item) {
    DrawIcon(&cv, yx - 8, yy - 8, Hud_GetItemIconTiles(hud_cur_item), false);
    int count = -1;
    if (hud_cur_item == 1 && link_item_bow)
      count = link_num_arrows;
    else if (hud_cur_item == kHudItem_Bombs)
      count = link_item_bombs;
    if (count >= 0) {
      char buf[4];
      int n = count > 99 ? 99 : count;
      buf[0] = '0' + n / 10, buf[1] = '0' + n % 10, buf[2] = 0;
      const char *s = n < 10 ? buf + 1 : buf;
      DrawSmallText(&cv, yx + R - SmallTextWidth(s) + 2, yy + 4, s, 0xf8f8f8);
    }
  }
  // B: spada
  if (link_sword_type && link_sword_type != 0xff)
    DrawSword(&cv, bx - 8, by - 8, link_sword_type);
  // A: scritta dell'azione sopra il tasto
  if (!in_dialog) {
    const char *label = ActionLabel(ComputeAction());
    if (*label) {
      int w = SmallTextWidth(label);
      int lx = ax - w / 2;
      if (lx + w > W - 3) lx = W - 3 - w;
      DrawSmallText(&cv, lx, ay - R - 10, label, 0xf8f8f8);
    }
  }

  // --- Rupie in basso a sinistra, chiavi del labirinto sopra ---------------
  if (!in_dialog) {
    int ry = H - 16;
    DrawTile(&cv, 8, ry, 0x3ca8);
    DrawNumberTiles(&cv, 18, ry, link_rupees_actual, 3);
    if (link_num_keys != 0xff) {
      DrawTile(&cv, 8, ry - 11, 0x2871);
      DrawNumberTiles(&cv, 18, ry - 11, link_num_keys, 1);
    }
  }
}
