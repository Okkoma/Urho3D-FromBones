#version 450

layout(set=1, binding=0) uniform sampler2D alphaMap;

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTextCoord;

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor.rgb = vColor.rgb;
    fragColor.a = vColor.a * texture(alphaMap, vTextCoord).r;
}

