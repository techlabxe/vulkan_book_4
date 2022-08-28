#version 450

layout(location=0) in vec4 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV0;

layout(location=3) in uvec4 inBlendIndices;
layout(location=4) in vec4 inBlendWeights;

layout(xfb_stride=32, xfb_offset=0, xfb_buffer=0, location=0)
out vec3 outPosition;

layout(xfb_stride=32, xfb_offset=12, xfb_buffer=0, location=1)
out vec3 outNormal;

layout(xfb_stride=32, xfb_offset=24, xfb_buffer=0, location=2)
out vec2 outUV0;


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

layout(set=0,binding=2)
uniform BoneMatrices
{
  mat4 boneMatrices[512];
};

vec4 TransformPosition() 
{
  vec4 pos = vec4(0);
  uint indices[4] = uint[4](inBlendIndices.x, inBlendIndices.y, inBlendIndices.z, inBlendIndices.w);
  float weights[4] = float[4](
    inBlendWeights.x, inBlendWeights.y, inBlendWeights.z, inBlendWeights.w
  );

  for(int i=0;i<4;++i) {
    mat4 mtx = boneMatrices[indices[i]];
    pos += mtx * inPos * weights[i];
  }
  pos.w = 1;
  return pos;
}
vec3 TransformNormal()
{
  vec3 nrm = vec3(0);
  uint indices[4] = uint[4](inBlendIndices.x, inBlendIndices.y, inBlendIndices.z, inBlendIndices.w);
  float weights[4] = float[4](
    inBlendWeights.x, inBlendWeights.y, inBlendWeights.z, inBlendWeights.w
  );
  for(int i=0;i<4;++i) {
    mat3 mtx = mat3(boneMatrices[indices[i]]);
    nrm += mtx * inNormal * weights[i];
  }
  return normalize(nrm);
}


void main()
{
  vec4 position = TransformPosition();
  outPosition = position.xyz;
  outNormal = TransformNormal();
  outUV0 = inUV0;
}
