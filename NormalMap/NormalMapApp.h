#pragma once
#include "VulkanAppBase.h"
#include <glm/glm.hpp>
#include "Camera.h"

class NormalMapApp : public VulkanAppBase
{
public:
  NormalMapApp();

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
  
    uint32_t drawFlag;
    float heightScale;
    float heightScalePOM = 0.2f;
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

  const std::string NormalMapPipeline = "NormalMap";

  enum DrawMode
  {
    DrawMode_GS_XFB,
    DrawMode_VS_XFB,
  };
  DrawMode m_mode = DrawMode_GS_XFB;

  enum DESCRIPTORSET_BINDINGS {
    DS_SCENE_UNIFORM = 0,
    DS_MODEL_UNIFORM = 1,

    DS_MATERIAL_ALBEDO = 8,
    DS_MATERIAL_NORMALMAP = 9,
    DS_MATERIAL_HEIGHTMAP = 10,
  };

  VkSampler m_sampler;
  ModelAsset m_model;

  VkPipelineLayout m_pipelineLayout;

  ImageObject m_texPlaneBase;
  ImageObject m_texNormalMap;
  ImageObject m_texHeightMap;
};