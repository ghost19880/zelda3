// Vibrazione del controller: i momenti del gioco che fanno vibrare
// (opzione [Features] Rumble).
// As in Four Swords Adventures, where the GameCube motor is on or off and
// FSA (801395a4) uses it for 0.5, 1 or 1.5 s, on hits and landings: a boss
// takes no rumble for a hit, only for its defeat; death, shield, charges and
// menus never vibrate. The kinds are groups of moments with the same
// strength and length (hd_rumble.c)
#ifndef ZELDA3_HD_RUMBLE_H_
#define ZELDA3_HD_RUMBLE_H_

#include "types.h"

enum {
  // Link: Pegasus Boots crash, damage under a heart, landing from a ledge or
  // a fall, hammer blow (FSA level 1)
  kRumble_Link,
  // Link, strong: damage of a heart or more, charged hammer blow, the three
  // medallions once at the start (FSA level 2)
  kRumble_LinkStrong,
  // light: the Hookshot hits or hooks on; a boss shakes the ground (Ganon and
  // his bat land, the last Armos Knight crushes, the Lanmolas come out)
  kRumble_Light,
  kRumble_BossIce,       // Kholdstare's falling ice hits the floor: light and short
  kRumble_Rock,          // a falling rock of Death Mountain lands near Link (FSA TWEV rubble)
  kRumble_Bomb,          // Link's bomb explodes (FSA HBMB, level 1)
  kRumble_SuperBomb,     // the Super Bomb explodes (FSA BBMB, level 3)
  kRumble_BossDefeat,    // a boss is defeated (FSA SLK2, 160 frames)
  kRumble_DungeonOpen,   // Misery Mire and Turtle Rock open, renewed while the screen shakes
  kRumble_DungeonBoom,   // each boom of the Palace of Darkness and Ganon's Tower openings
  kRumble_BirdFreed,     // the weathervane breaks and the bird flies free
  kRumble_Count,
};

// Un momento del gioco
void HdRumble(int kind);
// A ogni fotogramma del gioco: momenti che si vedono dallo stato
void HdRumble_Tick(void);
// A rumble only when sprite k is near Link (falling rocks)
void HdRumble_NearSprite(int kind, int k);

#endif  // ZELDA3_HD_RUMBLE_H_
