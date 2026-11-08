// PASS 1/3 - Maschere, alla risoluzione dello SNES.
// r = quanto il pixel "fa ombra": le zone scure del fondale (chiome, dirupi,
//     contorni) proiettano un'ombra morbida in basso a destra, come in Four
//     Swords Adventures. Sprite e HUD non contano.
// g = quanto il pixel e' chiaro: sorgente dell'alone sulle zone luminose.
// b = ombra di nuvole (0-1), nel mondo, all'aperto (fsa_clouds.inc).

#pragma parameter FSA_SHADOW_DARK "Soglia del buio che fa ombra" 0.30 0.05 0.60 0.01
#pragma parameter FSA_BLOOM_THRESHOLD "Soglia alone"             0.80 0.40 1.00 0.02
#pragma parameter FSA_CLOUD_AMOUNT "Quante nuvole (0-1)"         1.00 0.00 1.00 0.05
#pragma parameter FSA_CLOUD_WIND  "Velocita' nuvole (px/s)"    16.00 0.00 40.0 0.50

#include "fsa_common.inc"

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float FSA_SHADOW_DARK;
uniform COMPAT_PRECISION float FSA_BLOOM_THRESHOLD;
#else
#define FSA_SHADOW_DARK 0.30
#define FSA_BLOOM_THRESHOLD 0.80
#endif

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float FSA_CLOUD_AMOUNT;
uniform COMPAT_PRECISION float FSA_CLOUD_WIND;
#else
#define FSA_CLOUD_AMOUNT 1.00
#define FSA_CLOUD_WIND  16.00
#endif

// Posizione nel mondo (pixel SNES), tempo di gioco, sole e onda dello specchio
uniform COMPAT_PRECISION vec2 ScrollOffset;
// 1 = all'aperto con il sole (niente pioggia, nebbia o nuvole basse)
uniform COMPAT_PRECISION float Sunny;
// Game time in seconds + 1 (0 = not set, old exe): follows turbo and pause
uniform COMPAT_PRECISION float GameTime;
uniform COMPAT_PRECISION float MirrorDx[240];
// White the game added to the whole scene (0-1): masks of the scene under it
uniform COMPAT_PRECISION float WhiteFloor;

#include "fsa_clouds.inc"

void main()
{
    vec4 c = COMPAT_TEXTURE(Source, vTexCoord);
    float layer = floor(c.a * 255.0 + 0.5);
    // 2 = HUD, 3 = fondale (cielo e nuvole dietro la Piramide o sul Monte
    // della Morte), 4 e 6 = sprite: niente ombre da questi (vedi 03-final.glsl)
    float bg = 1.0 - step(1.5, layer) * step(layer, 4.5)
                   - step(5.5, layer) * step(layer, 6.5);
    float wf = clamp(WhiteFloor, 0.0, 0.98);
    float luma = dot(clamp((c.rgb - wf) / (1.0 - wf), 0.0, 1.0), vec3(0.299, 0.587, 0.114));
    float occluder = (1.0 - smoothstep(FSA_SHADOW_DARK * 0.35, FSA_SHADOW_DARK, luma)) * bg;
    float bright = smoothstep(FSA_BLOOM_THRESHOLD, min(FSA_BLOOM_THRESHOLD + 0.15, 1.0), luma)
                 * (1.0 - step(1.5, layer) * step(layer, 3.5));
    // nuvole: per pixel SNES (questo passaggio e' alla risoluzione del gioco)
    vec2 px = vTexCoord * TextureSize;
    vec2 world = px + ScrollOffset;
    world.x += MirrorDx[int(clamp(px.y, 0.0, 239.0))];
    float t = GameTime > 0.0 ? GameTime - 1.0 : float(FrameCount) / 60.0;
    float cloud = Sunny > 0.5 ? cloud_shadow(world, t) : 0.0;
    FragColor = vec4(occluder, bright, cloud, 1.0);
}

#endif
