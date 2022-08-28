#pragma once
#include "VulkanAppBase.h"
#include <glm/glm.hpp>
#include "Camera.h"

class TransformFeedbackApp : public VulkanAppBase
{
public:
  TransformFeedbackApp();

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

  const std::string GeometryShaderXfbPipeline = "skinnedDraw";
  const std::string VertexShaderXfbSlimPipeline = "xfbVSOnly";
  const std::string CombinedBufferDrawPipeline = "CombinedBufferDraw";

  enum DrawMode
  {
    DrawMode_GS_XFB,
    DrawMode_VS_XFB,
  };
  DrawMode m_mode = DrawMode_GS_XFB;

  enum DESCRIPTORSET_BINDINGS {
    // DescriptorSet:0
    DS_SCENE_UNIFORM = 0,
    DS_MODEL_UNIFORM = 1,
    DS_BONE_UNIFORM = 2,

    DS_MATERIAL_ALBEDO = 8,

  };

  PFN_vkCmdBeginTransformFeedbackEXT _vkCmdBeginTransformFeedbackEXT;
  PFN_vkCmdEndTransformFeedbackEXT   _vkCmdEndTransformFeedbackEXT;
  PFN_vkCmdBindTransformFeedbackBuffersEXT _vkCmdBindTransformFeedbackBuffersEXT;

  VkSampler m_sampler;
  ModelAsset m_model;

  VkPipelineLayout m_pipelineLayout;
};