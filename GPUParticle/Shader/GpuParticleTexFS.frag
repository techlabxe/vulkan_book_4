#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location=0) in vec3 inColor;
layout(location=1) in vec2 inUV0;

layout(location=0) out vec4 outColor;

layout(set=0,binding=8)
uniform sampler2D texParticle;

void main() 
{
  vec4 tex = texture(texParticle, inUV0);
  outColor = vec4(inColor * tex.xyz, tex.a);
}


