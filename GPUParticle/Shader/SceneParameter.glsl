layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  view;
  mat4  proj;
  vec4  lightDir;
  vec4  cameraPosition;

  uint  maxParticleCount;
  float frameDeltaTime;
  uint drawFlag;
  uint frameCountLow;

  vec4 particleColors[8];
  vec4 forceCenter1;
  mat4 matBillboard;
};
