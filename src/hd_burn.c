// Fuoco e ghiaccio su cespugli ed erba nel mondo esterno ("Interazione
// bruciatura e congelamento").
// Fuoco: la Lanterna e la Bacchetta del Fuoco danno fuoco a cespugli ed erba
// alti. Ogni tile resta in fiamme per circa un secondo e intanto il fuoco
// passa, con un po' di ritardo casuale, a quelle vicine (anche in diagonale),
// cosi' un prato brucia in qualche secondo come in Four Swords Adventures. Il
// Medaglione della Fiamma brucia insieme tutto quello che si vede e da li' il
// fuoco si propaga come gli altri. Anche le bombe: esplodendo danno fuoco
// all'erba intorno e fanno esplodere quelle vicine, e il fuoco fa esplodere le
// bombe che tocca (HdBurn_Bombs). Quando si spegne il cespuglio si comporta come quello tagliato:
// sparisce, lascia i suoi resti e scopre cio' che nascondeva; il terreno resta
// annerito anche lasciando l'area e tornandoci, finche' il gioco non fa
// ricrescere il cespuglio. Le fiamme feriscono Link e i nemici che le toccano.
// Ghiaccio: la Bacchetta del Ghiaccio congela i cespugli e l'erba sulla sua
// linea, il Medaglione dell'Etere tutti quelli che si vedono; il ghiaccio non
// si propaga. Il cespuglio congelato resta finche' non viene tagliato o
// sollevato: allora va in pezzi come un vaso, in blu. Il fuoco scioglie il
// ghiaccio (la tile non brucia) e il ghiaccio spegne il fuoco.
// Anche l'acqua (alta e bassa) si congela: una lastra per tile sotto il centro
// del colpo della bacchetta (una fila larga tre tile), con l'Etere tutta quella
// che si vede. Diventa ghiaccio scivoloso su cui si cammina
// (Overworld_GetTileAttributeAtLocation la fa 0x0f) e torna acqua dopo 7
// secondi (bacchetta) o 14 (Etere), o subito col fuoco. Cespugli ed erba
// congelati invece restano, come i nemici congelati del gioco.
// Indoors (houses and caves, not the dungeons) only the water freezes: the
// game's water and the water that is only drawn (the tubs in the houses). The
// frozen cells become ice (0x0f) in the room's attribute table, as the game
// does when it changes a tile, so Link, enemies and objects all see ice; the
// water comes back when the ice melts, and the list starts again in each room.
// Fiamme, fumo, annerimento e ghiaccio sono disegnati da hd_fx.c.
#include <stdlib.h>
#include <string.h>
#include "hd_burn.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "features.h"
#include "overworld.h"
#include "sprite.h"
#include "misc.h"
#include "hd_world.h"
#include "hd_water.h"
#include "ancilla.h"
#include "hd_fx.h"
#include "snes/ppu.h"

extern int g_render_bias_x, g_render_bias_y;

enum {
  kMaxPending = 256,
  kMaxScorched = 2048,   // tile annerite ricordate (10 byte l'una)
  kMaxFrozen = 1024,     // tile congelate ricordate
  kMaxWet = 64,          // tile appena sciolte ricordate
  kWetTime = 120,        // frame in cui la tile appena sciolta non prende fuoco
  // The same speed for every fire that spreads (Fire Rod, Lantern, bombs,
  // Bombos): measured on FSA (the user's video, 13:26-13:35, a field of tall
  // grass set on fire by a bomb) the front runs about 3 tiles a second, a
  // tile every 20 frames or so; a tile burns about a second (kHdBurnTime)
  kSpreadAt = 12,        // frame in fiamme prima di passare il fuoco ai vicini...
  kSpreadAtRand = 8,     // ...piu' fino a questi, a caso
  kDelaySide = 2,        // ritardo per i vicini di lato (piu' fino a kDelayRand)
  kDelayDiagonal = 10,   // e per quelli in diagonale, che sono piu' lontani
  kDelayRand = 6,
  // Bombs ("Burn and freeze interaction"): a bomb going off sets the grass
  // within kBlastFire pixels of its middle on fire and sets off the bombs
  // within kBlastChain pixels after kChainDelay frames, while its first
  // kBlastStages stages last (Ancilla07_Bomb, about 20 frames); a bomb on a
  // burning tile goes off at once
  kBlastFire = 24, kBlastChain = 32, kChainDelay = 8, kBlastStages = 5,
  kHarmlessAt = kHdBurnTime - 16,  // nelle ultime fiammelle il fuoco non fa piu' male
  // Danno a Link: quello degli sprite di fuoco del gioco (fuoco di Kodongo,
  // barre di fuoco: classe 4 di kPlayerDamages), un cuore con ogni armatura
  kLinkDamage = 8,
  // Danno ai nemici: quello della Bacchetta del Fuoco (classe 11 di
  // kEnemyDamages), cosi' ogni nemico reagisce come al fuoco della bacchetta:
  // immune, ferito o in fiamme
  kFireRodAncilla = 2,
};

typedef struct Pending {
  uint16 x, y;
  uint8 timer;
  uint16 fire;  // the fire it comes from (HdBurn_NewFire)
} Pending;

