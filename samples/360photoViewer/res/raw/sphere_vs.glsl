 #version 300 es

uniform mat4 u_proj;
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;

uniform mat4 u_view;
uniform mat4 u_model;
out vec2 v_tex_coord;

out vec4 v_world_pos;

void main() {
    mat4 mvp;
    mat4 model_matrix = u_model;
    mvp = u_proj * u_view * model_matrix;
    v_tex_coord = a_texcoord.xy;
    gl_Position = mvp * vec4(a_position,1.0);

    v_world_pos = model_matrix * vec4(a_position,1.0);
}