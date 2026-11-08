// PASS 2/3 - Sfoca le maschere (a meta' risoluzione): ombre e alone morbidi.
// Il canale b (ombre di nuvole) passa com'e'.

#pragma parameter FSA_SHADOW_SOFT "Morbidezza ombre" 1.60 0.50 3.00 0.05

#include "fsa_common.inc"

#ifdef PARAMETER_UNIFORM
uniform COMPAT_PRECISION float FSA_SHADOW_SOFT;
#else
#define FSA_SHADOW_SOFT 1.60
#endif

void main()
{
    vec2 texel = 1.0 / max(InputSize, vec2(1.0));
    vec2 uv = vTexCoord;
    vec2 sum = vec2(0.0);
    float norm = 0.0;
    for (int j = -3; j <= 3; j++) {
        for (int i = -3; i <= 3; i++) {
            vec2 o = vec2(float(i), float(j));
            float wgt = exp(-dot(o, o) / 6.0);
            sum += COMPAT_TEXTURE(Source, uv + o * texel * FSA_SHADOW_SOFT).rg * wgt;
            norm += wgt;
        }
    }
    // b (nuvole) passa senza sfocatura: il bordo e' gia' morbido
    FragColor = vec4(sum / norm, COMPAT_TEXTURE(Source, uv).b, 1.0);
}

#endif