static Pending g_pending[kMaxPending];
static int g_num_pending;
static HdBurnTile g_burning[kHdMaxBurning];
static uint8 g_spread_at[kHdMaxBurning];  // 0xff: non passa il fuoco ai vicini
static uint16 g_fire_of[kHdMaxBurning];   // the fire each burning tile belongs to
static uint16 g_fire_next;
static int g_num_burning;
static HdBurnTile g_scorched[kMaxScorched];
static int g_num_scorched;
static HdBurnTile g_frozen[kMaxFrozen];
static int g_num_frozen;
// Indice veloce delle tile congelate (un bit per tile, a meno di 64x64 tile):
// HdBurn_IsFrozenAt si chiama per ogni cella d'acqua a ogni frame
static uint8 g_frozen_hint[64 * 64 / 8];
static uint32 g_frozen_ver = 1, g_hint_ver;
// Tile appena sciolte dal fuoco: per un po' non bruciano, altrimenti la
// fiammata (che controlla la stessa tile per piu' frame) o le fiamme vicine le
// incendierebbero subito dopo
// Only the fire that melted the tile is kept off: a new fire (another use of
// the Lantern, another Fire Rod shot, a bomb, Bombos) sets it alight at once
static Pending g_wet[kMaxWet];
static int g_num_wet;
static uint16 g_burn_area = 0xffff;
static uint16 g_seen_area = 0xffff;  // per i segreti delle tile bruciate da lontano
static uint16 g_ice_area = 0xffff;   // area of the last look at the ice (ForgetIceOnAreaLoad)
static uint16 g_leaving_area = 0xffff;  // area left, still on the screen during the scroll
static uint8 g_sfx_timer;
static uint32 g_burn_rng = 0x2545f491;

// Numeri casuali propri: non toccano quelli del gioco
static int Rand(int n) {
  g_burn_rng = g_burn_rng * 1664525 + 1013904223;
  return (g_burn_rng >> 16) % n;
}

// Inside a house or a cave (not a dungeon): only water freezes there
static bool IndoorIce(void) {
  return player_is_indoors && (cur_palace_index_x2 & 0xff) == 0xff;
}

static bool Enabled(void) {
  return (!player_is_indoors || IndoorIce()) && (enhanced_features0 & kFeatures0_BurnAndFreeze);
}

// Indoors: water attribute each frozen cell had (0 = not changed), for the two
// layers of the room's attribute table (BG2 0-0xfff, BG1 0x1000-0x1fff), and
// the room they belong to
static uint8 g_in_orig[0x2000];
static int g_in_room = -1;

// Index of the cell of world pixel (x, y) in the room's attribute table
static inline int IndoorCell(uint16 x, uint16 y) {
  return (y & 0x1f8) << 3 | (x & 0x1f8) >> 3;
}

static uint8 IndoorAttr(int i) {
  return g_in_orig[i] ? g_in_orig[i] : dung_bg2_attr_table[i];
}

uint8 HdBurn_UnfrozenAttr(int i, uint8 a) {
  i &= 0x1fff;
  return a == 0x0f && g_in_orig[i] ? g_in_orig[i] : a;
}

// Turns the water cells of the tile into ice (on) or back into water (off)
static void IndoorSetIce(uint16 x, uint16 y, bool on) {
  if (!player_is_indoors || dungeon_room_index != g_in_room)
    return;
  for (int c = 0; c < 4; c++) {
    int i0 = IndoorCell(x + (c & 1) * 8, y + (c >> 1) * 8);
    for (int l = 0; l < 2; l++) {
      int i = i0 + l * 0x1000;
      uint8 a = dung_bg2_attr_table[i];
      if (on && !g_in_orig[i] && HdWater_IsWaterAttr(a)) {
        g_in_orig[i] = a;
        dung_bg2_attr_table[i] = 0x0f;
      } else if (!on && g_in_orig[i]) {
        if (a == 0x0f)
          dung_bg2_attr_table[i] = g_in_orig[i];
        g_in_orig[i] = 0;
      }
    }
  }
}

// All the frozen cells of the room back to water (restore) and the memory
// cleared
static void IndoorClear(bool restore) {
  if (restore && player_is_indoors && dungeon_room_index == g_in_room) {
    for (int i = 0; i < 0x2000; i++)
      if (g_in_orig[i] && dung_bg2_attr_table[i] == 0x0f)
        dung_bg2_attr_table[i] = g_in_orig[i];
  }
  memset(g_in_orig, 0, sizeof(g_in_orig));
}

// Cespuglio o erba alta: mappa16 che resta dopo e tipo dei resti
static bool Burnable(uint16 map16, uint16 *repl, uint8 *what) {
  if (map16 == 0x36) {
    *repl = 0xdc7, *what = 2;  // cespuglio
  } else if (map16 == 0x72a) {
    *repl = 0xdc8, *what = 4;  // cespuglio del Mondo delle Tenebre
  } else if (map16 == 0x37e) {
    *repl = 0xdc5, *what = 3;  // erba alta
  } else {
    return false;
  }
  return true;
}

// Posizione della tile nella mappa dell'area, -1 se e' fuori dall'area
static int TilePos(uint16 x, uint16 y) {
  uint16 dx = x - overworld_offset_base_x * 8, dy = y - overworld_offset_base_y;
  if (dx > (overworld_offset_mask_x * 8 | 0xf) || dy > (overworld_offset_mask_y | 0xf))
    return -1;
  return (dy & overworld_offset_mask_y) * 8 + ((x >> 3) - overworld_offset_base_x & overworld_offset_mask_x);
}

// Mondo unico: il fuoco passa da un'area all'altra e non si spegne quando
// Link cambia area (HdWorld_SeamlessOn)

