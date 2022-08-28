layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  view;
  mat4  proj;
  vec4  lightDir;
  vec4  cameraPosition;

  float frameDeltaTime;
  uint drawMode;
};
