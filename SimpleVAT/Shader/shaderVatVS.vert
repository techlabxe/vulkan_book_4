#version 450
#extension GL_GOOGLE_include_directive : enable
#include "Shader/SceneParameter.glsl"

layout(location=0) out vec4 outColor;

layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
    vec4 diffuse;
    vec4 ambient;
};

layout(set=0,binding=8)
uniform sampler2D texPosition;

layout(set=0,binding=9)
uniform sampler2D texNormal;

layout(set=0,binding=10)
uniform texture2D texPosition2;

void main()
{
  ivec2 loc;
  loc.x = gl_VertexIndex;
  loc.y = animationFrame;

  vec4 position = texelFetch(texPosition, loc, 0);
  vec4 normal = texelFetch(texNormal, loc, 0);

  if(position.w < 0.5) {
    position.w = -1;
  }
  mat4 mtxPV = proj * view;
  vec4 worldPosition = world * position;
  vec3 worldNormal = mat3(world) * normal.xyz;

  gl_Position = mtxPV * worldPosition;

  float dotNL = clamp(dot(normalize(lightDir.xyz), worldNormal), 0, 1);
  outColor.xyz = dotNL * diffuse.xyz + ambient.xyz;
  outColor.w = 1;
}
