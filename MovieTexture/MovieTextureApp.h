#pragma once
#include "VulkanAppBase.h"
#include <glm/glm.hpp>
#include "Camera.h"

#include "ManualMoviePlayer.h"
#include "MoviePlayer.h"

class MovieTextureApp : public VulkanAppBase
{
public:
  MovieTextureApp();

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
    uint32_t drawMode;
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

  const std::string DrawModelPipeline = "DrawModel";

  enum PlayerType
  {
    Mode_PlayerManual = 0,
    Mode_PlayerStd,
  };

  enum DESCRIPTORSET_BINDINGS {
    DS_SCENE_UNIFORM = 0,
    DS_MODEL_UNIFORM = 1,

    DS_MOVIE_TEX_0 = 8,
    DS_MOVIE_TEX_1 = 9,
    DS_MOVIE_TEX_2 = 10,
  };

  VkSampler m_sampler;
  VkSampler m_samplerY;
  ModelAsset m_model;

  std::shared_ptr<ManualMoviePlayer> m_moviePlayerManual;
  std::unique_ptr<MoviePlayer> m_moviePlayer;
};