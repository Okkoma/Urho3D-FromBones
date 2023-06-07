#version 450

layout(set=2, binding=0) uniform sampler2D uidiffMap;

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTextCoord;

layout(location=0) out vec4 fragColor;

void main()
{
    fragColor.rgb = vColor.rgb;

    float distance = texture(uidiffMap, vTextCoord).a;
	fragColor.a = distance < 0.5 ? 0.0 : vColor.a * smoothstep(0.5, 0.505, distance);
}

