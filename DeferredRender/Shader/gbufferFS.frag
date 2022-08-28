#version 450
#extension GL_GOOGLE_include_directive : enable
#include "SceneParameter.glsl"


layout(location=0) in vec4 inPositionW;
layout(location=1) in vec3 inNormalW;
layout(location=2) in vec2 inUV0;

layout(location=0) out vec4 outPosition;
layout(location=1) out vec4 outNormal;
layout(location=2) out vec4 outAlbedo;

layout(set=0,binding=8)
uniform sampler2D texAlbedo;
layout(set=0,binding=9)
uniform sampler2D texSpecularMask;

layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
    vec4 diffuse;
    vec4 ambient;
};


void main()
{
  vec4 albedo = texture(texAlbedo, inUV0);
  if(albedo.a < 0.5) {
    discard;
  }
  float specularMask = texture(texSpecularMask, inUV0).r;
  float specularShininess = diffuse.w;

  outPosition = inPositionW;
  outNormal = vec4(inNormalW, specularShininess);
  outAlbedo = vec4(albedo.xyz, specularMask);
}
