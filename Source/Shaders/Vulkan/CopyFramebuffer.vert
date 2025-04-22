#version 450

layout(location=0) in vec3 inPosition;

layout(location=0) out vec2 vScreenPos;

layout(set=0, binding=0) uniform CameraVS
{
    mat4 cViewProj;
    vec4 cClipPlane;
    vec4 cGBufferOffsets;    
};

layout(set=0, binding=1) uniform ObjectVS
{
    mat4 cModel;
};

vec4 GetClipPos(vec3 worldPos)
{
    vec4 ret = vec4(worldPos, 1.0) * cViewProj;
    gl_ClipDistance[0] = dot(cClipPlane, ret);
    return ret;
}

vec3 GetWorldPos(mat4 modelMatrix)
{
    return (vec4(inPosition, 0.0) * modelMatrix).xyz;
}

vec2 GetScreenPosPreDiv(vec4 clipPos)
{
    return vec2(
        clipPos.x / clipPos.w * cGBufferOffsets.z + cGBufferOffsets.x,
        clipPos.y / clipPos.w * cGBufferOffsets.w + cGBufferOffsets.y);
}

void main()
{
    vec3 worldPos = GetWorldPos(cModel);
    gl_Position = GetClipPos(worldPos);
    vScreenPos  = GetScreenPosPreDiv(gl_Position);     
}