// Mappa16 della tile, anche nelle aree vicine col mondo unico; -1 se non si sa
static int TileAt(uint16 x, uint16 y) {
  if (player_is_indoors)
    return -1;  // indoors only water freezes: no bushes or grass
  int pos = TilePos(x, y);
  if (pos >= 0)
    return overworld_tileattr[pos >> 1];
  return HdWorld_WorldTile(x, y);
}

// Area della tile (con il bit del mondo, come overworld_area_index)
static uint8 AreaOf(uint16 x, uint16 y) {
  if (player_is_indoors)
    return 0;  // one room at a time
  int head = TilePos(x, y) < 0 ? HdWorld_AreaAt(x, y) : -1;
  return head >= 0 ? (uint8)head : (uint8)overworld_area_index;
}

static bool IsBurnable(uint16 x, uint16 y) {
  int t = TileAt(x, y);
  uint16 repl;
  uint8 what;
  return t >= 0 && Burnable(t, &repl, &what);
}

uint8 HdBurn_WaterCells(uint16 x, uint16 y) {
  if (player_is_indoors) {
    uint8 mask = 0;
    for (int c = 0; c < 4; c++) {
      int i = IndoorCell(x + (c & 1) * 8, y + (c >> 1) * 8);
      if (HdWater_IsWaterAttr(IndoorAttr(i)) || HdWater_IsWaterAttr(IndoorAttr(i + 0x1000)))
        mask |= 1 << c;
    }
    return mask;
  }
  int t = TileAt(x, y);
  if (t < 0 || HdWater_IsSky(x, y))  // cielo del Monte della Morte
    return 0;
  const uint16 *m8 = GetMap16toMap8Table() + t * 4;
  const uint8 *attr = GetMap8toTileAttr();
  uint8 mask = 0;
  for (int i = 0; i < 4; i++) {
    int e = m8[i];
    // acqua alta o bassa, ma non le cascate (caselle 0x1ca-0x1cd, palette 7)
    // waterfalls as the HD water draws them, with the hidden entrance between
    // (only palette 7 can be a waterfall: the others need no look around)
    if (HdWater_IsWaterAttr(attr[e & 0x1ff]) &&
        ((e >> 10 & 7) != 7 || !HdWater_IsFallCellAt(x + (i & 1) * 8, y + (i >> 1) * 8)))
      mask |= 1 << i;
  }
  return mask;
}

// Cells of the tile that freeze: the game's water (HdBurn_WaterCells) and the
// water that is only drawn, which the HD water fills (village troughs: the
// tile is 0x126/0x127 over 0x136/0x137; the rocks in the water, whose cells
// are wall: the ice went round them and left the water of those cells). The
// ice covers only the pixels drawn as water of these cells (HdFx_FrozenWater),
// not the trough's rim or the rock
static uint8 FreezableCells(uint16 x, uint16 y) {
  uint8 mask = HdBurn_WaterCells(x, y);
  if (player_is_indoors) {
    // the blue tubs in the houses (HdWater_IsTubCell)
    for (int c = 0; c < 4; c++)
      if (HdWater_IsTubCell(IndoorCell(x + (c & 1) * 8, y + (c >> 1) * 8)))
        mask |= 1 << c;
    return mask;
  }
  int t = TileAt(x, y);
  if (t < 0)
    return mask;
  const uint16 *m8 = GetMap16toMap8Table() + t * 4;
  for (int i = 0; i < 2; i++)
    if (HdWater_IsTroughPair(m8[i], m8[i + 2]))
      mask |= 1 << i | 1 << (i + 2);
  for (int i = 0; i < 4; i++)
    if (HdWater_IsRockEntry(m8[i]))
      mask |= 1 << i;
  return mask;
}

bool HdBurn_TileShown(const HdBurnTile *t) {
  if (player_is_indoors)
    return true;  // the list holds only the room's tiles
  if (HdWorld_SeamlessOn())
    return !((t->area ^ overworld_area_index) & 0x40);
  // the area being left stays on the screen during the scroll: its ice and
  // burns stay with it until the scroll ends (ForgetIceOnAreaLoad)
  return t->area == (uint8)overworld_area_index || t->area == g_leaving_area;
}

// Keeps only the ice of the area being left (only_leaving), or all but it
static void DropFrozen(bool only_leaving) {
  int n = 0;
  for (int i = 0; i < g_num_frozen; i++)
    if ((g_frozen[i].area == g_leaving_area) == only_leaving)
      g_frozen[n++] = g_frozen[i];
  if (n != g_num_frozen)
    g_num_frozen = n, g_frozen_ver++;
}

// Without the continuous world the game loads every area anew: the bushes
// cut or burned grow again, and so the ice goes too, on bushes, grass and
// water. At the area change the ice of the area entered (from an earlier
// visit) goes at once, also during the scroll, when HdBurn_Update does not
// run (the drawing asks for the ice first); the ice of the area left goes
// when the scroll ends. With the continuous world the areas keep their
// changes (hd_world.c), the ice too
static void ForgetIceOnAreaLoad(void) {
  if (player_is_indoors)
    return;
  bool continuous = (enhanced_features0 & kFeatures0_ContinuousWorld) != 0;
  if (g_ice_area != overworld_area_index) {
    g_leaving_area = continuous || g_ice_area == 0xffff ? 0xffff : g_ice_area;
    if (!continuous)
      DropFrozen(true);
    g_ice_area = overworld_area_index;
  }
  if (g_leaving_area != 0xffff && HdFx_LiveOutdoors()) {
    DropFrozen(false);
    g_leaving_area = 0xffff;
  }
}

