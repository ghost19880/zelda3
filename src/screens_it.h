#ifndef ZELDA3_SCREENS_IT_H_
#define ZELDA3_SCREENS_IT_H_

#include "types.h"

// Scritte italiane disegnate con le lettere del gioco fuori dal menu Start:
// selezione dei giochi, didascalie del finale, storia dell'avventura nei
// titoli di coda e "Fine". Attive con la lingua "it" (HudIt_Active).

// Selezione, copia e cancellazione dei giochi e scelta del nome: dopo che
// l'NMI ha scritto le strisce nella mappa dello sfondo, cambia le parole
// inglesi con quelle italiane nelle stesse righe
void ScreensIt_FileSelectAfterStripes(void);

// Didascalie della scena del finale: dati nel formato di kEnding0_Data
// (orig..orig_end sono quelli originali della scena, servono le righe).
// Falso se resta il testo originale
bool ScreensIt_EndingScene(int scene, const uint8 *orig, const uint8 *orig_end,
                           const uint8 **cur, const uint8 **end);

// Riga row dei titoli di coda nel formato di kEnding_Credits_Text
// (colonna, lunghezza, caratteri), o NULL per quella originale
const uint8 *ScreensIt_CreditsRow(int row);

// Colonna delle cifre delle vite nella storia dell'avventura
int ScreensIt_CreditsDigitsColumn(void);

typedef struct Ppu Ppu;
// "Nintendo presenta" all'inizio e "Fine" al posto di "The End" alla fine
// dei titoli di coda, solo mentre la
// PPU disegna il fotogramma (rimessi da ScreensIt_AfterRender)
void ScreensIt_BeforeRender(Ppu *ppu);
void ScreensIt_AfterRender(Ppu *ppu);

#endif  // ZELDA3_SCREENS_IT_H_
