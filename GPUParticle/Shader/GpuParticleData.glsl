struct GpuParticleElement
{
  uint isActive;	// �����t���O.
  float lifeTime;	// ��������
  float elapsed;	// �o�ߎ���
  uint colorIndex;
  vec4 position;	// �ʒu
  vec4 velocity;	// �x���V�e�B
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

