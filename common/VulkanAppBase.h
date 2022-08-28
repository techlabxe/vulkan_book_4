#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <filesystem>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan_win32.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>

#include "Swapchain.h"

template<class T>
class VulkanObjectStore
{
public:
  VulkanObjectStore(std::function<void(T)> disposer) : m_disposeFunc(disposer) { }
  void Cleanup() {
    std::for_each(m_storeMap.begin(), m_storeMap.end(), [&](auto v) { m_disposeFunc(v.second); });
  }

  void Register(const std::string& name, T data)
  {
    m_storeMap[name] = data;
  }
  T Get(const std::string& name) const
  {
    auto it = m_storeMap.find(name);
    if (it == m_storeMap.end())
    {
      return VK_NULL_HANDLE;
    }
    return it->second;
  }
private:
  std::unordered_map<std::string, T> m_storeMap;
  std::function<void(T)> m_disposeFunc;
};

class VulkanAppBase {
public:
  VulkanAppBase() :m_isMinimizedWindow(false), m_isFullscreen(false) { }
  virtual ~VulkanAppBase() { }

  virtual bool OnSizeChanged(uint32_t width, uint32_t height);
  virtual bool OnMouseButtonDown(int button);
  virtual bool OnMouseButtonUp(int button);
  virtual bool OnMouseMove(int dx, int dy);

  uint32_t GetMemoryTypeIndex(uint32_t requestBits, VkMemoryPropertyFlags requestProps) const;
  void SwitchFullscreen(GLFWwindow* window);

  void Initialize(GLFWwindow* window, VkFormat format, bool isFullscreen);
  void Terminate();

  virtual void Render() = 0;
  virtual void Prepare() = 0;
  virtual void Cleanup() = 0;

  VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }
  VkDevice GetDevice() { return m_device; }
  VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
  VkInstance GetVulkanInstance() const { return m_vkInstance; }
  const Swapchain* GetSwapchain() const { return m_swapchain.get(); }

  VkPipelineLayout GetPipelineLayout(const std::string& name) { return m_pipelineLayoutStore->Get(name); }
  VkDescriptorSetLayout GetDescriptorSetLayout(const std::string& name) { return m_descriptorSetLayoutStore->Get(name); }
  VkRenderPass GetRenderPass(const std::string& name) { return m_renderPassStore->Get(name); }

  void RegisterLayout(const std::string& name, VkPipelineLayout layout) { m_pipelineLayoutStore->Register(name, layout); }
  void RegisterLayout(const std::string& name, VkDescriptorSetLayout layout) { m_descriptorSetLayoutStore->Register(name, layout); }
  void RegisterRenderPass(const std::string& name, VkRenderPass renderPass) { m_renderPassStore->Register(name, renderPass); }
  struct BufferObject
  {
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkDeviceSize size{};
  };
  struct ImageObject
  {
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView view{};
    uint32_t width;
    uint32_t height;
    VkFormat format;
  };

  BufferObject CreateBuffer(uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
  ImageObject CreateTexture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memPropsFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkFramebuffer CreateFramebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height, uint32_t viewCount, VkImageView* views);
  VkFence CreateFence();
  VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout dsLayout);

  void DestroyBuffer(BufferObject bufferObj);
  void DestroyImage(ImageObject imageObj);
  void DestroyFramebuffers(uint32_t count, VkFramebuffer* framebuffers);
  void DestroyFence(VkFence fence);
  void DeallocateDescriptorSet(VkDescriptorSet dsLayout);

  VkCommandBuffer CreateCommandBuffer(bool bBegin = true);
  void FinishCommandBuffer(VkCommandBuffer command);
  void DestroyCommandBuffer(VkCommandBuffer command);

  VkRect2D GetSwapchainRenderArea() const;

  std::vector<BufferObject> CreateUniformBuffers(uint32_t size, uint32_t imageCount);

  // ホストから見えるメモリ領域にデータを書き込む.以下バッファを対象に使用.
  // - ステージングバッファ
  // - ユニフォームバッファ
  void WriteToHostVisibleMemory(VkDeviceMemory memory, uint64_t size, const void* pData);
  void WriteToDeviceLocalMemory(BufferObject dstBuffer, const void* pSrcData, size_t size, VkCommandBuffer command = VK_NULL_HANDLE, BufferObject* stagingBufferUsed = nullptr);

  void AllocateCommandBufferSecondary(uint32_t count, VkCommandBuffer* pCommands);
  void FreeCommandBufferSecondary(uint32_t count, VkCommandBuffer* pCommands);

  void TransferStageBufferToImage(const BufferObject& srcBuffer, const ImageObject& dstImage, const VkBufferImageCopy* region);


  // レンダーパスの生成.
  VkRenderPass CreateRenderPass(VkFormat colorFormat, VkFormat depthFormat = VK_FORMAT_UNDEFINED, VkImageLayout layoutColor = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  void SetFrameDeltaTime(double t) { m_frameDeltaTime = t; }
  double GetFrameDeltaTime() const { return m_frameDeltaTime; }
  
  struct Node {
    std::vector<std::shared_ptr<Node>> children;
    glm::mat4 transform;
    glm::mat4 worldTransform = glm::mat4(1.0f);
    glm::mat4 offsetMatrix;
    std::string name;

    Node() = default;

    void UpdateMatrices(glm::mat4 mtxParent);
  };
  struct Material {
    glm::vec3 diffuse;
    float shininess;
    glm::vec3 ambient;

    ImageObject albedo;
    ImageObject specular;
  };
  struct ModelMeshParameters {
    glm::mat4 mtxWorld;
    glm::vec4 diffuse; // vec3:diffuse, w: specularShininess
    glm::vec4 ambient; // vec3:ambient, w: none
  };
  struct DrawBatch {
    uint32_t vertexOffsetCount;
    uint32_t indexCount;
    uint32_t indexOffsetCount;
    uint32_t materialIndex;

    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<aiBone*> boneList;
    std::vector<std::shared_ptr<Node>> boneList2;
    std::vector<BufferObject> boneMatrixPalette;
    std::vector<BufferObject> modelMeshParameterUBO;
  };
  struct ModelAsset {
    BufferObject Position, Normal, UV0;
    BufferObject BoneIndices, BoneWeights;
    BufferObject Tangent;
    BufferObject Indices;

    std::vector<DrawBatch> DrawBatches;
    Assimp::Importer* importer;
    const aiScene* scene;
    uint32_t totalVertexCount;
    uint32_t totalIndexCount;

    glm::mat4 invGlobalTransform;
    std::shared_ptr<Node> rootNode;
    std::vector<Material> materials;
    std::shared_ptr<Node> FindNode(const std::string& name);

    std::unordered_map<std::string, BufferObject> extraBuffers;
    
    void Release(VulkanAppBase* base);
    std::string name;
    VkPipelineLayout pipelineLayout;
  };

  ModelAsset LoadModelData(std::filesystem::path fileName, bool useFlipUV = false);
  ImageObject LoadTexture(std::filesystem::path fileName);


 private:
  void CreateInstance();
  void SelectGraphicsQueue();
  void CreateDevice();
  void CreateCommandPool();

  // デバッグレポート有効化.
  void EnableDebugReport();
  void DisableDebugReport();
  PFN_vkCreateDebugReportCallbackEXT	m_vkCreateDebugReportCallbackEXT;
  PFN_vkDebugReportMessageEXT	m_vkDebugReportMessageEXT;
  PFN_vkDestroyDebugReportCallbackEXT m_vkDestroyDebugReportCallbackEXT;
  VkDebugReportCallbackEXT  m_debugReport;

  void CreateDescriptorPool();

  // ImGui
  void PrepareImGui();
  void CleanupImGui();
protected:
  VkDeviceMemory AllocateMemory(VkBuffer image, VkMemoryPropertyFlags memProps);
  VkDeviceMemory AllocateMemory(VkImage image, VkMemoryPropertyFlags memProps);
  // 最小化メッセージループ.
  void MsgLoopMinimizedWindow();

  VkDevice  m_device;
  VkPhysicalDevice m_physicalDevice;
  VkInstance m_vkInstance;

  VkPhysicalDeviceMemoryProperties m_physicalMemProps;
  VkQueue m_deviceQueue;
  uint32_t  m_gfxQueueIndex;
  VkCommandPool m_commandPool;

  VkSemaphore m_renderCompletedSem, m_presentCompletedSem;

  VkDescriptorPool m_descriptorPool;

  bool m_isMinimizedWindow;
  bool m_isFullscreen;
  std::unique_ptr<Swapchain> m_swapchain;
  GLFWwindow* m_window;

  using RenderPassRegistry = VulkanObjectStore<VkRenderPass>;
  using PipelineLayoutManager = VulkanObjectStore<VkPipelineLayout>;
  using DescriptorSetLayoutManager = VulkanObjectStore<VkDescriptorSetLayout>;
  std::unique_ptr<RenderPassRegistry> m_renderPassStore;
  std::unique_ptr<PipelineLayoutManager> m_pipelineLayoutStore;
  std::unique_ptr<DescriptorSetLayoutManager> m_descriptorSetLayoutStore;

  std::unordered_map<std::string, ImageObject> m_textureDatabase;
  double m_frameDeltaTime = 0.0;
};