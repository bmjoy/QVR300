#version 300 es


precision highp float;
out vec4 out_color;
uniform sampler2D u_texture;
uniform vec3 u_color;
uniform float u_opacity;
in vec2 v_tex_coord;

in vec4 v_world_pos;

void main()
{
  vec4 color;
  color = texture(u_texture, v_tex_coord);
  out_color = vec4(color.r * u_color.r * u_opacity, color.g * u_color.g * u_opacity, color.b * u_color.b * u_opacity, color.a * u_opacity);
}