bool HdBurn_ScorchShown(const HdBurnTile *t) {
  return HdBurn_TileShown(t) && !IsBurnable(t->x, t->y);
}

// Spegne l'incendio in corso (le tile annerite restano)
static void ClearAll(void) {
  g_num_pending = g_num_burning = 0;
}

// Il fuoco resta nell'area in cui e' nato; col mondo unico si spegne solo
// passando all'altro mondo
static bool FireInOtherArea(void) {
  return HdWorld_SeamlessOn() ? ((g_burn_area ^ overworld_area_index) & 0x40) != 0 : g_burn_area != overworld_area_index;
}

// Aggiunge in fondo a una lista piena a max togliendo il piu' vecchio
#define PushDropOldest(arr, n, max, item) do {     if ((n) == (max))       memmove((arr), (arr) + 1, --(n) * sizeof((arr)[0]));     (arr)[(n)++] = (item);   } while (0)

// Cambia una tile dell'area caricata (attributi, mappa, schermo)
static void SetAreaTile(int pos, uint16 m16) {
  overworld_tileattr[pos >> 1] = m16;
  Overworld_Memorize_Map16_Change(pos, m16);
  Overworld_DrawMap16(pos, m16);
  nmi_load_bg_from_vram = 1;
}

static int FindPending(uint16 x, uint16 y) {
  for (int i = 0; i < g_num_pending; i++)
    if (g_pending[i].x == x && g_pending[i].y == y)
      return i;
  return -1;
}

static int FindBurning(uint16 x, uint16 y) {
  for (int i = 0; i < g_num_burning; i++)
    if (g_burning[i].x == x && g_burning[i].y == y)
      return i;
  return -1;
}

static inline int HintBit(uint16 x, uint16 y) {
  return (x >> 4 & 63) | (y >> 4 & 63) << 6;
}

static int FindFrozen(uint16 x, uint16 y) {
  if (g_hint_ver != g_frozen_ver) {
    g_hint_ver = g_frozen_ver;
    memset(g_frozen_hint, 0, sizeof(g_frozen_hint));
    for (int i = 0; i < g_num_frozen; i++) {
      int b = HintBit(g_frozen[i].x, g_frozen[i].y);
      g_frozen_hint[b >> 3] |= 1 << (b & 7);
    }
  }
  int b = HintBit(x, y);
  if (!(g_frozen_hint[b >> 3] & 1 << (b & 7)))
    return -1;
  int area = -1;  // AreaOf(x, y), computed at the first match
  for (int i = 0; i < g_num_frozen; i++) {
    if (g_frozen[i].x != x || g_frozen[i].y != y)
      continue;
    if (area < 0)
      area = AreaOf(x, y);
    if (g_frozen[i].area == area)
      return i;
  }
  return -1;
}

static void RemoveFrozen(int i) {
  g_frozen[i] = g_frozen[--g_num_frozen];
  g_frozen_ver++;
}

static void AddFrozen(HdBurnTile t) {
  PushDropOldest(g_frozen, g_num_frozen, kMaxFrozen, t);
  g_frozen_ver++;
}

// Tile bruciata: annerita (remote: in un'area vicina, segreto da scoprire)
static void AddScorched(HdBurnTile t) {
  PushDropOldest(g_scorched, g_num_scorched, kMaxScorched, t);
}

static bool IsWet(uint16 x, uint16 y, uint16 fire) {
  for (int i = 0; i < g_num_wet; i++)
    if (g_wet[i].x == x && g_wet[i].y == y && g_wet[i].fire == fire)
      return true;
  return false;
}

// Il fuoco scioglie il ghiaccio della tile: resta bagnata per kWetTime frame
static void Melt(int f, uint16 fire) {
  HdBurnTile t = g_frozen[f];
  RemoveFrozen(f);
  PushDropOldest(g_wet, g_num_wet, kMaxWet, ((Pending){ t.x, t.y, kWetTime, fire }));
}

static void Queue(uint16 x, uint16 y, uint8 timer, uint16 fire) {
  if (FindBurning(x, y) >= 0)
    return;
  if (FindPending(x, y) >= 0)
    return;
  if (g_num_pending < kMaxPending)
    g_pending[g_num_pending++] = (Pending){ x, y, timer, fire };
}

// It rains on the tile (its own area, not Link's: with the continuous world
// the fire can be in a neighbouring area): fire has no effect there at all,
// whatever starts it (Lantern, Fire Rod, Bombos, bombs, spreading flames),
// and it does not melt the ice either
bool HdBurn_RainAt(uint16 x, uint16 y) {
  return !player_is_indoors && HdFx_IsRainingInArea(AreaOf(x, y));
}

// La tile (gia' allineata a 16) prende fuoco; se e' congelata il ghiaccio si
// scioglie e la tile non brucia (ne' subito dopo, finche' e' bagnata)
static bool StartBurning(uint16 x, uint16 y, bool spread, uint16 fire) {
  if (!IsBurnable(x, y) || FindBurning(x, y) >= 0 || g_num_burning >= kHdMaxBurning || IsWet(x, y, fire) ||
      HdBurn_RainAt(x, y))
    return false;
  int f = FindFrozen(x, y);
  if (f >= 0) {
    Melt(f, fire);
    return false;
  }
  if (FireInOtherArea())
    ClearAll();
  g_burn_area = overworld_area_index;
  int i = g_num_burning++;
  g_burning[i] = (HdBurnTile){ x, y, 0, (uint8)Rand(256), AreaOf(x, y) };
  g_spread_at[i] = spread ? kSpreadAt + Rand(kSpreadAtRand) : 0xff;
  g_fire_of[i] = fire;
  if (g_sfx_timer == 0) {
    sound_effect_1 = CalculateSfxPan(x) | 0x2a;
    g_sfx_timer = 12;
  }
  return true;
}

