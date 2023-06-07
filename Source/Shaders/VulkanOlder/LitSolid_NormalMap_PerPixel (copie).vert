#version 450

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inTextCoord;
layout(location=3) in vec4 inColor;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTextCoord;

layout(set=0, binding=0) uniform CameraVS
{
//    vec3 cCameraPos;
//    float cNearClip;
//    float cFarClip;
//    vec4 cDepthMode;
//    vec3 cFrustumSize;
//    vec4 cGBufferOffsets;
//    mat4 cView;
//    mat4 cViewInv;
    mat4 cViewProj;
//    vec4 cClipPlane;
};

layout(set=1, binding=0) uniform ObjectVS
{
    mat4 cModel;
};


void main()
{
    vec3 worldPos = (vec4(inPosition, 1.0) * cModel).xyz;
    gl_Position   = vec4(worldPos, 1.0) * cViewProj;
    
    vColor = inColor;
    vTextCoord = inTextCoord;
}

