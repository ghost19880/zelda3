// Ombre di nuvole, calcolate alla risoluzione dello SNES nel passaggio 1
// (01-masks.glsl, canale b): sono morbide, quindi ingrandite restano uguali,
// e farle per ogni pixel dello schermo costava troppo alla scheda video
// (a 1440p/4K e 200 fps l'immagine andava a scatti).
//
// Le sagome sono disegnate a mano: una nuvola grande e una piccola in una
// piastrella che si ripete, grandi come quelle di Four Swords Adventures.
// fsa_clouds.png contiene la distanza firmata dal bordo delle nuvole
// (0.5 = bordo, piu' scuro = dentro; 1/64 di valore = 1 pixel del disegno).
// Un pixel SNES e' 0.61 pixel del disegno in orizzontale e 0.72 in
// verticale: piastrella di ~416 x 354 pixel di gioco.

uniform sampler2D FSAClouds;

// Piastrella del disegno: 254 x 255 pixel del disegno
#define CLOUD_DRAW_PX vec2(0.61, 0.72)
#define CLOUD_TILE (vec2(254.0, 255.0) / CLOUD_DRAW_PX)

float cloud_shadow(vec2 world, float t)
{
    // FSA: le nuvole vanno a destra e in basso (diagonale 4:3);
    // FSA_CLOUD_WIND = velocita' in pixel al secondo (FSA: 16)
    vec2 p = world - vec2(0.8, 0.6) * (t * FSA_CLOUD_WIND);
    float d = (COMPAT_TEXTURE(FSAClouds, p / CLOUD_TILE).r - 0.5) * 64.0 * 2.0 / (CLOUD_DRAW_PX.x + CLOUD_DRAW_PX.y);
    // FSA_CLOUD_AMOUNT 1 = come il disegno, piu' basso = nuvole piu' piccole
    d += (1.0 - FSA_CLOUD_AMOUNT) * 30.0;
    // bordo morbido di circa 4 pixel, come la trama ingrandita di FSA
    return clamp(0.5 - d / 4.0, 0.0, 1.0);
}
