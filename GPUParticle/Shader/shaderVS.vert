#version 450
#extension GL_GOOGLE_include_directive : enable
#include "SceneParameter.glsl"

layout(location=0) in vec4 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV0;

layout(location=0) out vec3 outNormalW;
layout(location=1) out vec2 outUV0;

layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
};

void main()
{
  vec4 worldPos = world * inPos;
  gl_Position = proj * view * worldPos;
  mat3 m = mat3(world);
  outNormalW = m * inNormal;
  outUV0 = inUV0;
}
