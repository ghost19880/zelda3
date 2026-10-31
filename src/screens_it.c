// Scritte italiane disegnate con le lettere del gioco fuori dal menu Start
// (quelle del menu sono in hud_it.c). Nomi dei luoghi come nella nostra
// traduzione (Calbarico, Santuario, Castello di Sabbia, Prigione del castello...).
#include <string.h>
#include "screens_it.h"
#include "hud_it.h"
#include "zelda_rtl.h"
#include "variables.h"
#include "snes/ppu.h"

// ---------------------------------------------------------------------------
// Selezione dei giochi. Le scritte sono strisce di lettere alte due caselle
// (sopra e sotto) nella mappa dello sfondo a 0x6000: A-P sono le caselle
// 0x00-0x0f, Q-Z 0x20-0x29, la parte di sotto e' 16 caselle dopo; la I e'
// 0xaf, lo spazio 0xa9. Simboli usati nelle frasi: '<' e '>' chiudono
// "WHICH?" nella copia.

static int FsTop(char c) {
  switch (c) {
  case ' ': return 0xa9;
  case 'I': return 0xaf;
  case '?': return 0x6f;
  case '<': return 0x85;
  case '>': return 0x86;
  }
  if (c >= 'A' && c <= 'P')
    return c - 'A';
  if (c >= 'Q' && c <= 'Z')
    return 0x20 + c - 'Q';
  return 0xa9;
}

static int FsBottom(char c) {
  switch (c) {
  case ' ': return 0xa9;
  case 'I': return 0xbf;
  case '?': return 0x7f;
  case '<': return 0x95;
  case '>': return 0x96;
  }
  return FsTop(c) + 0x10;
}

static const struct {
  const char *en, *it;
  uint8 module;  // 0: in tutte le schermate della selezione
  uint8 row;     // 0: in ogni riga (3: cartiglio in alto)
} kFileSelectWords[] = {
  { "PLAYER  SELECT", "SCEGLI PARTITA", 0, 3 },
  { "COPY  PLAYER", "COPIA PARTITA", 0, 3 },
  { "COPY  PLAYER", "COPIA PARTITA", 0 },
  { "ERASE PLAYER", "CANCELLAZIONE", 0, 3 },  // nel cartiglio ci stanno 13 lettere
  { "ERASE PLAYER", "CANCELLA PARTITA", 0 },
  { "WHICH PLAYER DO YOU WANT", "QUALE PARTITA VUOI", 0 },
  { "TO ERASE ?", "CANCELLARE?", 0 },
  { "<TO WHICH?>", "  <DOVE?>", 0 },
  { "<WHICH?>", "<QUALE?>", 0 },
  { "COPY OK", "CONFERMA", 0 },
  { "QUIT", "ANNULLA", 0 },
  { "ERASE THIS PLAYER", "CANCELLA QUESTA PARTITA", 0 },
  { "REGISTER  YOUR  NAME", " SCRIVI IL TUO NOME", 4 },
  { " END", "FINE", 4 },
};

// Vero se alla casella a (riga di sopra) c'e' la scritta s, sopra e sotto.
// Sotto gli spazi puo' esserci altro (nel cartiglio di legno di "PLAYER
// SELECT" c'e' la casella 0xb9)
static bool FsMatch(const uint16 *map, int a, const char *s) {
  for (int i = 0; s[i]; i++)
    if ((map[a + i] & 0x3ff) != FsTop(s[i]) ||
        (s[i] != ' ' && (map[a + 32 + i] & 0x3ff) != FsBottom(s[i])))
      return false;
  return true;
}

