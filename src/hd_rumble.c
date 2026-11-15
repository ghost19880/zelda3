// Vibrazione del controller: quando e quanto (hd_rumble.h)
#include <SDL.h>
#include "hd_rumble.h"
#include "hd_menu.h"
#include "config.h"
#include "variables.h"
#include "player.h"
#include "hd_fx.h"
#include <stdlib.h>

// Strength (at "Strong", Host_Rumble scales it for Medium and Light) and
// duration in ms of each kind. As in FSA a new rumble replaces the running one
// Strengths as they feel at "Medium" (Host_Rumble: Medium = 70% of Strong)
#define MEDIUM(pct) ((pct) / 70.0f)
enum {
  kFsaLevel1 = 500,   // FSA's levels: the full motor for 30, 60, 90 frames
  kFsaLevel2 = 1000,
  kFsaLevel3 = 1500,
  kShort = 200,       // falling rocks and ice: they land often
};
static const struct { float strength; int ms; } kRumble[kRumble_Count] = {
  [kRumble_Link] = { MEDIUM(50), kFsaLevel1 },
  [kRumble_LinkStrong] = { MEDIUM(50), kFsaLevel2 },
  [kRumble_Light] = { MEDIUM(15), kFsaLevel1 },
  [kRumble_BossIce] = { MEDIUM(15), kShort },
  [kRumble_Rock] = { MEDIUM(50), kShort },
  [kRumble_Bomb] = { MEDIUM(70), kFsaLevel1 },
  [kRumble_SuperBomb] = { MEDIUM(70), kFsaLevel3 },
  [kRumble_BossDefeat] = { MEDIUM(70), 2700 },  // FSA SLK2: 160 frames
  [kRumble_DungeonOpen] = { MEDIUM(100), 100 },  // renewed while the screen shakes
  [kRumble_DungeonBoom] = { MEDIUM(100), kFsaLevel1 },
  [kRumble_BirdFreed] = { MEDIUM(30), 250 },
};
#undef MEDIUM

void HdRumble(int kind) {
  // only while playing: not in the title, file select or attract demo
  if (!g_config.rumble || (unsigned)kind >= kRumble_Count || !HdFx_GameLoaded())
    return;
  // the GameCube pad has one motor: only the heavy (left) one, as Dolphin
  // plays it on a modern pad (Motor L); the light one made it feel longer
  Host_Rumble(kRumble[kind].strength, 0.0f, kRumble[kind].ms);
}

void HdRumble_NearSprite(int kind, int k) {
  int dx = (sprite_x_lo[k] | sprite_x_hi[k] << 8) - link_x_coord;
  int dy = (sprite_y_lo[k] | sprite_y_hi[k] << 8) - link_y_coord;
  if (abs(dx) < 48 && abs(dy) < 48)
    HdRumble(kind);
}

// Boss defeat: most bosses explode in sprite state 4; the Armos Knights and
// the Lanmolas die one by one (state 6), the defeat is the last of them
static void BossDefeatTick(void) {
  static uint8 last[16];
  for (int k = 0; k < 16; k++) {
    uint8 s = sprite_state[k], t = sprite_type[k];
    if (s != last[k]) {
      bool defeat = s == 4;
      if (s == 6 && (t == 0x53 || t == 0x54)) {
        defeat = true;
        for (int j = 0; j < 16; j++)
          if (j != k && sprite_type[j] == t && sprite_state[j] >= 9)
            defeat = false;
      }
      if (defeat)
        HdRumble(kRumble_BossDefeat);
    }
    last[k] = s;
  }
}

void HdRumble_Tick(void) {
  BossDefeatTick();
  static uint8 last_state, last_module, last_hook;
  uint8 st = link_player_handler_state;
  if (g_config.rumble) {
    // end of a ledge jump (states 11-16) on the ground; FSA does not
    // vibrate when the landing is in water
    if (st != last_state && last_state >= 11 && last_state <= 16 && st == kPlayerState_Ground)
      HdRumble(kRumble_Link);
    // fall into a hole: arrival on the floor below (module 17 -> 7)
    if (last_module == 17 && main_module_index == 7)
      HdRumble(kRumble_Link);
    // the Hookshot hooks onto something
    if (related_to_hookshot && !last_hook)
      HdRumble(kRumble_Light);
  }
  last_state = st, last_module = main_module_index, last_hook = related_to_hookshot;
}
