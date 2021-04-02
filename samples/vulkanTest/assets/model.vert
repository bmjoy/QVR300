//
// Model.vert
//

#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// Uniform Constant Buffer
layout(std140, set = 0, binding = 0) uniform VertConstantsBuff 
{
    mat4 projMtx;
    mat4 viewMtx;
    mat4 mdlMtx;
} VertCB;

layout (location = 0) in vec3 position;
layout (location = 3) in vec3 normal;

// Varying's
layout (location = 0) out vec3 vWorldPos;
layout (location = 1) out vec3 vWorldNormal;

void main()
{
    gl_Position = VertCB.projMtx * (VertCB.viewMtx * (VertCB.mdlMtx * vec4(position, 1.0))); 
    vWorldPos = (VertCB.mdlMtx * vec4(position.xyz, 1.0)).xyz;

    // Only rotate the rest of these! 
    vWorldNormal = (VertCB.mdlMtx * vec4(normal.xyz, 0.0)).xyz;
}