static void Spread(uint16 x, uint16 y, uint16 fire) {
  for (int dy = -16; dy <= 16; dy += 16) {
    for (int dx = -16; dx <= 16; dx += 16) {
      if ((dx | dy) && IsBurnable(x + dx, y + dy))
        Queue(x + dx, y + dy, ((dx && dy) ? kDelayDiagonal : kDelaySide) + Rand(kDelayRand), fire);
    }
  }
}

// Riquadro delle fiamme della tile (come quello di uno sprite)
static void FlameHitBox(SpriteHitBox *hb, uint16 x, uint16 y) {
  x += 2, y += 2;
  hb->r4_spr_xlo = x, hb->r10_spr_xhi = x >> 8;
  hb->r5_spr_ylo = y, hb->r11_spr_yhi = y >> 8;
  hb->r6_spr_xsize = hb->r7_spr_ysize = 12;
}

bool g_hdburn_hurt_link;

// Link tocca le fiamme: si ferisce e rimbalza indietro come toccando uno
// sprite di fuoco (Sprite_AttemptDamageToLinkPlusRecoil)
static void HurtLink(uint16 x, uint16 y) {
  if (countdown_for_blink | link_disable_sprite_damage)
    return;
  SpriteHitBox hb;
  Link_SetupHitBox(&hb);
  FlameHitBox(&hb, x, y);
  if (!CheckIfHitBoxesOverlap(&hb))
    return;
  int dx = link_x_coord + 8 - (x + 8), dy = link_y_coord + 12 - (y + 8);
  int m = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
  if (m == 0)
    dy = m = 1;
  link_incapacitated_timer = 19;
  link_actual_vel_x = dx * 24 / m;
  link_actual_vel_y = dy * 24 / m;
  g_ram[0xc7] = link_actual_vel_z = 12;
  link_z_coord = 0;
  link_auxiliary_state = 1;
  link_give_damage = kLinkDamage;
  g_hdburn_hurt_link = true;
}

// I nemici nelle fiamme vengono colpiti come dal fuoco della Bacchetta
static void HurtSprites(uint16 x, uint16 y) {
  for (int j = 15; j >= 0; j--) {
    if (sprite_state[j] < 9 || sprite_ignore_projectile[j] || sprite_pause[j] || sprite_hit_timer[j])
      continue;
    SpriteHitBox hb;
    hb.r0_xlo = x + 2, hb.r8_xhi = (x + 2) >> 8;
    hb.r1_ylo = y + 2, hb.r9_yhi = (y + 2) >> 8;
    hb.r2 = hb.r3 = 12;
    Sprite_SetupHitBox(j, &hb);
    if (CheckIfHitBoxesOverlap(&hb))
      Ancilla_CheckDamageToSprite(j, kFireRodAncilla);
  }
}

// Mondo continuo, durante il passo della copia dell'area (HdWorld: CopyEffects):
// le fiamme delle sue caselle colpiscono anche i suoi nemici (HdBurn_Update
// gira solo nel gioco vero, dove ci sono soltanto i nemici dell'area di Link)
void HdBurn_HurtSpritesInArea(uint8 area) {
  if (!Enabled())
    return;
  for (int i = 0; i < g_num_burning; i++) {
    const HdBurnTile *b = &g_burning[i];
    if (b->area == area && b->age < kHarmlessAt)
      HurtSprites(b->x, b->y);
  }
}

// Il fuoco si spegne: resta il cespuglio bruciato
static void BurnOut(uint16 x, uint16 y) {
  int pos = TilePos(x, y);
  uint16 repl;
  uint8 what;
  if (pos < 0) {
    // tile di un'area vicina (mondo unico): cambia nel mondo persistente; il
    // segreto sotto il cespuglio si scopre quando Link entra nell'area
    int t = TileAt(x, y);
    if (t < 0 || !Burnable(t, &repl, &what) || !HdWorld_SetWorldTile(x, y, repl))
      return;
    BYTE(dung_secrets_unk1) = 0;
    Sprite_SpawnImmediatelySmashedTerrain(what, x, y);
    AddScorched((HdBurnTile){ x, y, 0, (uint8)Rand(256), AreaOf(x, y), 1 });
    return;
  }
  if (!Burnable(overworld_tileattr[pos >> 1], &repl, &what))
    return;
  uint16 secret = Overworld_RevealSecret(pos);
  if (secret != 0)
    repl = secret;
  SetAreaTile(pos, repl);
  Sprite_SpawnImmediatelySmashedTerrain(what, x, y);
  AddScorched((HdBurnTile){ x, y, 0, (uint8)Rand(256), (uint8)overworld_area_index });
}

