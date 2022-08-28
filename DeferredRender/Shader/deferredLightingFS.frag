#version 450
#extension GL_GOOGLE_include_directive : enable
#include "SceneParameter.glsl"

layout(input_attachment_index=0,binding=3) uniform subpassInput inPosition;
layout(input_attachment_index=1,binding=4) uniform subpassInput inNormal;
layout(input_attachment_index=2,binding=5) uniform subpassInput inAlbedo;

layout(location=0) in vec2 inUV0;

layout(location=0) out vec4 outColor;

#define LIGHT_NUM (100)

void main()
{
  // GBuffer �̓ǂݎ��.
  // Subpass �ł̓��͂���ǂݎ��̂�texture���߂ł͂Ȃ�.
  vec4 position = subpassLoad(inPosition);
  vec3 normal = subpassLoad(inNormal).xyz;
  vec4 albedo = subpassLoad(inAlbedo);
  float specularMask = albedo.w;
  float specularPower = subpassLoad(inNormal).w;
  vec3 toCameraDir = normalize(cameraPosition.xyz - position.xyz);

  if(position.w < 0.5) {
	discard;
  }

  normal = normalize(normal.xyz);

  // �����o�[�g���C�e�B���O.
  float dotNL = dot(normalize(lightDir.xyz), normal);

  // �X�y�L�����̌v�Z(�}�X�N�l��)
  vec3 r = reflect(-normalize(lightDir.xyz), normal);
  float specular = clamp(dot(r, toCameraDir), 0, 1);
  specular = pow(specular, specularPower) * specularMask;

  vec3 color = vec3(0);
  color += dotNL * albedo.xyz;
  color += albedo.xyz * 0.1; // ambient
  color += specular.xxx;

  // �V�[�����̃|�C���g���C�g�̏���.
  for(int i=0;i<LIGHT_NUM;++i) {
    float lightDistance = length(position.xyz - pointLights[i].xyz);
	if(lightDistance < pointLights[i].w) {
	  // ���C�g�̉e�����󂯂�.
	  float rate = clamp(1.0 - lightDistance / pointLights[i].w, 0, 1);
	  vec3 lightColor = rate * pointLightColors[ i % 8].xyz;
	  color += lightColor * albedo.xyz;
	}
  }
  outColor = vec4(color,1);
}
