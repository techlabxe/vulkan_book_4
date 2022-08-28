#version 450

layout(location=0) in vec3 inNormalW;
layout(location=1) in vec2 inUV0;
layout(location=2) in vec3 inTangentW;
layout(location=3) in vec3 inPositionW;

layout(location=0) out vec4 outColor;

layout(set=0, binding=0)
uniform SceneParameters
{
  mat4  view;
  mat4  proj;
  vec4  lightDir;

  vec4  cameraPosition;

  uint  drawFlag;
  float heightScale;
};

layout(set=0,binding=1)
uniform ModelMeshParamters
{
    mat4 world;
};

layout(set=0,binding=8)
uniform sampler2D texAlbedo;

layout(set=0,binding=9)
uniform sampler2D texNormalMap;

layout(set=0,binding=10)
uniform sampler2D texHeightMap;

vec3 FetchNormalMap(vec2 uv)
{
  vec3 normal = texture(texNormalMap, uv).xyz;
  normal = normalize(normal * 2 - 1);
  return normal;
}
float FetchHeightMap(vec2 uv)
{
  return texture(texHeightMap, uv).r;
}

vec3 ComputeBinormal()
{
  return cross(inNormalW.xyz, inTangentW.xyz);
}

vec3 GetWorldNormal(vec2 uv)
{
  vec3 normal = FetchNormalMap(uv);  
  vec3 binormal = ComputeBinormal();

  vec3 v = normal;
  vec3 worldNormal = inTangentW.xyz * v.x + binormal.xyz * v.y + inNormalW.xyz * v.z;
  return normalize(worldNormal);
}

vec3 GetEyeDirectionTS() {
  vec3 toEyeDir = normalize(cameraPosition.xyz - inPositionW.xyz);
  vec3 tangentW = inTangentW.xyz;
  vec3 normalW = inNormalW.xyz;
  vec3 binormalW = ComputeBinormal();
  vec3 toEyeDirTS;
  toEyeDirTS.x = dot(toEyeDir, tangentW);
  toEyeDirTS.y = dot(toEyeDir, binormalW);
  toEyeDirTS.z = dot(toEyeDir, normalW);
  return toEyeDirTS;
}


vec2 ParallaxMapping()
{
  float height = FetchHeightMap(inUV0);
  float heightBias = 0;
  height = height * heightScale + heightBias;

  // 接空間での視線ベクトル.
  vec3 toEye = GetEyeDirectionTS();
  
  // オフセットを計算.
  vec2 uv = inUV0.xy + height * toEye.xy;
  return uv;
}

vec2 ParallaxOcclusionMapping()
{
  // 接空間での視線ベクトル.
  vec3 toEye = GetEyeDirectionTS();
  vec2 rayDirectionTS = -toEye.xy / toEye.z;
  float hScale = 0.2;
  vec2 maxParallaxOffset = rayDirectionTS * hScale;

  const int sampleCount = 32;
  float zStep = 1.0 / sampleCount;
  vec2 texStep = maxParallaxOffset * zStep;

  float rayZ = 1.0;
  int sampleIndex = 0;

  vec2 texUV = inUV0;
  float height = FetchHeightMap(texUV);
  float prevHeight = height;
  vec2  prevUV = texUV;
  while (height < rayZ && sampleIndex < sampleCount) {
    prevHeight = height;
    prevUV = texUV;

    texUV += texStep;
    height = FetchHeightMap(texUV);
    rayZ -= zStep;
    sampleIndex++;
  }

  float   prevRayZ = rayZ + zStep;
  float t = (prevHeight - prevRayZ) / (prevHeight - height + rayZ - prevRayZ);
  texUV = prevUV + t * texStep;
  return texUV;
}

void main()
{
  vec2 texUV = inUV0;
  if( drawFlag == 1) {
    texUV = ParallaxMapping();
  }
  if( drawFlag == 2) {
    texUV = ParallaxOcclusionMapping();
  }

  vec3 worldNormal = GetWorldNormal(texUV);
  vec3 lightDirection = normalize(lightDir.xyz);

  vec3 albedo = texture(texAlbedo, texUV).xyz;
  //outColor = albedo;
  outColor.xyz = worldNormal * 0.5 +0.5;
  vec3 color = vec3(0);
  float dotNL = clamp(dot(worldNormal, lightDirection), 0, 1);
  color += albedo * dotNL;
  outColor.xyz = color;
  outColor.a = 1;
}
