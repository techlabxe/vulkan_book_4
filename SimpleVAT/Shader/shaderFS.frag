#version 450
#extension GL_GOOGLE_include_directive : enable
#include "SceneParameter.glsl"


layout(location=0) in vec3 inNormalW;
layout(location=1) in vec2 inUV0;

layout(location=0) out vec4 outColor;

layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
};

layout(set=0,binding=8)
uniform sampler2D texAlbedo;

void main()
{
  vec4 albedo = texture(texAlbedo, inUV0);
  float dotNL = clamp(dot(inNormalW, normalize(lightDir.xyz)), 0, 1);

  vec3 color = vec3(0);
  color += albedo.xyz * dotNL;
  outColor = vec4(color, 1);
}
