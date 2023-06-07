#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

#ifdef VERTEXLIGHT
#include "Lighting.glsl"
#endif

varying vec2 vTexCoord;
varying vec4 vColor;

#ifdef VERTEXLIGHT
varying vec3 vVertexLight;
#endif

//flat varying int fTexId;
#ifdef VERTEXLIGHT
flat varying uint fTexLit;
#endif

/*
    /// fTexLit Values ///
    // LIT   = 0
    // UNLIT = 1
*/

void VS()
{
//    vec3 worldPos = vec3((iPos * iModelMatrix).xy, 0.0);
    vec3 worldPos = vec3((iPos * iModelMatrix).xyz);

    vTexCoord = iTexCoord;
    vColor = iColor;

//	fTexId   = int(iTangent.x);  // texture unit

    gl_Position = GetClipPos(worldPos);

#ifdef VERTEXLIGHT
    fTexLit  = uint(iTangent.y); // unlit

    // Lit Cases
    if (fTexLit == 0.0)
    {
        // Ambient & per-vertex lighting
        vVertexLight = GetAmbient(GetZonePos(worldPos));

        #ifdef NUMVERTEXLIGHTS
        for (int i = 0; i < NUMVERTEXLIGHTS; ++i)
            vVertexLight += GetVertexLightVolumetric(i, worldPos) * cVertexLights[i * 3].rgb;
        #endif
    }
#endif
}

void PS()
{
    vec4 diffInput = texture2D(sDiffMap, vTexCoord);

#ifdef VERTEXLIGHT
    if (fTexLit < 1.0)
        diffInput *= vec4(vVertexLight, 1.0);
#endif

    gl_FragColor = cMatDiffColor * vColor * diffInput;
}
