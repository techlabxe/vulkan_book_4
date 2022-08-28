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
uniform sampler2D texMovie0;
layout(set=0,binding=9)
uniform sampler2D texMovie1;

layout(set=0,binding=10)
uniform sampler2D texMovie2;

void main()
{
  vec3 color = vec3(0);

  if(drawMode == 0) {
#if 1
    color = texture(texMovie0, inUV0).xyz;
#else
    // for YUV
    float y = texture(texMovie0, inUV0).x;
    vec2 uv = texture(texMovie2, inUV0).xy - 0.5;

    vec3 yuv = vec3(y, uv.x, uv.y);
    mat3 m = mat3( 
      vec3(1.0, 0, 1.402),
      vec3(1.0, -0.344, -0.714),
      vec3(1.0, 1.772, 0) );
    color = yuv * m;
#endif

  }
  if(drawMode == 1) {
    color = texture(texMovie1, inUV0).xyz;
  }

  outColor = vec4(color.xyz, 1);
}
