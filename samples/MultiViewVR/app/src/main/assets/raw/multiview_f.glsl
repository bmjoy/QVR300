#version 300 es
uniform vec3 modelColor;
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec3 testColor;
out highp vec4 outColor;
void main()
{
    vec3 Nn = normalize(vWorldNormal);
    vec3 LightPos = vec3(0.0, 0.0, 0.0);
    vec3 ToLight = normalize(LightPos.xyz - vWorldPos.xyz);
    float diffuse = clamp(dot(Nn, ToLight), 0.0, 1.0);
    outColor = vec4( diffuse * modelColor, 1.0);

    // DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! DEBUG! 
    // outColor = vec4( diffuse * testColor.xyz, 1.0);
}
