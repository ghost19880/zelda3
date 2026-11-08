// PASS 3/3 - Composizione finale, alla risoluzione dello schermo.
//
// Cosa si vede in Four Swords Adventures (studiato su un video del primo
// livello): la pixel art resta nitida, con i suoi contorni; sopra c'e' una
// trama di pennellate diagonali "sporche" (fsa_brush.png, piastrella continua
// inclinata), ferma rispetto al mondo; alberi e dirupi
// proiettano ombre morbide in basso a destra; all'aperto passano ombre di
// nuvole; i colori sono saturi; le zone molto chiare hanno un leggero alone.
// Personaggi e HUD non ricevono la trama.

#pragma parameter FSA_PIXEL_SHARP "Nitidezza pixel"              2.50 1.00 8.00 0.10
#pragma parameter FSA_HATCH       "Forza della trama"            1.00 0.00 2.00 0.05
#pragma parameter FSA_BRUSH_SCALE "Dimensione pennellate"        0.25 0.05 1.00 0.01
#pragma parameter FSA_BRUSH_SKIP  "Macchie saltate (0-1)"        0.70 0.00 1.00 0.02
#pragma parameter FSA_SHADOW      "Ombre morbide"                0.45 0.00 1.00 0.05
#pragma parameter FSA_CLOUDS      "Ombre di nuvole (esterni)"    0.17 0.00 0.60 0.01
#pragma parameter FSA_BLOOM       "Alone sulle zone chiare"      0.30 0.00 1.00 0.02
#pragma parameter FSA_SATURATION  "Saturazione"                  0.96 0.50 2.00 0.02
#pragma parameter FSA_CONTRAST    "Contrasto a S"                0.12 0.00 0.80 0.02
#pragma parameter FSA_KEEP_SPRITES "Sprite senza trama"          1.00 0.00 1.00 0.05
#pragma parameter FSA_KEEP_HUD    "HUD senza effetto"            1.00 0.00 1.00 0.05
#pragma parameter FSA_SKIP_MENUS  "Spegni l'effetto nei menu"    1.00 0.00 1.00 1.00

#include "fsa_common.inc"

uniform sampler2D OrigTexture;
uniform sampler2D FSABrush;
uniform COMPAT_PRECISION vec2 OrigTextureSize;
uniform COMPAT_PRECISION vec2 ScrollOffset;
// Modulo di gioco + 1 (0 = uniform non valorizzato, cioe' eseguibile vecchio).
uniform COMPAT_PRECISION float GameModule;
// 1 = all'aperto con il sole (niente pioggia, nebbia o nuvole basse)
uniform COMPAT_PRECISION float Sunny;
// Animazione dello specchio: quanto e' bianco lo schermo (0-1; 0 anche con
// un eseguibile vecchio che non lo valorizza) e di quanto e' spostata ogni
// riga del fondale dall'onda
uniform COMPAT_PRECISION float MirrorWarp;
uniform COMPAT_PRECISION float MirrorDx[240];
// 1 = "Filtro lineare" attivo nelle impostazioni (LinearFiltering): pixel
// morbidi come senza shader invece che nitidi
uniform COMPAT_PRECISION float LinearFilter;
// White the game added to the whole scene (palettes going white, fixed color
// added, flashes): 0-1, 0 with an old exe. The effects go under it.
uniform COMPAT_PRECISION float WhiteFloor;

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float FSA_PIXEL_SHARP;
uniform COMPAT_PRECISION float FSA_HATCH;
uniform COMPAT_PRECISION float FSA_BRUSH_SCALE;
uniform COMPAT_PRECISION float FSA_BRUSH_SKIP;
uniform COMPAT_PRECISION float FSA_SHADOW;
uniform COMPAT_PRECISION float FSA_CLOUDS;
uniform COMPAT_PRECISION float FSA_BLOOM;
uniform COMPAT_PRECISION float FSA_SATURATION;
uniform COMPAT_PRECISION float FSA_CONTRAST;
uniform COMPAT_PRECISION float FSA_KEEP_SPRITES;
uniform COMPAT_PRECISION float FSA_KEEP_HUD;
uniform COMPAT_PRECISION float FSA_SKIP_MENUS;
#else
#define FSA_PIXEL_SHARP  2.50
#define FSA_HATCH        1.00
#define FSA_BRUSH_SCALE  0.25
#define FSA_BRUSH_SKIP   0.70
#define FSA_SHADOW       0.45
#define FSA_CLOUDS       0.17
#define FSA_BLOOM        0.30
#define FSA_SATURATION   0.96
#define FSA_CONTRAST     0.12
#define FSA_KEEP_SPRITES 1.00
#define FSA_KEEP_HUD     1.00
#define FSA_SKIP_MENUS   1.00
#endif