// La tile (gia' allineata a 16) si congela; se bruciava il fuoco si spegne.
// Con water (kHdIceRod o kHdIceEther) anche (o solo, con only_water)
// l'acqua; il ghiaccio d'acqua gia' fatto torna nuovo se cosi' dura di piu'
static bool StartFreezing(uint16 x, uint16 y, uint8 water, bool only_water) {
  bool burnable = !only_water && IsBurnable(x, y);
  if (!burnable && !(water && FreezableCells(x, y)))
    return false;
  // under the rain the water freezes and melts again at once: it grows in
  // and then opens up in patches as when the fire takes it (kHdIceMelting)
  bool rain = water && HdBurn_RainAt(x, y);
  int f = FindFrozen(x, y);
  if (f >= 0) {
    HdBurnTile *t = &g_frozen[f];
    if (!t->water || !water)
      return false;
    if (rain)
      return true;
    HdBurnTile n = *t;
    n.water = water, n.age = 8;
    if (HdBurn_IceLeft(&n) > HdBurn_IceLeft(t))
      *t = n;
    return true;
  }
  if (!burnable) {
    AddFrozen((HdBurnTile){ x, y, 0, (uint8)Rand(256), AreaOf(x, y), 0, rain ? kHdIceMelting : water });
    IndoorSetIce(x, y, true);
    return true;
  }
  int b = FindBurning(x, y);
  if (b >= 0) {
    g_num_burning--;
    g_burning[b] = g_burning[g_num_burning];
    g_spread_at[b] = g_spread_at[g_num_burning];
  }
  int p = FindPending(x, y);
  if (p >= 0)
    g_pending[p] = g_pending[--g_num_pending];
  AddFrozen((HdBurnTile){ x, y, 0, (uint8)Rand(256), AreaOf(x, y) });
  return true;
}

// Riquadro del mondo che si vede sullo schermo, tile 16x16 comprese
static void VisibleTiles(uint16 *x0, uint16 *y0, int *w, int *h) {
  int x, y;
  HdWorld_ScreenRect(&x, &y, w, h);  // regola unica degli effetti sullo schermo
  *x0 = x & ~0xf, *y0 = y & ~0xf;
  *w += 16, *h += 16;
}

// Il fuoco scioglie l'acqua ghiacciata della tile (gia' allineata a 16): si
// apre a chiazze come a fine tempo, ma in quanto dura una fiamma
static void MeltWater(uint16 x, uint16 y) {
  int f = FindFrozen(x, y);
  if (f < 0 || !g_frozen[f].water || g_frozen[f].water == kHdIceMelting || HdBurn_RainAt(x, y))
    return;
  HdBurnTile n = g_frozen[f];
  n.water = kHdIceMelting, n.age = kHdMeltStart;
  // se stava gia' finendo da solo resta il tempo che gli mancava
  if (HdBurn_IceLeft(&n) < HdBurn_IceLeft(&g_frozen[f]))
    g_frozen[f] = n;
}

uint16 HdBurn_NewFire(void) {
  return ++g_fire_next ? g_fire_next : ++g_fire_next;  // 0 is never a fire
}

bool HdBurn_Ignite(uint16 x, uint16 y, bool spread, uint16 fire) {
  if (!Enabled())
    return false;
  MeltWater(x & ~0xf, y & ~0xf);  // (neither under the rain: HdBurn_RainAt)
  return StartBurning(x & ~0xf, y & ~0xf, spread, fire);
}

bool HdBurn_Freeze(uint16 x, uint16 y) {
  if (!Enabled())
    return false;
  return StartFreezing(x & ~0xf, y & ~0xf, 0, false);
}

void HdBurn_FreezeWater(uint16 x, uint16 y) {
  if (Enabled())
    StartFreezing(x & ~0xf, y & ~0xf, kHdIceRod, true);
}

void HdBurn_MeltWater(uint16 x, uint16 y) {
  if (Enabled())
    MeltWater(x & ~0xf, y & ~0xf);
}

void HdBurn_IgniteScreen(void) {
  if (!Enabled())
    return;
  uint16 x0, y0;
  int w, h;
  VisibleTiles(&x0, &y0, &w, &h);
  for (int y = 0; y < h; y += 16)
    for (int x = 0; x < w; x += 16)
      MeltWater(x0 + x, y0 + y);
  // all at once, and from there the fire goes on to the grass beyond the
  // view as any other fire (the user asked for it)
  uint16 fire = HdBurn_NewFire();
  for (int y = 0; y < h; y += 16)
    for (int x = 0; x < w; x += 16)
      StartBurning(x0 + x, y0 + y, true, fire);
}

// ---------------------------------------------------------------------------
// Bombs and fire ("Burn and freeze interaction"), as in FSA: a bomb going off
// sets off the bombs near it and sets the grass around it on fire; the fire
// in the grass sets off the bombs on it. Bombs: Link's (ancilla 7) and the
// enemies' (sprite 0x4a); Bombs_SetOff (ancilla.c) is the one rule that sets
// a bomb off, also for the ring of the charged hammer
static bool g_blast_seen_anc[kAncilla_Slots], g_blast_seen_spr[16];
static int g_blast_x, g_blast_y;

static bool NearBlast(int x, int y, void *ctx) {
  int dx = x - g_blast_x, dy = y - g_blast_y;
  return dx * dx + dy * dy <= kBlastChain * kBlastChain;
}

static bool OnFire(int x, int y, void *ctx) {
  int i = FindBurning(x & ~0xf, y & ~0xf);
  return i >= 0 && g_burning[i].age < kHarmlessAt;
}

