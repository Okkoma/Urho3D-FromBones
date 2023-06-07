#version 450

layout(location=0) in vec3 inPosition;
layout(location=1) in vec4 inColor;
layout(location=2) in vec2 inTextCoord;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTextCoord;

layout(set=0, binding=0) uniform CameraVS
{
    mat4 cViewProj;
};

void main()
{
	gl_Position = vec4(inPosition, 1.0) * cViewProj;
    vColor = inColor;
    vTextCoord = inTextCoord;
}