float hash21(vec2 p)
{
    p = fract(p * vec2(233.34, 851.73));
    p += dot(p, p + 23.45);
    return fract(p.x * p.y);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    return vnoise(p) * 0.55
         + vnoise(p * 2.03 + vec2(11.3, -7.1)) * 0.30
         + vnoise(p * 4.11 + vec2(-5.7, 19.4)) * 0.15;
}

// --- Maschera dei layer ---------------------------------------------------
// Il PPU di zelda3 scrive nel canale alpha l'indice del layer di ogni pixel:
//   0 = BG1   1 = BG2   2 = BG3 (HUD)   3 = fondale   5 = sfondo
//   4 e 6 = OBJ (sprite, palette 0-3 e 4-7)   7 = acqua (fondale)
// Con un eseguibile non modificato l'alpha e' 0: niente viene protetto.
float layer_at(vec2 uv)
{
    return floor(COMPAT_TEXTURE(OrigTexture, uv).a * 255.0 + 0.5);
}

float sprite_of(float layer)
{
    return step(3.5, layer) * step(layer, 4.5) + step(5.5, layer) * step(layer, 6.5);
}

// HUD (2) e fondali (3) restano come li disegna il gioco. Il 3 lo scrive il
// gioco solo sul cielo dietro la Piramide e sulle nuvole del Monte della
// Morte (lo sfondo, 5, e' spesso anche il prato fra le caselle: si dipinge).
float hud_of(float layer)
{
    return step(1.5, layer) * step(layer, 3.5);
}

// 7 = acqua (marcata dal gioco): niente pennellate
float water_of(float layer)
{
    return step(6.5, layer) * step(layer, 7.5);
}

float water_mask(vec2 uv, vec2 sz)
{
    vec2 p = uv * sz - 0.5;
    vec2 i = floor(p);
    vec2 f = p - i;
    f = f * f * (3.0 - 2.0 * f);
    vec2 base = (i + 0.5) / sz;
    vec2 tx = vec2(1.0 / sz.x, 0.0);
    vec2 ty = vec2(0.0, 1.0 / sz.y);
    return mix(mix(water_of(layer_at(base)), water_of(layer_at(base + tx)), f.x),
               mix(water_of(layer_at(base + ty)), water_of(layer_at(base + tx + ty)), f.x), f.y);
}

// Il confronto va fatto sui singoli texel e interpolato dopo: interpolando
// l'indice di layer verrebbero fuori layer intermedi che non esistono.
vec2 protect_mask(vec2 uv, vec2 sz)
{
    vec2 p = uv * sz - 0.5;
    vec2 i = floor(p);
    vec2 f = p - i;
    f = f * f * (3.0 - 2.0 * f);
    vec2 base = (i + 0.5) / sz;
    vec2 tx = vec2(1.0 / sz.x, 0.0);
    vec2 ty = vec2(0.0, 1.0 / sz.y);
    float l00 = layer_at(base), l10 = layer_at(base + tx);
    float l01 = layer_at(base + ty), l11 = layer_at(base + tx + ty);
    vec2 m00 = vec2(sprite_of(l00), hud_of(l00));
    vec2 m10 = vec2(sprite_of(l10), hud_of(l10));
    vec2 m01 = vec2(sprite_of(l01), hud_of(l01));
    vec2 m11 = vec2(sprite_of(l11), hud_of(l11));
    return mix(mix(m00, m10, f.x), mix(m01, m11, f.x), f.y);  // x = sprite, y = HUD
}

