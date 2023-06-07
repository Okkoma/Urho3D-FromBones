#version 450

layout(location=0) in vec2 inPosition;
layout(location=1) in vec2 inColor;

layout(location=0) out vec4 fragColor;

void main()
{
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
    fragColor = vec4(inColor, 0.0, 1.0);
}