// A bomb going off with its middle at (x, y); first: its first frame
static void Blast(int x, int y, bool first) {
  if (first && Enabled()) {
    uint16 fire = HdBurn_NewFire();
    for (int dy = -32; dy <= 32; dy += 16)
      for (int dx = -32; dx <= 32; dx += 16) {
        int tx = (x + dx) & ~0xf, ty = (y + dy) & ~0xf;
        int ex = tx + 8 - x, ey = ty + 8 - y;
        if (ex * ex + ey * ey <= kBlastFire * kBlastFire)
          HdBurn_Ignite((uint16)tx, (uint16)ty, true, fire);
      }
  }
  g_blast_x = x, g_blast_y = y;
  Bombs_SetOff(NearBlast, NULL, kChainDelay);
}

void HdBurn_Bombs(void) {
  if (!(enhanced_features0 & kFeatures0_BurnAndFreeze) || submodule_index)
    return;
  for (int k = 0; k < kAncilla_Slots; k++) {
    bool on = ancilla_type[k] == 7 && ancilla_item_to_link[k] != 0;
    if (on && ancilla_item_to_link[k] <= kBlastStages)
      Blast(Ancilla_GetX(k), Ancilla_GetY(k), !g_blast_seen_anc[k]);  // its middle
    g_blast_seen_anc[k] = on;
  }
  for (int k = 0; k < 16; k++) {
    // SpriteBomb_ExplosionIncoming: sprite_C 2 from the blast, 31 frames
    bool on = sprite_state[k] && sprite_type[k] == 0x4a && sprite_C[k] >= 2;
    if (on && sprite_delay_aux1[k] >= 31 - 4 * kBlastStages)
      Blast(Sprite_GetX(k) + 8, Sprite_GetY(k) + 8, !g_blast_seen_spr[k]);
    g_blast_seen_spr[k] = on;
  }
  if (g_num_burning && Enabled())
    Bombs_SetOff(OnFire, NULL, 1);
}

void HdBurn_FreezeScreen(bool water) {
  if (!Enabled())
    return;
  uint16 x0, y0;
  int w, h;
  VisibleTiles(&x0, &y0, &w, &h);
  for (int y = 0; y < h; y += 16)
    for (int x = 0; x < w; x += 16)
      StartFreezing(x0 + x, y0 + y, water ? kHdIceEther : 0, false);
}

// Il cespuglio o l'erba appena tagliati o sollevati nel punto (x, y) (angolo
// dei resti) erano congelati? Se si' li dimentica: vanno in pezzi di ghiaccio
bool HdBurn_TakeFrozen(uint16 x, uint16 y) {
  if (player_is_indoors)
    return false;
  for (int t = 0; t < 2; t++) {
    uint16 tx = (x + t * 8) & ~0xf, ty = (y + t * 8) & ~0xf;
    int i = FindFrozen(tx, ty);
    if (i >= 0 && !g_frozen[i].water && !IsBurnable(tx, ty)) {
      RemoveFrozen(i);
      return true;
    }
  }
  return false;
}

bool HdBurn_IsFrozenAt(uint16 x, uint16 y) {
  ForgetIceOnAreaLoad();
  if ((player_is_indoors && !IndoorIce()) || g_num_frozen == 0)
    return false;
  x &= ~0xf, y &= ~0xf;
  int f = FindFrozen(x, y);
  return f >= 0 && (g_frozen[f].water || IsBurnable(x, y));
}

static void ResetAll(bool restore) {
  IndoorClear(restore);
  ClearAll();
  g_num_scorched = 0;
  g_num_frozen = 0;
  g_frozen_ver++;
  g_num_wet = 0;
  g_leaving_area = 0xffff;
}

void HdBurn_Reset(void) {
  ResetAll(true);
}

// Before loading a game: the water frozen indoors is already in the loaded
// RAM as it was, it must not be turned back into water there
void HdBurn_Forget(void) {
  ResetAll(false);
}

// Fires, burnt and frozen tiles go with the game in quick saves and in the
// rewind (zelda_rtl.c HdState_IO); the same calls save and load
void HdBurn_SaveLoad(SaveLoadFunc *func, void *ctx) {
#define F(v) func(ctx, &v, sizeof(v));
  F(g_pending) F(g_num_pending) F(g_burning) F(g_spread_at) F(g_fire_of) F(g_fire_next)
  F(g_num_burning) F(g_scorched) F(g_num_scorched) F(g_frozen) F(g_num_frozen)
  F(g_wet) F(g_num_wet) F(g_burn_area) F(g_seen_area) F(g_ice_area) F(g_leaving_area)
  F(g_sfx_timer) F(g_burn_rng) F(g_in_orig) F(g_in_room)
#undef F
}

// After HdBurn_SaveLoad has loaded: the index of the frozen tiles is rebuilt
void HdBurn_Loaded(void) {
  g_frozen_ver++;
}

// Link e' entrato in un'area dove il fuoco era arrivato da un'area vicina:
// sotto i cespugli bruciati compare quello che nascondevano (buche, scale),
// come quando bruciano nell'area di Link
static void RevealRemoteSecrets(void) {
  for (int i = 0; i < g_num_scorched; i++) {
    HdBurnTile *s = &g_scorched[i];
    if (!s->remote || s->area != (uint8)overworld_area_index)
      continue;
    s->remote = 0;
    int pos = TilePos(s->x, s->y);
    if (pos < 0 || IsBurnable(s->x, s->y))
      continue;
    uint16 secret = Overworld_RevealSecret(pos);
    BYTE(dung_secrets_unk1) = 0;
    if (secret == 0)
      continue;
    SetAreaTile(pos, secret);
  }
}

