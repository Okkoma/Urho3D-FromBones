#version 450

layout(location=0) in vec2 inPosition;
layout(location=1) in vec2 inTextCoord;
layout(location=2) in vec4 inColor;
layout(location=3) in float inPositionZ;
layout(location=4) in int inTextCoord2;
layout(location=5) in int inTextCoord3;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTextCoord;
layout(location=2) out vec2 vRefractUV;

layout(set=0, binding=0) uniform CameraVS
{
    mat4 cViewProj;
};

layout(set=1, binding=0) uniform FrameVS
{
    float cElapsedTime;
};

vec2 GetQuadTexCoord(vec4 clipPos)
{
    return vec2(
        clipPos.x / clipPos.w * 0.5 + 0.5,
        clipPos.y / clipPos.w * 0.5 + 0.5);
}

void main()
{
	gl_Position = vec4(inPosition, inPositionZ, 1.0) * cViewProj;

    vColor      = inColor;
    vTextCoord  = inTextCoord + vec2(0.25 * sin(cElapsedTime), 0.0);
    vRefractUV  = GetQuadTexCoord(gl_Position);
}
