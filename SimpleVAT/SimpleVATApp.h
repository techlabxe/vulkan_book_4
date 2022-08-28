#pragma once
#include "VulkanAppBase.h"
#include <glm/glm.hpp>
#include "Camera.h"

class SimpleVATApp : public VulkanAppBase
{
public:
  SimpleVATApp();

  virtual void Prepare();
  virtual void Cleanup();
  virtual void Render();

  virtual bool OnSizeChanged(uint32_t width, uint32_t height);
  virtual bool OnMouseButtonDown(int button);
  virtual bool OnMouseButtonUp(int button);
  virtual bool OnMouseMove(int dx, int dy);

  struct ShaderParameters
  {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 lightDir;
    glm::vec4 cameraPosition;

    float  frameDeltaTime;
    uint32_t drawFlag;
    uint32_t frameCountLow;
    uint32_t animationFrame;
  };

  struct VATData {
    ImageObject texPosition;
    ImageObject texNormal;
    int  vertexCount;
    int  animationCount;

    std::vector<VkDescriptorSet> descriptorSet;
  };
private:
  // 本アプリで使用するレイアウト(ディスクリプタレイアウト/パイプラインレイアウト)を作成.
  void CreateSampleLayouts();

  void PrepareFramebuffers();

  void CreatePipeline();

  void PrepareVATData();

  void PrepareModelResource(ModelAsset& model);

  void RenderHUD(VkCommandBuffer command);

  void DrawModel(VkCommandBuffer command);

  VATData LoadVAT(std::string path);
private:
  ImageObject m_depthBuffer;

  std::vector<VkFramebuffer> m_framebuffers;

  struct FrameCommandBuffer
  {
    VkCommandBuffer commandBuffer;
    VkFence fence;
  };
  std::vector<FrameCommandBuffer> m_commandBuffers;

  std::unordered_map<std::string, VkPipeline> m_pipelines;

  Camera m_camera;
  std::vector<BufferObject> m_uniformBuffers;
  ShaderParameters m_sceneParameters;

  const std::string DrawVATPipeline = "DrawVAT";
  const std::string DrawFloorPipeline = "DrawPlane";

  enum DrawMode
  {
    DrawMode_Fluid,
    DrawMode_Destroy,
  };
  DrawMode m_mode = DrawMode_Fluid;

  enum DESCRIPTORSET_BINDINGS {
    DS_SCENE_UNIFORM = 0,
    DS_MODEL_UNIFORM = 1,

    DS_MATERIAL_ALBEDO = 8,
  };
  enum DESCRIPTORSET_BINDINGS_VAT {
    DS_VAT_SCENE_UNIFORM = 0,
    DS_VAT_MODEL_UNIFORM = 1,
    DS_VAT_VATDATA_POSITION = 8,
    DS_VAT_VATDATA_NORMAL = 9,
  };

  VkSampler m_sampler;
  VkSampler m_vatSampler;
  ModelAsset m_model;

  ImageObject m_texPlaneBase;
  VATData m_vatDestroy, m_vatFluid;
  std::vector<BufferObject> m_materialVATUBO;
  ModelMeshParameters m_materialFluid, m_materialDestroy;

  uint64_t  m_frameCount = 0;
  bool m_animeAuto;
};