// Pixel nitidi con il bordo appena ammorbidito (niente scalini tremolanti)
vec3 sample_sharp(vec2 uv, vec2 sz, float sharp)
{
    vec2 p = uv * sz - 0.5;
    vec2 i = floor(p);
    vec2 f = p - i;
    f = clamp((f - 0.5) * sharp + 0.5, 0.0, 1.0);
    f = f * f * (3.0 - 2.0 * f);
    return COMPAT_TEXTURE(OrigTexture, (i + 0.5 + f) / sz).rgb;
}

// Filtro lineare: media dei quattro pixel vicini (calcolata qui, cosi' non
// dipende dal filtro con cui e' caricata la texture)
vec3 sample_linear(vec2 uv, vec2 sz)
{
    vec2 p = uv * sz - 0.5;
    vec2 i = floor(p);
    vec2 f = p - i;
    vec2 base = (i + 0.5) / sz;
    vec2 tx = vec2(1.0 / sz.x, 0.0);
    vec2 ty = vec2(0.0, 1.0 / sz.y);
    return mix(mix(COMPAT_TEXTURE(OrigTexture, base).rgb, COMPAT_TEXTURE(OrigTexture, base + tx).rgb, f.x),
               mix(COMPAT_TEXTURE(OrigTexture, base + ty).rgb, COMPAT_TEXTURE(OrigTexture, base + tx + ty).rgb, f.x), f.y);
}

// Menu (intro, selezione file, inventario, scelta del punto di partenza),
// morte di Link (modulo 18, lo schermo rosso) e prologo (modulo 20: le
// immagini della storia e la mappa in Mode 7; per le stanze del prologo il
// gioco manda 7): escono intatti.
// GameModule arriva incrementato di 1.
float menu_amount()
{
    float known = step(0.5, GameModule);
    float m = GameModule;
    float menu = step(m, 6.5)
               + step(14.5, m) * step(m, 15.5)
               + step(18.5, m) * step(m, 19.5)
               + step(20.5, m) * step(m, 21.5)
               + step(27.5, m) * step(m, 28.5);
    return known * clamp(menu, 0.0, 1.0) * FSA_SKIP_MENUS;
}

// All'aperto (modulo 9 = mondo esterno)
float outdoors()
{
    return step(9.5, GameModule) * step(GameModule, 10.5);
}

// --- Pennellate -------------------------------------------------------------
// fsa_brush.png e' una piastrella continua (ricavata dalla trama disegnata
// dall'utente da Four Swords: la banda diagonale raddrizzata, con i bordi
// raccordati): ripetuta lungo la stessa inclinazione, una ripetizione prosegue
// nella successiva. I vuoti sono quelli della trama stessa.
#define BRUSH_PX vec2(225.0, 133.0)   // dimensioni di fsa_brush.png
#define BRUSH_ANGLE 50.44             // inclinazione della banda, in gradi

float brush_marks(vec2 world)
{
    float ang = radians(BRUSH_ANGLE);
    vec2 d = vec2(cos(ang), sin(ang));
    vec2 n = vec2(-d.y, d.x);
    vec2 bw = vec2(dot(world, d), dot(world, n));   // pixel SNES lungo/attraverso la banda
    vec2 band = bw / FSA_BRUSH_SCALE;
    float m = COMPAT_TEXTURE(FSABrush, band / BRUSH_PX).r;
    // Ogni tanto una macchia salta: buchi irregolari grandi piu' o meno come
    // una macchia, allungati come le pennellate, fermi rispetto al mondo.
    float hole = vnoise(bw * vec2(1.0 / 15.0, 1.0 / 7.0) + vec2(3.7, 11.2));
    float skip = smoothstep(FSA_BRUSH_SKIP - 0.05, FSA_BRUSH_SKIP + 0.05, 1.0 - hole);
    return m * (1.0 - skip);
}