void ScreensIt_FileSelectAfterStripes(void) {
  if (!HudIt_Active() || main_module_index < 1 || main_module_index > 4)
    return;
  uint16 *map = &g_zenv.vram[0x6000];
  // due schermate da 32 x 32 (la scelta del nome usa anche la seconda)
  for (int a = 0; a < 0x800 - 32; a++) {
    int col = a & 31;
    for (int k = 0; k < countof(kFileSelectWords); k++) {
      const char *en = kFileSelectWords[k].en, *it = kFileSelectWords[k].it;
      int module = kFileSelectWords[k].module;
      int len_en = (int)strlen(en), len_it = (int)strlen(it);
      int n = len_en > len_it ? len_en : len_it;
      int row = kFileSelectWords[k].row;
      if ((module && module != main_module_index) || (row && row != (a >> 5)) ||
          col + n > 32 || !FsMatch(map, a, en))
        continue;
      uint16 attr = map[a] & ~0x3ff;
      // parte di sotto degli spazi: quella dei primi spazi della scritta
      int space_bottom = FsBottom(' ');
      for (int i = 0; i < len_en; i++)
        if (en[i] == ' ') {
          space_bottom = map[a + 32 + i] & 0x3ff;
          break;
        }
      for (int i = 0; i < n; i++) {
        char c = i < len_it ? it[i] : ' ';
        uint16 at = i < len_en ? map[a + i] & ~0x3ff : attr;
        uint16 at_bottom = i < len_en ? map[a + 32 + i] & ~0x3ff : attr;
        map[a + i] = at | FsTop(c);
        map[a + 32 + i] = at_bottom | (c == ' ' ? space_bottom : FsBottom(c));
      }
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Finale. Le lettere sono indici di kEnding_MapData: gialle (titolo della
// scena) 26 + lettera, verdi 56 + lettera, rosse (intestazioni dei titoli di
// coda) 0 + lettera, grandi su due righe 93 + lettera sopra e 131 + lettera
// sotto; 159 e' lo spazio.
enum { kFontYellow, kFontGreen, kFontRed, kFontBigTop, kFontBigBottom };

static uint8 EndingChar(char c, int font) {
  if (c == ' ')
    return 159;
  bool letter = c >= 'A' && c <= 'Z';
  switch (font) {
  case kFontYellow:
    return letter ? 26 + c - 'A' : c == '\'' ? 53 : c == ',' ? 55 : c == '-' ? 54 : 159;
  case kFontGreen:
    return letter ? 56 + c - 'A' : c == '.' ? 82 : 159;
  case kFontRed:
    return letter ? c - 'A' : 159;
  case kFontBigTop:
    return letter ? 93 + c - 'A' : c == '\'' ? 119 : c == '!' ? 120 : 159;
  default:
    return letter ? 131 + c - 'A' : c == '\'' ? 157 : c == '!' ? 158 : 159;
  }
}

// Didascalie: titolo (giallo), riga verde (solo l'ultima scena), luogo
// (grande). Come sulla GBA, con i nostri nomi.
static const char *const kEndingScenes[16][3] = {
  { "IL RITORNO DEL RE", NULL, "CASTELLO DI HYRULE" },
  { "IL PRETE FEDELE", NULL, "SANTUARIO" },
  { "SAHASRAHLA RITORNA", NULL, "VILLAGGIO CALBARICO" },
  { "LA TERRA DEGLI AVVOLTOI", NULL, "CASTELLO DI SABBIA" },
  { "UN BULLO PER AMICO", NULL, "TORRE SUL MONTE" },
  { "LA GUARIGIONE DELLO ZIO", NULL, "CASA" },
  { "PINNE IN VENDITA", NULL, "CASCATE DI ZORA" },
  { "LA STREGA E L'APPRENDISTA", NULL, "BOTTEGA MAGICA" },
  { "GEMELLI TAGLIALEGNA", NULL, "CAPANNA DEI BOSCAIOLI" },
  { "L'OCARINA SUONA ANCORA", NULL, "BOSCHETTO SPIRITATO" },
  { "VENUS REGINA DELLE FATE", NULL, "CASCATA DEI DESIDERI" },
  { "I FABBRI NANI", NULL, "FUCINA" },
  { "IL RAGAZZO DEL RETINO", NULL, "VILLAGGIO CALBARICO" },
  { "IL VECCHIO PERDUTO", NULL, "MONTE MORTE" },
  { "UN LADRO FRA LE FRONDE", NULL, "BOSCO PERDUTO" },
  { "LA SPADA SUPREMA", "RIPOSA DI NUOVO...", "E PER SEMPRE!" },
};

// Riga di testo centrata in 32 caselle nella riga di mappa row_adr, nel
// formato di kEnding0_Data (indirizzo alto e basso, 0, 2 * lunghezza - 1)
static uint8 *PutEndingLine(uint8 *p, int row_adr, const char *s, int font) {
  int len = (int)strlen(s);
  int adr = row_adr + (32 - len) / 2;
  *p++ = adr >> 8;
  *p++ = adr & 0xff;
  *p++ = 0;
  *p++ = len * 2 - 1;
  for (int i = 0; i < len; i++)
    *p++ = EndingChar(s[i], font);
  return p;
}

bool ScreensIt_EndingScene(int scene, const uint8 *orig, const uint8 *orig_end,
                           const uint8 **cur, const uint8 **end) {
  static uint8 buf[256];
  if (!HudIt_Active() || scene < 0 || scene >= 16)
    return false;
  uint8 *p = buf;
  // righe e caratteri dalle voci originali: si cambia solo il testo
  while (orig < orig_end) {
    int adr = orig[0] << 8 | orig[1], len = (orig[3] >> 1) + 1;
    int c = orig[4];
    const char *s = NULL;
    int font = 0;
    if (c >= 26 && c < 56)
      s = kEndingScenes[scene][0], font = kFontYellow;
    else if (c >= 56 && c < 83)
      s = kEndingScenes[scene][1], font = kFontGreen;
    else if (c >= 93 && c < 121)
      s = kEndingScenes[scene][2], font = kFontBigTop;
    else if (c >= 131 && c < 159)
      s = kEndingScenes[scene][2], font = kFontBigBottom;
    // il segno sopra la S di SAHASRALAH'S (0x624f, un carattere solo) non serve
    if (s && len > 1)
      p = PutEndingLine(p, adr & ~31, s, font);
    orig += 4 + len;
  }
  *cur = buf;
  *end = p;
  return true;
}

// Storia dell'avventura in fondo ai titoli di coda (righe di
// kEnding_Credits_Offs). Le cifre delle vite vanno due caselle piu' a destra
// che nell'originale, perche' "TANA DEI LADRONI" e "ROCCA TARTARUGA" non ci
// starebbero.
enum { kDigitsColumn = 0x1b };

int ScreensIt_CreditsDigitsColumn(void) {
  return HudIt_Active() ? kDigitsColumn : 0x19;
}

static const char *const kCastleLines[5] = {
  "CASTELLO DI HYRULE", "PRIGIONE DEL CASTELLO", "PALAZZO DELL'EST",
  "CASTELLO DI SABBIA", "TORRE SUL MONTE",
};
static const char *const kDungeonLines[8] = {
  "PALAZZO CUPO", "DEDALO D'ACQUA", "BOSCO D'OSSA", "TANA DEI LADRONI",
  "FORTE GHIACCIO", "MALAPALUDE", "ROCCA TARTARUGA", "TORRE DI GANON",
};

static uint8 *PutCreditsText(uint8 *p, const char *s, int font) {
  for (; *s; s++)
    *p++ = EndingChar(*s, font);
  return p;
}

const uint8 *ScreensIt_CreditsRow(int row) {
  static uint8 buf[64];
  if (!HudIt_Active())
    return NULL;
  uint8 *p = buf + 2;
  int col;
  static const char kHistory[] = "STORIA DELL'AVVENTURA";
  static const char kTotal[] = "TOTALE VITE USATE";
  if (row == 322 || row == 323) {
    col = (32 - (int)strlen(kHistory)) / 2;
    p = PutCreditsText(p, kHistory, row == 322 ? kFontBigTop : kFontBigBottom);
  } else if (row == 326) {
    // LUOGO a sinistra, VITE sopra le cifre
    col = 3;
    p = PutCreditsText(p, "LUOGO", kFontRed);
    while (p - (buf + 2) < kDigitsColumn - 1 - col)
      *p++ = 159;
    p = PutCreditsText(p, "VITE", kFontRed);
  } else if (row >= 329 && row <= 345 && (row - 329) % 4 == 0) {
    int i = (row - 329) / 4;
    col = 4;
    p = PutCreditsText(p, kCastleLines[i], i & 1 ? kFontGreen : kFontYellow);
  } else if (row >= 349 && row <= 378 && ((row - 349) % 4 == 0 || (row - 349) % 4 == 1)) {
    int i = (row - 349) / 4;
    bool top = (row - 349) % 4 == 0;
    if (top) {
      // numero (cifra grande, parte di sopra), spazio, nome
      col = 8;
      *p++ = 84 + i;
    } else {
      // "L" rossa come sulla GBA al posto di "LEVEL", poi la cifra di sotto
      col = 7;
      *p++ = EndingChar('L', kFontRed);
      *p++ = 122 + i;
    }
    *p++ = 159;
    p = PutCreditsText(p, kDungeonLines[i], top ? kFontBigTop : kFontBigBottom);
  } else if (row == 392 || row == 393) {
    col = 4;
    p = PutCreditsText(p, kTotal, row == 392 ? kFontBigTop : kFontBigBottom);
  } else {
    return NULL;
  }
  int len = (int)(p - (buf + 2));
  buf[0] = col;
  buf[1] = len * 2 - 1;
  return buf;
}

// ---------------------------------------------------------------------------
// "Fine" al posto di "The End": quattro sprite 16x16 (caselle 0x00-0x07 e
// 0x10-0x17 della seconda tavola degli sprite, 64 x 16 pixel in corsivo).
// Si compone dai pixel del gioco: la F e' la T con una barretta, la n quella
// di "End", la e quella di "The"; la i e' disegnata qui nello stesso stile
// (5 contorno, 4 pieno, 3 ombra). Come le altre scritte, la VRAM si cambia
// solo mentre la PPU disegna.

enum { kEndW = 64, kEndH = 16, kEndTheEndHash = 0x60223b15 };

static uint16 g_end_saved[16][16];
static uint16 g_end_adr;
static bool g_end_changed;

static int ObjTileAdr(Ppu *ppu, int tile) {
  return (ppu->objTileAdr2 + (tile >> 4) * 256 + (tile & 15) * 16) & 0x7fff;
}

static void ReadEnd(Ppu *ppu, uint8 img[kEndH][kEndW]) {
  for (int y = 0; y < kEndH; y++)
    for (int x = 0; x < kEndW; x++) {
      const uint16 *t = &ppu->vram[ObjTileAdr(ppu, (y >> 3) * 16 + (x >> 3))];
      int b = 7 - (x & 7), r = y & 7;
      img[y][x] = (t[r] >> b & 1) | (t[r] >> (b + 8) & 1) << 1 |
                  (t[8 + r] >> b & 1) << 2 | (t[8 + r] >> (b + 8) & 1) << 3;
    }
}

static void WriteEnd(Ppu *ppu, uint8 img[kEndH][kEndW]) {
  for (int k = 0; k < 16; k++) {
    uint16 *t = &ppu->vram[ObjTileAdr(ppu, (k >> 3) * 16 + (k & 7))];
    memset(t, 0, 16 * sizeof(uint16));
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 8; c++) {
        int p = img[(k >> 3) * 8 + r][(k & 7) * 8 + c], b = 7 - c;
        t[r] |= (p & 1) << b | (p >> 1 & 1) << (b + 8);
        t[8 + r] |= (p >> 2 & 1) << b | (p >> 3 & 1) << (b + 8);
      }
  }
}

static void PutRows(uint8 img[kEndH][kEndW], const char *const *rows, int n, int x0, int y0) {
  for (int y = 0; y < n; y++)
    for (int x = 0; rows[y][x]; x++)
      if (rows[y][x] != '.' && x0 + x < kEndW && y0 + y < kEndH)
        img[y0 + y][x0 + x] = rows[y][x] - '0';
}

static void MakeFine(uint8 src[kEndH][kEndW], uint8 out[kEndH][kEndW]) {
  enum { kX = 15 };  // "Fine" e' largo 34 pixel: centrato nei 64
  memset(out, 0, kEndH * kEndW);
  // F: barra della T (righe 0-3) e asta con il ricciolo, tagliate prima della h
  for (int y = 0; y < kEndH; y++) {
    int last = y < 4 ? 12 : y == 4 ? 11 : y < 12 ? 10 : 9;
    for (int x = 0; x <= last; x++)
      if (src[y][x])
        out[y][kX + x] = src[y][x];
  }
  static const char *const kBarEnd[] = { ".", "5", "5", "5" };
  PutRows(out, kBarEnd, 4, kX + 12, 0);
  out[3][kX + 11] = 5;
  static const char *const kCrossBar[] = { "5555555..", "5444444555", ".55555555" };
  PutRows(out, kCrossBar, 3, kX + 4, 6);
  static const char *const kI[] = {
    "....55", "...5445", "....55", "", "...55.", "..5445",
    "..545.", ".5345.", ".5345.", "533445", "53345.", ".555..",
  };
  PutRows(out, kI, 12, kX + 11, 4);
  // n di "End" (colonne 44-52) ed e di "The" (colonne 22-31), righe 6-15
  for (int y = 6; y < kEndH; y++) {
    for (int x = 44; x <= 52; x++)
      if (src[y][x])
        out[y][kX + 16 + x - 44] = src[y][x];
    for (int x = 22; x <= 31; x++)
      if (src[y][x])
        out[y][kX + 24 + x - 22] = src[y][x];
  }
}

// ---------------------------------------------------------------------------
// "Nintendo presenta" al posto di "Nintendo presents" all'inizio del gioco
// (Intro_DisplayLogo: quattro sprite 16x16 della prima tavola, caselle 0x69,
// 0x6b, 0x6d e 0x6e, 56 x 16 pixel). La s finale sta nella casella 0x7e
// (colonne 3-6, righe 3-7): diventa una a nello stesso stile, con il colore
// della s. Si riconosce il logo dall'hash dei pixel, come "The End".

enum { kLogoW = 56, kLogoH = 16, kLogoHash = 0x55f9e398, kLogoLastTile = 0x7e };

static uint16 g_logo_saved[16];
static uint16 g_logo_adr;
static bool g_logo_changed;

static int Obj1TileAdr(Ppu *ppu, int tile) {
  return (ppu->objTileAdr1 + (tile & 0xff) * 16) & 0x7fff;
}

static int ObjTilePixel(const uint16 *t, int x, int y) {
  int b = 7 - x;
  return (t[y] >> b & 1) | (t[y] >> (b + 8) & 1) << 1 |
         (t[8 + y] >> b & 1) << 2 | (t[8 + y] >> (b + 8) & 1) << 3;
}

static void LogoToPresenta(Ppu *ppu) {
  static const uint8 kLogoX[4] = { 0, 16, 32, 40 };
  static const uint8 kLogoTile[4] = { 0x69, 0x6b, 0x6d, 0x6e };
  static uint8 img[kLogoH][kLogoW];
  memset(img, 0, sizeof(img));
  for (int i = 0; i < 4; i++)
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        int t = kLogoTile[i] + (y >> 3) * 16 + (x >> 3);
        int p = ObjTilePixel(&ppu->vram[Obj1TileAdr(ppu, t)], x & 7, y & 7);
        if (p)
          img[y][kLogoX[i] + x] = p;
      }
  uint32 h = 2166136261u;
  for (int y = 0; y < kLogoH; y++)
    for (int x = 0; x < kLogoW; x++)
      h = (h ^ img[y][x]) * 16777619u;
  if (h != kLogoHash)
    return;
  int adr = Obj1TileAdr(ppu, kLogoLastTile);
  uint16 *t = &ppu->vram[adr];
  memcpy(g_logo_saved, t, sizeof(g_logo_saved));
  g_logo_adr = adr;
  g_logo_changed = true;
  int color = 0;  // quello della s
  for (int y = 3; y < 8; y++)
    for (int x = 3; x < 7; x++)
      if (ObjTilePixel(t, x, y))
        color = ObjTilePixel(t, x, y);
  static const char *const kA[5] = { "###.", "...#", ".###", "#..#", ".###" };
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 4; x++) {
      int r = 3 + y, b = 7 - (3 + x), c = kA[y][x] == '#' ? color : 0;
      uint16 m = (uint16)~(1 << b | 1 << (b + 8));
      t[r] = (t[r] & m) | (c & 1) << b | (c >> 1 & 1) << (b + 8);
      t[8 + r] = (t[8 + r] & m) | (c >> 2 & 1) << b | (c >> 3 & 1) << (b + 8);
    }
}

void ScreensIt_BeforeRender(Ppu *ppu) {
  g_end_changed = false;
  g_logo_changed = false;
  if (HudIt_Active() && main_module_index == 0)
    LogoToPresenta(ppu);
  if (!HudIt_Active() || main_module_index != 0x1a)
    return;
  static uint8 src[kEndH][kEndW], out[kEndH][kEndW];
  ReadEnd(ppu, src);
  uint32 h = 2166136261u;
  for (int y = 0; y < kEndH; y++)
    for (int x = 0; x < kEndW; x++)
      h = (h ^ src[y][x]) * 16777619u;
  if (h != kEndTheEndHash)
    return;
  for (int k = 0; k < 16; k++)
    memcpy(g_end_saved[k], &ppu->vram[ObjTileAdr(ppu, (k >> 3) * 16 + (k & 7))], 16 * sizeof(uint16));
  g_end_adr = ppu->objTileAdr2;
  MakeFine(src, out);
  WriteEnd(ppu, out);
  g_end_changed = true;
}

void ScreensIt_AfterRender(Ppu *ppu) {
  if (g_logo_changed) {
    memcpy(&ppu->vram[g_logo_adr], g_logo_saved, sizeof(g_logo_saved));
    g_logo_changed = false;
  }
  if (!g_end_changed || ppu->objTileAdr2 != g_end_adr)
    return;
  for (int k = 0; k < 16; k++)
    memcpy(&ppu->vram[ObjTileAdr(ppu, (k >> 3) * 16 + (k & 7))], g_end_saved[k], 16 * sizeof(uint16));
  g_end_changed = false;
}
