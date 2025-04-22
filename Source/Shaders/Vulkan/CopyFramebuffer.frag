#version 450

layout(location=0) in vec2 vScreenPos;

layout(set=1, binding=0) uniform CameraPS
{
    vec2 cGBufferInvSize;
};

layout(set=2, binding=0) uniform sampler2D sDiffMap[1];

layout(location=0) out vec4 fragColor;

void main()
{	
    fragColor = texture(sDiffMap[0], vScreenPos);
}
