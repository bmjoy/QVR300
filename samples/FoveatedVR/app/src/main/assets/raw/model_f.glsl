#version 300 es

in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vTexcoord0;

uniform vec3 eyePos;
uniform vec3 modelColor;

uniform sampler2D srcTex;

out highp vec4 outColor;

//--------------------------------------------------------------------------------------
void main()
//--------------------------------------------------------------------------------------
{
    vec4 srcColor = texture(srcTex, vTexcoord0);
    vec4 FinalColor  = vec4(srcColor.xyz, 1.0);
    outColor = FinalColor;
}

