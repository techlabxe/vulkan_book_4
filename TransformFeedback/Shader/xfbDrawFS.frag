#version 450

#if 0
layout(location=0) in vec3 inColor;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  world;
  mat4  view;
  mat4  proj;
  vec4  lightPos;
  vec4  cameraPos;
};

void main()
{
//  vec3 toEye = normalize(cameraPos.xyz - inWorldPos.xyz);
//  vec3 toLight = normalize(lightPos.xyz);
//  vec3 halfVector = normalize(toEye + toLight);
//  float val = clamp(dot(halfVector, normalize(inNormal)), 0, 1);
//
//  float shininess = 20;
//  float specular = pow(val, shininess);
//
//  vec4 color = inColor;
//  color.rgb += specular;

  outColor = vec4(inColor, 1);
}
#endif

layout(location=0) in vec3 inNormalW;
layout(location=1) in vec2 inUV0;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  view;
  mat4  proj;
  vec4  lightDir;
};

layout(set=0,binding=8)
uniform sampler2D texAlbedo;

void main()
{
  vec4 albedo = texture(texAlbedo, inUV0);
  if( albedo.a < 0.9) {
	discard;
  }
  outColor = albedo;
}