void HdBurn_Update(void) {
  if (player_is_indoors) {
    // a new room: its attribute table is loaded again, nothing to restore
    if (dungeon_room_index != g_in_room) {
      IndoorClear(false);
      HdBurn_Reset();
      g_in_room = dungeon_room_index;
    }
    if (!IndoorIce()) {
      HdBurn_Reset();
      return;
    }
    if (main_module_index != 7 || submodule_index != 0)
      return;  // the ice waits during menus and room changes
  } else {
    if (g_in_room >= 0) {
      IndoorClear(false);
      g_in_room = -1;
    }
    ForgetIceOnAreaLoad();
    // Il fuoco si ferma durante menu e transizioni
    if (!HdFx_LiveOutdoors())
      return;
  }
  if (!player_is_indoors && g_seen_area != overworld_area_index) {
    g_seen_area = overworld_area_index;
    RevealRemoteSecrets();
  }
  // Terreno annerito dell'area: si fa definitivo e sparisce solo se il
  // cespuglio e' ricresciuto (il gioco li fa ricrescere rientrando nell'area)
  int n = 0;
  for (int i = 0; i < g_num_scorched; i++) {
    HdBurnTile s = g_scorched[i];
    if (HdBurn_TileShown(&s)) {
      if (IsBurnable(s.x, s.y))
        continue;
      if (s.age < kHdScorchTime)
        s.age++;
    }
    g_scorched[n++] = s;
  }
  g_num_scorched = n;
  // Ghiaccio dell'area: sparisce se il cespuglio non c'e' piu'; quello
  // dell'acqua si scioglie da solo (HdBurn_IceLeft)
  n = 0;
  for (int i = 0; i < g_num_frozen; i++) {
    HdBurnTile s = g_frozen[i];
    if (HdBurn_TileShown(&s)) {
      if (s.water ? HdBurn_IceLeft(&s) <= 0 || !FreezableCells(s.x, s.y) : !IsBurnable(s.x, s.y)) {
        if (s.water)
          IndoorSetIce(s.x, s.y, false);
        continue;
      }
      if (s.age < 0xffff)
        s.age++;
    }
    g_frozen[n++] = s;
  }
  if (n != g_num_frozen)
    g_frozen_ver++;
  g_num_frozen = n;
  // Tile sciolte: si asciugano
  n = 0;
  for (int i = 0; i < g_num_wet; i++)
    if (--g_wet[i].timer)
      g_wet[n++] = g_wet[i];
  g_num_wet = n;
  if (g_num_pending + g_num_burning == 0)
    return;
  // Il fuoco resta nell'area in cui e' nato (col mondo unico: nel suo mondo)
  if (FireInOtherArea()) {
    ClearAll();
    return;
  }
  if (g_sfx_timer)
    g_sfx_timer--;

  // Tile in attesa: prendono fuoco quando scade il loro ritardo
  n = 0;
  Pending ready[kMaxPending];
  int num_ready = 0;
  for (int i = 0; i < g_num_pending; i++) {
    Pending p = g_pending[i];
    if (p.timer > 1) {
      p.timer--;
      g_pending[n++] = p;
    } else {
      ready[num_ready++] = p;
    }
  }
  g_num_pending = n;
  for (int i = 0; i < num_ready; i++)
    StartBurning(ready[i].x, ready[i].y, true, ready[i].fire);

  // Tile in fiamme: passano il fuoco ai vicini, poi si spengono
  n = 0;
  int num_out = 0;
  static HdBurnTile out[kHdMaxBurning], spread[kHdMaxBurning];
  static uint16 spread_fire[kHdMaxBurning];
  int num_spread = 0;
  for (int i = 0; i < g_num_burning; i++) {
    HdBurnTile b = g_burning[i];
    uint8 spread_at = g_spread_at[i];
    uint16 fire = g_fire_of[i];
    if (!IsBurnable(b.x, b.y))
      continue;  // tagliato o sollevato mentre bruciava: le fiamme spariscono
    b.age++;
    if (b.age < kHarmlessAt) {
      HurtLink(b.x, b.y);
      HurtSprites(b.x, b.y);
    }
    if (b.age == spread_at)
      spread_fire[num_spread] = fire, spread[num_spread++] = b;
    if (b.age >= kHdBurnTime) {
      out[num_out++] = b;
      continue;
    }
    g_spread_at[n] = spread_at;
    g_fire_of[n] = fire;
    g_burning[n++] = b;
  }
  g_num_burning = n;
  for (int i = 0; i < num_spread; i++)
    Spread(spread[i].x, spread[i].y, spread_fire[i]);
  for (int i = 0; i < num_out; i++)
    BurnOut(out[i].x, out[i].y);
}

int HdBurn_GetBurning(const HdBurnTile **tiles) {
  *tiles = g_burning;
  if (player_is_indoors)
    return 0;
  return FireInOtherArea() ? 0 : g_num_burning;
}

int HdBurn_GetScorched(const HdBurnTile **tiles) {
  ForgetIceOnAreaLoad();
  *tiles = g_scorched;
  return player_is_indoors ? 0 : g_num_scorched;
}

int HdBurn_GetFrozen(const HdBurnTile **tiles) {
  ForgetIceOnAreaLoad();
  *tiles = g_frozen;
  return player_is_indoors && !IndoorIce() ? 0 : g_num_frozen;
}
