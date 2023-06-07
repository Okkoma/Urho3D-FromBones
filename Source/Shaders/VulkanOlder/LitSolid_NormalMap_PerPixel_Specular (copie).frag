#version 450

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTextCoord;

layout(location=0) out vec4 fragColor;

layout(set=2, binding=0) uniform sampler2D diffmap;

void main()
{
    fragColor = texture(diffmap, vTextCoord);
}

