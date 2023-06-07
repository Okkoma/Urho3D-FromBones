#define Urho2DSamplers

#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "PostProcess.glsl"
#include "FXAA2n.glsl"

#ifdef VERTEXLIGHT
#include "Lighting.glsl"
#endif

varying vec4 vColor;
varying vec2 vTexCoord;

#ifdef VERTEXLIGHT
varying vec3 vVertexLight;
#endif

flat varying int  fTextureId;
flat varying uint fTexEffect1;
flat varying uint fTexEffect2;
flat varying uint fTexEffect3;

void VS()
{
    vec3 worldPos = vec3((iPos * iModelMatrix).xy, 0.0);
  
    vColor = iColor;
    vTexCoord = iTexCoord;

	fTextureId   = int(iTangent.x);
	
    fTexEffect1  = uint(iTangent.y); // unlit
    fTexEffect2  = uint(iTangent.z); // other effects (cropalpha, blur, fxaa2)
    fTexEffect3  = uint(iTangent.w); // tile index

#ifdef VERTEXLIGHT
    if (fTexEffect1 == uint(0))
    {
        // Ambient & per-vertex lighting
        vVertexLight = GetAmbient(GetZonePos(worldPos));

        #ifdef NUMVERTEXLIGHTS
        for (int i = 0; i < NUMVERTEXLIGHTS; ++i)
            vVertexLight += GetVertexLightVolumetric(i, worldPos) * cVertexLights[i * 3].rgb;
        #endif
    }
#endif

    gl_Position = GetClipPos(worldPos);
}

#ifdef COMPILEPS
vec4 ApplyTextureEffects(sampler2D texSampler)
{
    vec4 value = texture2D(texSampler, vTexCoord);

    // BLUR=4
    if ((fTexEffect2 & uint(2)) == uint(2))
        value *= GaussianBlur(5, vec2(1.0, 1.0), cGBufferInvSize.xy * 2.0, 2.0, texSampler, vTexCoord);
    // FXAA=8
    else if ((fTexEffect2 & uint(4)) == uint(4))
        value *= FXAA2(texSampler, vTexCoord);

    // CROPALPHA=1
    if ((fTexEffect2 & uint(1)) == uint(1))
    {
        const float minval = 0.13;
        if (value.r < minval && value.g < minval && value.b > 1-minval)
            value.a = (value.r + value.g + 1.0-value.b) / 3.0;
    }

    // LIT or UNLIT=1
#ifdef VERTEXLIGHT
    vec4 litColor;

    // USE LIGHT (by default)
    if (fTexEffect1 == uint(0))
        litColor = vec4(vVertexLight, 1.0);
    // UNLIT
    else
        litColor = vec4(1);

    value *= litColor;
#endif

    return value;
}
#endif

void PS()
{
    vec4 diffInput;

	switch(fTextureId)
	{
        case -1: diffInput = vec4(1.0); break;
		case  0: diffInput = ApplyTextureEffects(sUrho2DTextures[ 0]); break;
        case  1: diffInput = ApplyTextureEffects(sUrho2DTextures[ 1]); break;
        case  2: diffInput = ApplyTextureEffects(sUrho2DTextures[ 2]); break;
        case  3: diffInput = ApplyTextureEffects(sUrho2DTextures[ 3]); break;
        case  4: diffInput = ApplyTextureEffects(sUrho2DTextures[ 4]); break;
        case  5: diffInput = ApplyTextureEffects(sUrho2DTextures[ 5]); break;
        case  6: diffInput = ApplyTextureEffects(sUrho2DTextures[ 6]); break;
        case  7: diffInput = ApplyTextureEffects(sUrho2DTextures[ 7]); break;
	}

    gl_FragColor = cMatDiffColor * vColor * diffInput;
}
