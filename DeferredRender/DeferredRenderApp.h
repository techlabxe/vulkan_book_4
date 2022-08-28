#pragma once
#include "VulkanAppBase.h"
#include <glm/glm.hpp>
#include "Camera.h"

class DeferredRenderApp : public VulkanAppBase
{
public:
  DeferredRenderApp();

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

    glm::vec4 pointLightColors[8];
    glm::vec4 pointLights[100];
  };

private:
  // 本アプリで使用するレイアウト(ディスクリプタレイアウト/パイプラインレイアウト)を作成.
  void CreateSampleLayouts();

  void PrepareFramebuffers();

  void CreatePipeline();

  void PrepareModelResource(ModelAsset& model);

  void RenderHUD(VkCommandBuffer command);

  void DrawModel(VkCommandBuffer command);

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

  const std::string DepthPrepassPipeline = "DepthPrepass";
  const std::string DrawGBufferPipeline = "DrawGBuffer";
  const std::string LightingPassPipeline = "DeferredLightingPass";

  enum AttachmentIndex {
    AttachmentBackbuffer = 0,
    AttachmentDepth = 1,
    AttachmentGbufferPosition = 2,
    AttachmentGbufferNormal = 3,
    AttachmentGbufferAlbedo = 4,
  };
  enum Subpass {
    SubpassDepthPrepass = 0,
    SubpassGbuffer = 1,
    SubpassLighting = 2,
  };
  enum DrawMode
  {
    DrawMode_Fluid,
    DrawMode_Destroy,
  };
  DrawMode m_mode = DrawMode_Fluid;

  enum DESCRIPTORSET_BINDINGS_DRAW {
    DS_DRAW_SCENE_UNIFORM = 0,
    DS_DRAW_MODEL_UNIFORM = 1,
    DS_DRAW_MATERIAL_ALBEDO = 8,
    DS_DRAW_MATERIAL_SPECULAR = 9,
  };
  enum DESCRIPTORSET_BINDINGS_DEFERRED_LIGHTING {
    DS_DEFERRED_SCENE_UNIFORM = 0,
    DS_DEFERRED_IN_POSITION = 3,
    DS_DEFERRED_IN_NORMAL = 4,
    DS_DEFERRED_IN_ALBEDO = 5,
  };

  VkSampler m_sampler;
  ModelAsset m_model;

  ImageObject m_rtPosition;
  ImageObject m_rtNormal;
  ImageObject m_rtAlbedo;
  std::vector<VkFramebuffer> m_fbGbuffers;

  std::vector<VkDescriptorSet> m_dsGbuffer;
  std::vector<VkDescriptorSet> m_dsDeferredLighting;

  uint64_t  m_frameCount = 0;
};