/*
 * Deferred Inspect Fragment shader 
 */
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (set = 0, binding = 0) uniform sampler2D colorTex;

layout (location = 0) in vec4 colorVert;
layout (location = 1) in vec2 uv;
layout (location = 2) flat in int vID;

layout(std140, set = 0, binding = 1) uniform FragConstantsBuff 
{
    vec4    color;
    vec4    ambient;
    vec4    eyePos;
    vec4    lightPos;
    vec4    lightColor;
} FragCB;

#define saturate(x) clamp( x, 0.0, 1.0 )

layout (location = 0) out vec4 FragColor;

void main() 
{
    FragColor = vec4(texture( colorTex, uv ).xyz, 1.0f);
}
