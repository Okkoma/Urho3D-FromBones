#version 450

/// ps=AMBIENT DIFFMAP DIRLIGHT PERPIXEL SPECULAR

layout(location=0) in vec3 vNormal;
layout(location=1) in vec4 vWorldPos;
layout(location=2) in vec4 vColor;
layout(location=3) in vec2 vTextCoord;

layout(location=0) out vec4 fragColor;

layout(set=0, binding=1) uniform CameraPS
{
    vec3 cCameraPosPS;
};

layout(set=0, binding=2) uniform ZonePS
{
    vec3 cAmbientColor;
    vec4 cFogParams;
    vec3 cFogColor;
};

layout(set=1, binding=1) uniform MaterialPS
{
    vec4 cMatDiffColor;
    vec3 cMatEmissiveColor;
    vec3 cMatEnvMapColor;
    vec4 cMatSpecColor;
};

layout(set=3, binding=0) uniform LightPS
{
    vec4 cLightColor;
    vec3 cLightDirPS;
};

layout(set=4, binding=0) uniform sampler2D diffmap;


float GetDiffuse(vec3 normal, vec3 worldPos, out vec3 lightDir)
{
    // DIRLIGHT
    lightDir = cLightDirPS;
    return max(dot(normal, lightDir), 0.0);
}

float GetSpecular(vec3 normal, vec3 eyeVec, vec3 lightDir, float specularPower)
{
    vec3 halfVec = normalize(normalize(eyeVec) + lightDir);
    return pow(max(dot(normal, halfVec), 0.0), specularPower);
}

vec3 GetFog(vec3 color, float fogFactor)
{
    return mix(cFogColor, color, fogFactor);
}

float GetFogFactor(float depth)
{
    return clamp((cFogParams.x - depth) * cFogParams.y, 0.0, 1.0);
}


void main()
{
    // Get material diffuse albedo
    vec4 diffInput = texture(diffmap, vTextCoord);
    vec4 diffColor = cMatDiffColor * diffInput;

    vec3 specColor = cMatSpecColor.rgb;

    // Get normal
    vec3 normal = normalize(vNormal);

    // Get fog factor
    float fogFactor = GetFogFactor(vWorldPos.w);

    // Per-pixel forward lighting
    vec3 lightDir;
    float diff = GetDiffuse(normal, vWorldPos.xyz, lightDir);

    vec3 lightColor = cLightColor.rgb;

    // SPECULAR
    float spec = GetSpecular(normal, cCameraPosPS - vWorldPos.xyz, lightDir, cMatSpecColor.a);
    vec3 finalColor = diff * lightColor * (diffColor.rgb + spec * specColor * cLightColor.a);

    // AMBIENT
    finalColor += cAmbientColor * diffColor.rgb;
    finalColor += cMatEmissiveColor;

    fragColor = vec4(GetFog(finalColor, fogFactor), diffColor.a);
}

