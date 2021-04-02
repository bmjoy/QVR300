/*
 * Deferred inspection Vertex shader
 */
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 pos;
layout (location = 1) in vec4 vertColor;
layout (location = 2) in vec2 vertUV;

layout (location = 0) out vec4 color;
layout (location = 1) out vec2 uv;
layout (location = 2) flat out int vID;

layout(std140, set = 0, binding = 0) uniform VertConstantsBuff 
{
    mat4 projMtx;
    mat4 viewMtx;
    mat4 mdlMtx;
} VertCB;


out gl_PerVertex
{
   vec4 gl_Position;
};

void main() {
  
  color = vertColor;
  
  gl_Position  =  vec4(pos, 1.0f);
  
  uv = vertUV;
  
  vID = gl_VertexIndex;
  //vID - 1;
}
