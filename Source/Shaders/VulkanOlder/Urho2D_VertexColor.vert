#version 450

layout(location=0) in vec3 inPosition;
layout(location=1) in vec4 inColor;
layout(location=2) in vec2 inTextCoord;
layout(location=3) in vec4 inTangent;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTextCoord;

void main()
{
    gl_Position = vec4(inPosition.xy, 0.f, 1.f);

    vColor = inColor;
    vTextCoord = inTextCoord;
}

