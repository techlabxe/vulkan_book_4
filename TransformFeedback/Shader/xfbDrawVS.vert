#version 450

#if 0
layout(location=0) in vec4 inPos;
layout(location=1) in vec3 inNormal;

//layout(location=0) out vec3 outNormal;

out gl_PerVertex
{
  vec4 gl_Position;
};

layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  world;
  mat4  view;
  mat4  proj;
  vec4  lightDir;
};

#if 01
layout(xfb_stride=24, xfb_offset=0, xfb_buffer=0, location=0)
out vec3 vOutPosition;
layout(xfb_stride=24, xfb_offset=12, xfb_buffer=0, location=1)
out vec3 vOutNormal;
#endif


void main()
{
  gl_Position = inPos;
  //outNormal = mat3(world) * inNormal;

  vOutPosition = inPos.xyz;
  vOutNormal = inNormal.xyz;
}
#endif

layout(location=0) in vec4 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV0;

layout(location=0) out vec3 outNormalW;
layout(location=1) out vec2 outUV0;

layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  view;
  mat4  proj;
  vec4  lightDir;
};
layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
};

void main()
{
  gl_Position = proj * view * world * inPos;
  outNormalW = mat3(world) * inNormal;
  outUV0 = inUV0;
}
