#version 450

layout(location=0) in vec3 inPosition;


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
    vec3 worldPos = (vec4(inPosition, 1.0) * cModel).xyz;
    gl_Position   = vec4(worldPos, 1.0) * cViewProj;	

}

