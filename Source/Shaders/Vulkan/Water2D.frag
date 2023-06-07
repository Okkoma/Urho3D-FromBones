#version 450

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTextCoord;
layout(location=2) in vec2 vRefractUV;

layout(set=1, binding=1) uniform FramePS
{
	float cElapsedTimePS;
};

layout(set=2, binding=0) uniform sampler2D diffmap;
//layout(set=3, binding=0) uniform sampler2D normalmap;

layout(location=0) out vec4 fragColor;


void main()
{
	const float freq = 35.0;
	const float accel = 5.0;
	const float amplitude = 0.0002;
    vec2 refractUV = vRefractUV;

    float variation = amplitude * sin(freq * refractUV.x + accel * cElapsedTimePS) + amplitude * cos(freq * refractUV.y + accel * cElapsedTimePS);
    refractUV.x += variation;
    refractUV.y += variation;

    // refracted color from the refraction map (texUnit=0 is the viewport textured getted at the previous pass in ForwardUrho2D renderpath)
    vec3 refractColor = texture(diffmap, refractUV).rgb;

    // diffmap color
//    vec3 waterColor   = texture(normalmap, vTextCoord).rgb;

//    fragColor = vec4(/*cMatDiffColor.rgb * */ refractColor * waterColor * vColor.rgb, 1.0);
    fragColor = vec4(refractColor * vColor.rgb, 1.0);
}
