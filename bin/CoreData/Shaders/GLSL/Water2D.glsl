#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"

varying vec2 vTexCoord;
varying vec4 vColor;

varying vec2 vRefractUV;
varying vec2 vWaterUV;

void VS()
{
//    vec3 worldPos = vec3((iPos * iModelMatrix).xy, 0.0);
    vec3 worldPos = vec3((iPos * iModelMatrix).xyz);

    vTexCoord = iTexCoord;
    vColor = iColor;
	vWaterUV = iTexCoord + cElapsedTime * vec2(0.05, 0.0);

    gl_Position = GetClipPos(worldPos);
    vRefractUV = GetQuadTexCoord(gl_Position);
}

void PS()
{
    vec2 refractUV = vRefractUV;
    float variation = sin(10.0 * vWaterUV.y + 2.0 * cElapsedTimePS);
    refractUV.x += 0.0025 * variation;
    refractUV.y += 0.0012 * variation;

    vec3 reflectColor = texture2D(sDiffMap, refractUV).rgb;
    //vec3 waterColor   = texture2D(sNormalMap, vWaterUV).rgb;

    //gl_FragColor.rgb = cMatDiffColor.rgb * waterColor * reflectColor * vColor.rgb;
    gl_FragColor.rgb = cMatDiffColor.rgb * reflectColor * vColor.rgb;
    gl_FragColor.a = 1.0;
}
