struct GpuParticleElement
{
  uint isActive;	// 生存フラグ.
  float lifeTime;	// 生存時間
  float elapsed;	// 経過時間
  uint colorIndex;
  vec4 position;	// 位置
  vec4 velocity;	// ベロシティ
};

layout(std430,set=0,binding=1)
buffer ParticleElements {
  GpuParticleElement gpuParticleElements[];
};
layout(std430,set=0,binding=2)
buffer PrticleIndexList {
  uint headIndex;
  uint indices[];
};

