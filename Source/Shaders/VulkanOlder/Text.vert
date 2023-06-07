#version 450

layout(location=0) in vec2 inPosition;
layout(location=1) in vec2 inTextCoord;
layout(location=2) in vec4 inColor;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTextCoord;

layout(set=0, binding=0) uniform CameraVS
{
    mat4 cViewProj;
};

layout(set=1, binding=0) uniform ObjectVS
{
    mat4 cModel;
};

void main()
{
    //gl_Position = vec4(inPosition.xy, 0.0, 1.0);
    
    gl_Position = vec4(inPosition, 0.0, 1.0) * cViewProj;
    
    //vec3 worldPos      = (vec4(inPosition.xy, 0.f, 1.0) * cModel).xyz;
    //gl_Position        = vec4(worldPos, 1.0) * cViewProj;
    //gl_Position.y      = -gl_Position.y;
    //gl_Position.z      = (gl_Position.z + gl_Position.w) / 2.0;
    
    vColor = inColor;
    vTextCoord = inTextCoord;
}

