#version 300 es

#extension GL_OVR_multiview : enable
#extension GL_OVR_multiview2 : enable
#extension GL_OVR_multiview_multisampled_render_to_texture : enable

layout(num_views = 2) in;

in vec3 position;
in vec3 normal;
uniform mat4 projMtx;
uniform mat4 viewMtx[2];
uniform mat4 mdlMtx;
out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec3 testColor;
void main()
{
    gl_Position = projMtx * (viewMtx[gl_ViewID_OVR] * (mdlMtx * vec4(position, 1.0))); 
    vWorldPos = (mdlMtx * vec4(position.xyz, 1.0)).xyz;

    // Only rotate the rest of these! 
    vWorldNormal = (mdlMtx * vec4(normal.xyz, 0.0)).xyz; 

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    testColor = vec3(1.0 - float(gl_ViewID_OVR), float(gl_ViewID_OVR), 0.0);
}
