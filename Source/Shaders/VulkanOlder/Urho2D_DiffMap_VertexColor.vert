#version 450

layout(location=0) in vec2 inPosition;
layout(location=1) in vec2 inTextCoord;
layout(location=2) in vec4 inColor;
layout(location=3) in int inTextCoord2;
layout(location=4) in int inTextCoord3;
layout(location=5) in int inTextCoord4;

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

/*
vec3 GetWorldPos(mat4 modelMatrix)
{
    return (vec4(inPosition, 1.0) * modelMatrix).xyz;
}

vec4 GetClipPos(vec3 worldPos)
{
    vec4 ret = vec4(worldPos, 1.0) * cViewProj;
    //gl_ClipDistance[0] = dot(cClipPlane, ret);
    return ret;
}
*/

void main()
{
    //mat4 modelMatrix   = cModel;
    //vec3 worldPos      = GetWorldPos(modelMatrix);
    //gl_Position        = GetClipPos(worldPos);

    vec3 worldPos      = (vec4(inPosition, 0.0, 1.0) * cModel).xyz;
    gl_Position        = vec4(worldPos, 1.0) * cViewProj;
    //gl_ClipDistance[0] = dot(cClipPlane, clipPos);

    vColor             = inColor;
    vTextCoord         = inTextCoord;
}

