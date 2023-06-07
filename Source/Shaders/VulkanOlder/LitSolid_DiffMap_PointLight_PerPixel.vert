#version 450

/// vs=DIFFMAP POINTLIGHT PERPIXEL

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTextCoord;
layout(location=3) in vec4 inColor;

layout(location=0) out vec3 vNormal;
layout(location=1) out vec4 vWorldPos;
layout(location=2) out vec4 vColor;
layout(location=3) out vec2 vTextCoord;

layout(set=0, binding=0) uniform CameraVS
{
    vec4 cDepthMode;
    mat4 cViewProj;
};

layout(set=1, binding=0) uniform MaterialVS
{
    vec4 cUOffset;
    vec4 cVOffset;
};

layout(set=2, binding=0) uniform ObjectVS
{
    mat4 cModel;
};


mat3 GetNormalMatrix(mat4 modelMatrix)
{
    return mat3(modelMatrix[0].xyz, modelMatrix[1].xyz, modelMatrix[2].xyz);
}

vec3 GetWorldNormal(mat4 modelMatrix)
{
    return normalize(inNormal * GetNormalMatrix(modelMatrix));
}

float GetDepth(vec4 clipPos)
{
    return dot(clipPos.zw, cDepthMode.zw);
}

vec2 GetTexCoord(vec2 texCoord)
{
    return vec2(dot(texCoord, cUOffset.xy) + cUOffset.w, dot(texCoord, cVOffset.xy) + cVOffset.w);
}

void main()
{
    vec3 worldPos = (vec4(inPosition, 1.0) * cModel).xyz;
    gl_Position   = vec4(worldPos, 1.0) * cViewProj;

    vNormal = GetWorldNormal(cModel);
    vWorldPos = vec4(worldPos, GetDepth(gl_Position));

    vColor = inColor;
    vTextCoord = GetTexCoord(inTextCoord);
}