void main()
{
    vec2 uv = vTexCoord;
    vec2 src = max(OrigTextureSize, vec2(1.0));
    vec2 px = uv * src;                 // pixel SNES sullo schermo
    vec2 world = px + ScrollOffset;     // pixel SNES nel mondo
    // specchio: pennellate e nuvole ondeggiano con il fondale
    world.x += MirrorDx[int(clamp(px.y, 0.0, 239.0))];

    vec2 prot = protect_mask(uv, src);
    float sprite = prot.x * FSA_KEEP_SPRITES;
    float hud = prot.y * FSA_KEEP_HUD;
    float scene = 1.0 - clamp(sprite + hud, 0.0, 1.0);   // fondale
    float menu = menu_amount();

    vec3 orig = LinearFilter > 0.5 ? sample_linear(uv, src) : sample_sharp(uv, src, FSA_PIXEL_SHARP);
    // The scene without the white the game put on it: the effects work on
    // this and the white goes back on top at the end, so shadows, clouds and
    // brush strokes fade out with the scene instead of staying dark over it
    float wf = clamp(WhiteFloor, 0.0, 0.98);
    vec3 base = clamp((orig - wf) / (1.0 - wf), 0.0, 1.0);
    vec3 color = base;
    float luma = dot(color, vec3(0.299, 0.587, 0.114));

    // --- Ombre morbide ------------------------------------------------
    // La luce viene dall'alto a sinistra: l'ombra in un punto viene dalle zone
    // scure che stanno un po' piu' in alto a sinistra. Dove il pixel e' gia'
    // scuro di suo (la chioma stessa) l'ombra non si aggiunge.
    vec2 shadow_off = vec2(3.0, 4.0) / src;
    float occ = COMPAT_TEXTURE(Source, uv - shadow_off).r;
    float self_dark = COMPAT_TEXTURE(Source, uv).r;
    float shadow = clamp(occ * 1.7 - self_dark * 0.9, 0.0, 1.0);
    color *= 1.0 - FSA_SHADOW * 0.55 * shadow * (1.0 - hud);

    // --- Ombre di nuvole (solo all'aperto) -------------------------------
    // calcolate alla risoluzione SNES nel passaggio 1 (fsa_clouds.inc)
    // (letto a meta' risoluzione: il bordo torna stretto come prima)
    float cloud = Sunny > 0.5 ? smoothstep(0.2, 0.8, COMPAT_TEXTURE(Source, uv).b) : 0.0;
    color *= 1.0 - FSA_CLOUDS * cloud * (1.0 - hud);

    // --- Pennellate di Four Swords -------------------------------------------
    // Come FSA: colore del fondale moltiplicato per la trama, su tutti i toni
    // (niente sprite e HUD). In FSA la trama scurisce in media del 9%;
    // 0.25 da' la stessa media con fsa_brush.png.
    // Regola di FSA: la pennellata va solo sul fondale opaco; l'acqua e' uno
    // strato semitrasparente (alfa < 255) e resta liscia (provato in Dolphin)
    float marks = brush_marks(world);
    float brushed = scene * (1.0 - water_mask(uv, src));
    color *= max(1.0 - FSA_HATCH * 0.25 * marks * brushed, 0.0);
    color *= 1.0 + 0.04 * scene;  // il fondale di Four Swords e' un filo piu' chiaro

    // --- Alone sulle zone chiare ------------------------------------------
    vec3 glow = COMPAT_TEXTURE(Source, uv).g * vec3(1.0, 0.99, 0.95);
    color += glow * FSA_BLOOM * 0.5 * scene;

    // --- Colore ---------------------------------------------------------
    color = clamp(color, 0.0, 1.0);
    float l = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(l), color, FSA_SATURATION);
    color = clamp(color, 0.0, 1.0);
    color = mix(color, color * color * (3.0 - 2.0 * color), FSA_CONTRAST);

    // the game's white back on top (channels under it: left as they are)
    color = orig + (1.0 - wf) * (color - base);

    // HUD e menu escono intatti; durante lo specchio l'effetto si schiarisce
    // insieme allo schermo (il gioco aggiunge il bianco dopo tutto il resto)
    color = mix(color, orig, max(max(hud, menu), clamp(MirrorWarp, 0.0, 1.0)));
    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

#endif
