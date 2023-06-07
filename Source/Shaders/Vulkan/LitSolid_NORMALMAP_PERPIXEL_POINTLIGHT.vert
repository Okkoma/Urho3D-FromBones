#version 450

/// vs=DIRLIGHT NORMALMAP PERPIXEL

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTextCoord;
layout(location=3) in vec4 inColor;
//layout(location=4) in vec4 inTangent;

layout(location=0) out vec3 vNormal;
layout(location=1) out vec4 vWorldPos;
layout(location=2) out vec4 vColor;
layout(location=3) out vec4 vTextCoord;
layout(location=4) out vec4 vTangent;

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

    mat3 normalMatrix = mat3(cModel[0].xyz, cModel[1].xyz, cModel[2].xyz);

    vNormal = normalize(inNormal * normalMatrix);
    vWorldPos = vec4(worldPos, GetDepth(gl_Position));

    vColor = inColor;

    vec3 tangent = normalize(vec3(1.0,1.0,1.0) * normalMatrix);
    vec3 bitangent = cross(tangent, vNormal);
    vTextCoord = vec4(GetTexCoord(inTextCoord), bitangent.xy);
    vTangent = vec4(tangent, bitangent.z);
}

