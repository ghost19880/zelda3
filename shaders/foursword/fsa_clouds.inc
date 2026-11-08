// Ombre di nuvole, calcolate alla risoluzione dello SNES nel passaggio 1
// (01-masks.glsl, canale b): sono morbide, quindi ingrandite restano uguali,
// e farle per ogni pixel dello schermo costava troppo alla scheda video
// (a 1440p/4K e 200 fps l'immagine andava a scatti).

float cloud_hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cloud_vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(cloud_hash21(i), cloud_hash21(i + vec2(1.0, 0.0)), u.x),
               mix(cloud_hash21(i + vec2(0.0, 1.0)), cloud_hash21(i + vec2(1.0, 1.0)), u.x), u.y);
}

float cloud_fbm(vec2 p)
{
    return cloud_vnoise(p) * 0.55
         + cloud_vnoise(p * 2.03 + vec2(11.3, -7.1)) * 0.30
         + cloud_vnoise(p * 4.11 + vec2(-5.7, 19.4)) * 0.15;
}

// --- Ombre di nuvole ----------------------------------------------------------
// Cloud shadows that behave like the ones of Four Swords Adventures. Only the
// layout and the measured numbers come from there (one pattern that repeats
// every 512 x 384 SNES pixels, one tile of drift right and down every 40
// seconds, about 40% of the ground covered, a soft edge of about one 4 x 3
// pixel step); the shapes are our own and change from tile to tile. Each tile
// has one big cumulus (a body and a smaller lump below on the right) and one
// small cloud, made of lumps of different sizes melted together. Distances are in tile units of 128 on
// each side, so a lump of radius r is 4r pixels wide and 3r tall as in FSA.
// The shadow darkens ground and sprites alike (only the HUD is left alone).
#define CLOUD_TILE vec2(512.0, 384.0)

vec2 hash22(vec2 p)
{
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.xx + q.yz) * q.zy);
}

// Smooth union (k in tile units): lumps melt into each other instead of
// meeting in sharp notches, which looked like a cartoon
float cloud_smin(float a, float b)
{
    const float k = 1.5;
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

// Signed distance (tile units) to one cumulus: an oval body of half sizes r,
// nb big lumps inside it that make the outline uneven and ns small lumps of
// different sizes along its edge
float cloud_cumulus(vec2 q, vec2 r, float nb, float ns, vec2 seed)
{
    float m = min(r.x, r.y);
    float d = (length(q / r) - 0.72) * m;
    for (int i = 0; i < 6; i++) {
        if (float(i) >= nb)
            break;
        vec2 h = hash22(seed + float(i) * vec2(5.13, 9.71));
        float a = 6.2831853 * (float(i) + 0.8 * h.x) / nb + seed.x * 3.1;
        float k = mix(0.45, 0.75, h.y);
        float rr = m * mix(0.32, 0.48, hash22(h.yx * vec2(13.7, 29.1)).x);
        d = cloud_smin(d, length(q - vec2(cos(a), sin(a)) * r * k) - rr);
    }
    for (int i = 0; i < 18; i++) {
        if (float(i) >= ns)
            break;
        vec2 h = hash22(seed + float(i) * vec2(7.31, 3.17));
        float a = 6.2831853 * (float(i) + 0.9 * h.x) / ns + seed.x;
        float k = mix(0.7, 0.88, h.y);
        float rr = m * mix(0.13, 0.24, hash22(h.yx * vec2(31.7, 17.3) + seed).x);
        d = cloud_smin(d, length(q - vec2(cos(a), sin(a)) * r * k) - rr);
    }
    return d;
}

// The clouds of tile c, seen from u (both in tile units)
float cloud_tile(vec2 u, vec2 c)
{
    vec2 h1 = hash22(c + vec2(0.37, 0.71));
    vec2 h2 = hash22(c + vec2(37.1, 11.9));
    vec2 h3 = hash22(c + vec2(5.3, 91.1));
    vec2 o = c * 128.0;
    // quick exit: nothing of this tile reaches u
    if (length((u - o - vec2(64.0)) / vec2(122.0, 118.0)) > 1.0)
        return 1e5;
    float s = mix(0.9, 1.1, h2.x);
    vec2 b = o + vec2(76.0, 38.0) + (h1 - 0.5) * vec2(16.0, 12.0);
    float d = cloud_cumulus(u - b, vec2(40.0, 30.0) * s, 6.0, 18.0, h1 * vec2(9.1, 5.3));
    vec2 t = b + vec2(12.0 + 10.0 * h2.y, 34.0 + 6.0 * h3.x) * s;
    d = min(d, cloud_cumulus(u - t, vec2(26.0, 21.0) * s, 4.0, 12.0, h2 * vec2(7.7, 3.9)));
    vec2 p = o + vec2(24.0, 100.0) + (h3.yx - 0.5) * vec2(16.0, 10.0);
    return min(d, cloud_cumulus(u - p, vec2(20.0, 15.0), 3.0, 9.0, h3 * vec2(4.3, 8.1)));
}

float cloud_shadow(vec2 world, float t)
{
    // FSA: one tile right and one down in 2400 frames (12.8 and 9.6 pixels a
    // second, 16 along the drift); FSA_CLOUD_WIND scales the speed
    vec2 p = world - CLOUD_TILE * (t / 40.0) * (FSA_CLOUD_WIND / 16.0);
    vec2 u = p / CLOUD_TILE * 128.0;
    vec2 c = floor(u / 128.0);
    float d = 1e5;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            d = min(d, cloud_tile(u, c + vec2(float(i), float(j))));
    // a slow wobble of the edge, so no outline is a clean row of circles
    d += (cloud_fbm(u * vec2(0.07, 0.093)) - 0.5) * 2.0;
    // FSA_CLOUD_AMOUNT 1 = as in FSA, lower = smaller clouds, 0 = none
    d += (1.0 - FSA_CLOUD_AMOUNT) * 40.0;
    // soft edge of about one tile unit (4 x 3 pixels)
    return clamp(0.5 - d / 1.2, 0.0, 1.0);
}
