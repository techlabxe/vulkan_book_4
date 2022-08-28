#include "VulkanAppBase.h"
#include "VulkanBookUtil.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include <vector>
#include <sstream>
#include <stack>

#include <glm/gtc/type_ptr.hpp>

static VkBool32 VKAPI_CALL DebugReportCallback(
  VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objactTypes,
  uint64_t object,
  size_t	location,
  int32_t messageCode,
  const char* pLayerPrefix,
  const char* pMessage,
  void* pUserData)
{
  VkBool32 ret = VK_FALSE;
  if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT ||
    flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
  {
    ret = VK_TRUE;
  }
  std::stringstream ss;
  if (pLayerPrefix)
  {
    ss << "[" << pLayerPrefix << "] ";
  }
  ss << pMessage << std::endl;

  OutputDebugStringA(ss.str().c_str());

  return ret;
}

static std::string ConvertFromUTF8(const char* utf8Str)
{
  DWORD dwRet = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
  std::vector<wchar_t> wstrbuf;
  wstrbuf.resize(dwRet);

  MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstrbuf.data(), int(wstrbuf.size()));

  dwRet = WideCharToMultiByte(932, 0, wstrbuf.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> strbuf;
  strbuf.resize(dwRet);

  WideCharToMultiByte(932, 0, wstrbuf.data(), -1, strbuf.data(), int(strbuf.size()), NULL, NULL);
  return std::string(strbuf.data());
}

static void AddVertexIndex(glm::ivec4& v, int index)
{
  if (v.x == -1) {
    v.x = index;
    return;
  }
  if (v.y == -1) {
    v.y = index;
    return;
  }
  if (v.z == -1) {
    v.z = index;
    return;
  }
  if (v.w == -1) {
    v.w = index;
    return;
  }
}
static void AddVertexWeight(glm::vec4& v, float weight)
{
  if (v.x < 0) {
    v.x = weight;
    return;
  }
  if (v.y < 0) {
    v.y = weight;
    return;
  }
  if (v.z < 0) {
    v.z = weight;
    return;
  }
  if (v.w < 0) {
    v.w = weight;
    return;
  }
}
static glm::mat4 ConvertMatrix(const aiMatrix4x4& mtx)
{
  auto m = glm::make_mat4x4(&mtx.a1);
  return glm::transpose(m);
}

struct KTXHeader {
  char identifier[12];
  char endianness[4] = { 1, 2, 3, 4 };
  uint32_t glType;
  uint32_t glTypeSize;
  uint32_t glFormat;
  uint32_t glInternalFormat;
  uint32_t glBaseInternalFormat;
  uint32_t pixelWidth;
  uint32_t pixelHeight;
  uint32_t pixelDepth;
  uint32_t numberOfArrayElements;
  uint32_t numberOfFaces;
  uint32_t numberOfMipmapLevels;
  uint32_t bytesOfKeyValueData;
};
static std::vector<char> ktx_load(const char* fileName, int* width, int* height) {
  std::ifstream infile(fileName, std::ios::binary);
  std::vector<char> imageData;

  if (!infile) {
    return imageData;
  }
  std::vector<char> fileHeader(sizeof(KTXHeader));
  infile.read(fileHeader.data(), fileHeader.size());

  const unsigned char ktxIdentifier[] = { 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
  const auto* header = reinterpret_cast<const KTXHeader*>(fileHeader.data());
  if (memcmp(ktxIdentifier, header->identifier, sizeof(ktxIdentifier)) != 0) {
    return imageData;
  }

  *width = header->pixelWidth;
  *height = header->pixelHeight;

  // Float 32bit のテクスチャのみサポート.
  if (header->glInternalFormat != 0x8814 /*GL_RGBA32F*/) {
    return imageData;
  }
  uint32_t imageSize = 0;
  infile.read((char*)&imageSize, sizeof(imageSize));
  imageData.resize(imageSize);
  infile.read(imageData.data(), imageSize);
  return imageData;
}

bool VulkanAppBase::OnSizeChanged(uint32_t width, uint32_t height)
{
  m_isMinimizedWindow = (width == 0 || height == 0);
  if (m_isMinimizedWindow)
  {
    return false;
  }
  vkDeviceWaitIdle(m_device);

  auto format = m_swapchain->GetSurfaceFormat().format;
  // スワップチェインを作り直す.
  m_swapchain->Prepare(m_physicalDevice, m_gfxQueueIndex, width, height, format);
  return true;
}

bool VulkanAppBase::OnMouseButtonDown(int button)
{
  return ImGui::GetIO().WantCaptureMouse;
}

bool VulkanAppBase::OnMouseButtonUp(int button)
{
  return ImGui::GetIO().WantCaptureMouse;
}

bool VulkanAppBase::OnMouseMove(int dx, int dy)
{
  return ImGui::GetIO().WantCaptureMouse;
}


uint32_t VulkanAppBase::GetMemoryTypeIndex(uint32_t requestBits, VkMemoryPropertyFlags requestProps) const
{
  uint32_t result = ~0u;
  for (uint32_t i = 0; i < m_physicalMemProps.memoryTypeCount; ++i)
  {
    if (requestBits & 1)
    {
      const auto& types = m_physicalMemProps.memoryTypes[i];
      if ((types.propertyFlags & requestProps) == requestProps)
      {
        result = i;
        break;
      }
    }
    requestBits >>= 1;
  }
  return result;
}

void VulkanAppBase::SwitchFullscreen(GLFWwindow* window)
{
  static int lastWindowPosX, lastWindowPosY;
  static int lastWindowSizeW, lastWindowSizeH;

  auto monitor = glfwGetPrimaryMonitor();
  const auto mode = glfwGetVideoMode(monitor);

#if 1 // 現在のモニターに合わせたサイズへ変更.
  if (!m_isFullscreen)
  {
    // to fullscreen
    glfwGetWindowPos(window, &lastWindowPosX, &lastWindowPosY);
    glfwGetWindowSize(window, &lastWindowSizeW, &lastWindowSizeH);
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  }
  else
  {
    // to windowmode
    glfwSetWindowMonitor(window, nullptr, lastWindowPosX, lastWindowPosY, lastWindowSizeW, lastWindowSizeH, mode->refreshRate);
  }
#else
  // 指定された解像度へモニターを変更.
  if (!m_isFullscreen)
  {
    // to fullscreen
    glfwGetWindowPos(window, &lastWindowPosX, &lastWindowPosY);
    glfwGetWindowSize(window, &lastWindowSizeW, &lastWindowSizeH);
    glfwSetWindowMonitor(window, monitor, 0, 0, lastWindowSizeW, lastWindowSizeH, mode->refreshRate);
  }
  else
  {
    // to windowmode
    glfwSetWindowMonitor(window, nullptr, lastWindowPosX, lastWindowPosY, lastWindowSizeW, lastWindowSizeH, mode->refreshRate);
  }
#endif
  m_isFullscreen = !m_isFullscreen;
}

void VulkanAppBase::Initialize(GLFWwindow* window, VkFormat format, bool isFullscreen)
{
  m_window = window;
  CreateInstance();

  // 物理デバイスの選択.
  uint32_t count;
  vkEnumeratePhysicalDevices(m_vkInstance, &count, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices(count);
  vkEnumeratePhysicalDevices(m_vkInstance, &count, physicalDevices.data());
  // 最初のデバイスを使用する.
  m_physicalDevice = physicalDevices[0];
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_physicalMemProps);

  // グラフィックスのキューインデックス取得.
  SelectGraphicsQueue();

#ifdef _DEBUG
  EnableDebugReport();
#endif
  // 論理デバイスの生成.
  CreateDevice();

  // コマンドプールの生成.
  CreateCommandPool();

  VkSurfaceKHR surface;
  auto result = glfwCreateWindowSurface(m_vkInstance, window, nullptr, &surface);
  ThrowIfFailed(result, "glfwCreateWindowSurface Failed.");

  // スワップチェインの生成.
  m_swapchain = std::make_unique<Swapchain>(m_vkInstance, m_device, surface);

  int width, height;
  glfwGetWindowSize(window, &width, &height);
  m_swapchain->Prepare(
    m_physicalDevice, m_gfxQueueIndex,
    uint32_t(width), uint32_t(height),
    format
  );
  auto imageCount = m_swapchain->GetImageCount();
  auto extent = m_swapchain->GetSurfaceExtent();

  VkSemaphoreCreateInfo semCI{
    VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    nullptr, 0,
  };

  vkCreateSemaphore(m_device, &semCI, nullptr, &m_renderCompletedSem);
  vkCreateSemaphore(m_device, &semCI, nullptr, &m_presentCompletedSem);

  // ディスクリプタプールの生成.
  CreateDescriptorPool();

  m_renderPassStore = std::make_unique<RenderPassRegistry>([&](VkRenderPass renderPass) { vkDestroyRenderPass(m_device, renderPass, nullptr); });
  m_descriptorSetLayoutStore = std::make_unique<DescriptorSetLayoutManager>([&](VkDescriptorSetLayout layout) { vkDestroyDescriptorSetLayout(m_device, layout, nullptr); });
  m_pipelineLayoutStore = std::make_unique<PipelineLayoutManager>([&](VkPipelineLayout layout) { vkDestroyPipelineLayout(m_device, layout, nullptr); });

  Prepare();

  PrepareImGui();
}

void VulkanAppBase::Terminate()
{
  if (m_device != VK_NULL_HANDLE)
  {
    vkDeviceWaitIdle(m_device);
  }
  Cleanup();

  CleanupImGui();

  if (m_swapchain)
  {
    m_swapchain->Cleanup();
  }
#ifdef _DEBUG
  DisableDebugReport();
#endif

  for (auto& t : m_textureDatabase) {
    DestroyImage(t.second);
  }
  m_textureDatabase.clear();

  m_renderPassStore->Cleanup();
  m_descriptorSetLayoutStore->Cleanup();
  m_pipelineLayoutStore->Cleanup();

  vkDestroySemaphore(m_device, m_renderCompletedSem, nullptr);
  vkDestroySemaphore(m_device, m_presentCompletedSem, nullptr);

  vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
  vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  vkDestroyDevice(m_device, nullptr);
  vkDestroyInstance(m_vkInstance, nullptr);
  m_commandPool = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_vkInstance = VK_NULL_HANDLE;
}

VulkanAppBase::BufferObject VulkanAppBase::CreateBuffer(uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
  BufferObject obj;
  VkBufferCreateInfo bufferCI{
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    nullptr, 0,
    size, usage,
    VK_SHARING_MODE_EXCLUSIVE,
    0, nullptr
  };
  auto result = vkCreateBuffer(m_device, &bufferCI, nullptr, &obj.buffer);
  ThrowIfFailed(result, "vkCreateBuffer Failed.");

  // メモリ量の算出.
  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(m_device, obj.buffer, &reqs);
  VkMemoryAllocateInfo info{
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    nullptr,
    reqs.size,
    GetMemoryTypeIndex(reqs.memoryTypeBits, props)
  };
  vkAllocateMemory(m_device, &info, nullptr, &obj.memory);
  vkBindBufferMemory(m_device, obj.buffer, obj.memory, 0);
  obj.size = size;
  return obj;
}

VulkanAppBase::ImageObject VulkanAppBase::CreateTexture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memPropsFlags)
{
  ImageObject obj;
  VkImageCreateInfo imageCI{
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    nullptr, 0,
    VK_IMAGE_TYPE_2D,
    format, { width, height, 1 },
    1, 1, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_TILING_OPTIMAL,
    usage,
    VK_SHARING_MODE_EXCLUSIVE,
    0, nullptr,
    VK_IMAGE_LAYOUT_UNDEFINED
  };
  auto result = vkCreateImage(m_device, &imageCI, nullptr, &obj.image);
  ThrowIfFailed(result, "vkCreateImage Failed.");

  // メモリ量の算出.
  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(m_device, obj.image, &reqs);
  VkMemoryAllocateInfo info{
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    nullptr,
    reqs.size,
    GetMemoryTypeIndex(reqs.memoryTypeBits, memPropsFlags)
  };
  result = vkAllocateMemory(m_device, &info, nullptr, &obj.memory);
  ThrowIfFailed(result, "vkAllocateMemory Failed.");
  vkBindImageMemory(m_device, obj.image, obj.memory, 0);

  VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
  {
    imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  VkImageViewCreateInfo viewCI{
    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    nullptr, 0,
    obj.image,
    VK_IMAGE_VIEW_TYPE_2D,
    imageCI.format,
    book_util::DefaultComponentMapping(),
    { imageAspect, 0, 1, 0, 1}
  };
  result = vkCreateImageView(m_device, &viewCI, nullptr, &obj.view);
  ThrowIfFailed(result, "vkCreateImageView Failed.");
  return obj;
}

void VulkanAppBase::DestroyBuffer(BufferObject bufferObj)
{
  vkDestroyBuffer(m_device, bufferObj.buffer, nullptr);
  vkFreeMemory(m_device, bufferObj.memory, nullptr);
}

void VulkanAppBase::DestroyImage(ImageObject imageObj)
{
  vkDestroyImage(m_device, imageObj.image, nullptr);
  vkFreeMemory(m_device, imageObj.memory, nullptr);
  if (imageObj.view != VK_NULL_HANDLE)
  {
    vkDestroyImageView(m_device, imageObj.view, nullptr);
  }
}

VkFramebuffer VulkanAppBase::CreateFramebuffer(
  VkRenderPass renderPass, uint32_t width, uint32_t height, uint32_t viewCount, VkImageView* views)
{
  VkFramebufferCreateInfo fbCI{
    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    nullptr, 0,
    renderPass,
    viewCount, views,
    width, height,
    1,
  };
  VkFramebuffer framebuffer;
  auto result = vkCreateFramebuffer(m_device, &fbCI, nullptr, &framebuffer);
  ThrowIfFailed(result, "vkCreateFramebuffer Failed.");
  return framebuffer;
}
void VulkanAppBase::DestroyFramebuffers(uint32_t count, VkFramebuffer* framebuffers)
{
  for (uint32_t i = 0; i < count; ++i)
  {
    vkDestroyFramebuffer(m_device, framebuffers[i], nullptr);
  }
}
VkFence VulkanAppBase::CreateFence()
{
  VkFenceCreateInfo fenceCI{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    nullptr,
    VK_FENCE_CREATE_SIGNALED_BIT
  };
  VkFence fence;
  auto result = vkCreateFence(m_device, &fenceCI, nullptr, &fence);
  ThrowIfFailed(result, "vkCreateFence Failed.");
  return fence;
}
void VulkanAppBase::DestroyFence(VkFence fence)
{
  vkDestroyFence(m_device, fence, nullptr);
}

VkDescriptorSet VulkanAppBase::AllocateDescriptorSet(VkDescriptorSetLayout dsLayout)
{
  VkDescriptorSetAllocateInfo descriptorSetAI{
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    nullptr, m_descriptorPool,
    1, &dsLayout
  };

  VkDescriptorSet ds;
  auto result = vkAllocateDescriptorSets(m_device, &descriptorSetAI, &ds);
  ThrowIfFailed(result, "vkAllocateDescriptorSets Failed.");
  return ds;
}
void VulkanAppBase::DeallocateDescriptorSet(VkDescriptorSet descriptorSet)
{
  vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &descriptorSet);
}


VkCommandBuffer VulkanAppBase::CreateCommandBuffer(bool bBegin)
{
  VkCommandBufferAllocateInfo commandAI{
     VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    nullptr, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    1
  };
  VkCommandBufferBeginInfo beginInfo{
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  VkCommandBuffer command;
  vkAllocateCommandBuffers(m_device, &commandAI, &command);
  if (bBegin)
  {
    vkBeginCommandBuffer(command, &beginInfo);
  }
  return command;
}

void VulkanAppBase::FinishCommandBuffer(VkCommandBuffer command)
{
  auto result = vkEndCommandBuffer(command);
  ThrowIfFailed(result, "vkEndCommandBuffer Failed.");
  VkFence fence;
  VkFenceCreateInfo fenceCI{
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    nullptr, 0
  };
  result = vkCreateFence(m_device, &fenceCI, nullptr, &fence);

  VkSubmitInfo submitInfo{
    VK_STRUCTURE_TYPE_SUBMIT_INFO,
    nullptr,
    0, nullptr,
    nullptr,
    1, &command,
    0, nullptr,
  };
  vkQueueSubmit(m_deviceQueue, 1, &submitInfo, fence);
  vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(m_device, fence, nullptr);
}

void VulkanAppBase::DestroyCommandBuffer(VkCommandBuffer command)
{
  vkFreeCommandBuffers(m_device, m_commandPool, 1, &command);
}

VkRect2D VulkanAppBase::GetSwapchainRenderArea() const
{
  return VkRect2D{
    VkOffset2D{0,0},
    m_swapchain->GetSurfaceExtent()
  };
}

std::vector<VulkanAppBase::BufferObject> VulkanAppBase::CreateUniformBuffers(uint32_t bufferSize, uint32_t imageCount)
{
  std::vector<BufferObject> buffers(imageCount);
  for (auto& b : buffers)
  {
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    b = CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, props);
  }
  return buffers;
}

void VulkanAppBase::WriteToHostVisibleMemory(VkDeviceMemory memory, uint64_t size, const void* pData)
{
  void* p;
  vkMapMemory(m_device, memory, 0, VK_WHOLE_SIZE, 0, &p);
  memcpy(p, pData, size);

  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory;
  range.offset = 0;
  range.size = VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(m_device, 1, &range);

  vkUnmapMemory(m_device, memory);
}

void VulkanAppBase::AllocateCommandBufferSecondary(uint32_t count, VkCommandBuffer* pCommands)
{
  VkCommandBufferAllocateInfo commandAI{
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    nullptr, m_commandPool,
    VK_COMMAND_BUFFER_LEVEL_SECONDARY, count
  };
  auto result = vkAllocateCommandBuffers(m_device, &commandAI, pCommands);
  ThrowIfFailed(result, "vkAllocateCommandBuffers Faield.");
}

void VulkanAppBase::FreeCommandBufferSecondary(uint32_t count, VkCommandBuffer* pCommands)
{
  vkFreeCommandBuffers(m_device, m_commandPool, count, pCommands);
}

void VulkanAppBase::TransferStageBufferToImage(
  const BufferObject& srcBuffer, const ImageObject& dstImage, const VkBufferImageCopy* region)
{ 
  VkImageMemoryBarrier imb{
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
    0, VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
    dstImage.image,
    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
  };

  // Staging から転送.
  auto command = CreateCommandBuffer();
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr,
    0, nullptr, 1, &imb);

  vkCmdCopyBufferToImage(
    command, 
    srcBuffer.buffer, dstImage.image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, region);
  imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr,
    0, nullptr,
    1, &imb);
  FinishCommandBuffer(command);
  DestroyCommandBuffer(command);
}


VkRenderPass VulkanAppBase::CreateRenderPass(VkFormat colorFormat, VkFormat depthFormat, VkImageLayout layoutColor)
{
  VkRenderPass renderPass;

  std::vector<VkAttachmentDescription> attachments;

  if (colorFormat == VK_FORMAT_UNDEFINED)
  {
    colorFormat = m_swapchain->GetSurfaceFormat().format;
  }

  VkAttachmentDescription colorTarget, depthTarget;
  colorTarget = VkAttachmentDescription{
    0,  // Flags
    colorFormat,
    VK_SAMPLE_COUNT_1_BIT,
    VK_ATTACHMENT_LOAD_OP_CLEAR,
    VK_ATTACHMENT_STORE_OP_STORE,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    VK_ATTACHMENT_STORE_OP_DONT_CARE,
    VK_IMAGE_LAYOUT_UNDEFINED,
    layoutColor
  };
  depthTarget = VkAttachmentDescription{
    0,
    depthFormat,
    VK_SAMPLE_COUNT_1_BIT,
    VK_ATTACHMENT_LOAD_OP_CLEAR,
    VK_ATTACHMENT_STORE_OP_STORE,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    VK_ATTACHMENT_STORE_OP_DONT_CARE,
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  };

  VkAttachmentReference colorRef{
    0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  };
  VkAttachmentReference depthRef{
    1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  };

  VkSubpassDescription subpassDesc{
    0, // Flags
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    0, nullptr, // InputAttachments
    1, &colorRef, // ColorAttachments
    nullptr,    // ResolveAttachments
    nullptr,    // DepthStencilAttachments
    0, nullptr, // PreserveAttachments
  };

  attachments.push_back(colorTarget);
  if (depthFormat != VK_FORMAT_UNDEFINED)
  {
    attachments.push_back(depthTarget);
    subpassDesc.pDepthStencilAttachment = &depthRef;
  }

  VkRenderPassCreateInfo rpCI{
    VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    nullptr, 0,
    uint32_t(attachments.size()), attachments.data(),
    1, &subpassDesc,
    0, nullptr, // Dependency
  };
  auto result = vkCreateRenderPass(m_device, &rpCI, nullptr, &renderPass);
  ThrowIfFailed(result, "vkCreateRenderPass Failed.");
  return renderPass;
}

void VulkanAppBase::WriteToDeviceLocalMemory(BufferObject dstBuffer, const void* pSrcData, size_t size, VkCommandBuffer command, BufferObject* stagingBufferUsed)
{
  VkBufferUsageFlags srcUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  auto stagingBuffer = CreateBuffer(uint32_t(size), srcUsage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  WriteToHostVisibleMemory(stagingBuffer.memory, size, pSrcData);

  VkCommandBuffer xferCommand = command;
  if (xferCommand == VK_NULL_HANDLE) {
    xferCommand = CreateCommandBuffer();
  }

  VkBufferCopy region{};
  region.size = size;
  vkCmdCopyBuffer(xferCommand, stagingBuffer.buffer, dstBuffer.buffer, 1, &region);
  if (command == VK_NULL_HANDLE) {
    // コマンドを自分で確保した場合には実行と解放.
    FinishCommandBuffer(xferCommand);
    DestroyCommandBuffer(xferCommand);
    DestroyBuffer(stagingBuffer);
  } else {
    (*stagingBufferUsed) = stagingBuffer;
  }
}


void VulkanAppBase::PrepareImGui()
{
  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(m_window, true);

  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = m_vkInstance;
  info.PhysicalDevice = m_physicalDevice;
  info.Device = m_device;
  info.QueueFamily = m_gfxQueueIndex;
  info.Queue = m_deviceQueue;
  info.DescriptorPool = m_descriptorPool;
  info.MinImageCount = m_swapchain->GetImageCount();
  info.ImageCount = m_swapchain->GetImageCount();
  ImGui_ImplVulkan_Init(&info, GetRenderPass("default"));

  // フォントテクスチャを転送する.
  VkCommandBufferAllocateInfo commandAI{
   VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    nullptr, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
  };
  VkCommandBufferBeginInfo beginInfo{
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };

  VkCommandBuffer command;
  vkAllocateCommandBuffers(m_device, &commandAI, &command);
  vkBeginCommandBuffer(command, &beginInfo);
  ImGui_ImplVulkan_CreateFontsTexture(command);
  vkEndCommandBuffer(command);

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
  submitInfo.pCommandBuffers = &command;
  submitInfo.commandBufferCount = 1;
  vkQueueSubmit(m_deviceQueue, 1, &submitInfo, VK_NULL_HANDLE);

  // フォントテクスチャ転送の完了を待つ.
  vkDeviceWaitIdle(m_device);
  vkFreeCommandBuffers(m_device, m_commandPool, 1, &command);
}

void VulkanAppBase::CleanupImGui()
{
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}


VulkanAppBase::ModelAsset VulkanAppBase::LoadModelData(std::filesystem::path fileName, bool useFlipUV)
{
  ModelAsset model;
  model.importer = new Assimp::Importer();
  uint32_t flags = 0;
  flags |= aiProcess_Triangulate | aiProcess_CalcTangentSpace;
  if (fileName.extension() == ".pmx") {
    flags |= aiProcess_FlipUVs;
  }
  if (useFlipUV) {
    flags |= aiProcess_FlipUVs;
  }
  model.scene = model.importer->ReadFile(fileName.string(), flags);
  model.name = fileName.filename().string();
  auto scene = model.scene;
  UINT totalVertexCount = 0, totalIndexCount = 0;
  bool hasBone = false;

  std::stack<std::shared_ptr<Node>> nodes;
  model.rootNode = std::make_shared<Node>();
  nodes.push(model.rootNode);

  std::stack<aiNode*> nodeStack;
  nodeStack.push(scene->mRootNode);
  while (!nodeStack.empty()) {
    auto* node = nodeStack.top();
    nodeStack.pop();

    auto nodeTarget = nodes.top(); nodes.pop();

    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
      nodeStack.push(node->mChildren[i]);
    }
    // 操作用の子ノードを生成.
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
      auto child = std::make_shared<Node>();
      nodeTarget->children.push_back(child);
      nodes.push(child);
    }

    auto name = ConvertFromUTF8(node->mName.C_Str());
    auto meshCount = node->mNumMeshes;
    nodeTarget->name = name;

    if (meshCount > 0) {
      for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        auto meshIndex = node->mMeshes[i];
        const auto* mesh = scene->mMeshes[meshIndex];
        totalVertexCount += mesh->mNumVertices;
        totalIndexCount += mesh->mNumFaces * 3;
        hasBone |= mesh->HasBones();
      }
    }
    nodeTarget->transform = ConvertMatrix(node->mTransformation);
  }

  uint32_t bufferSize = sizeof(glm::vec3) * totalVertexCount;
  VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  model.Position = CreateBuffer(bufferSize, usage, props);
  model.Normal = CreateBuffer(bufferSize, usage, props);
  model.Tangent = CreateBuffer(bufferSize, usage, props);

  bufferSize = sizeof(glm::vec2) * totalVertexCount;
  model.UV0 = CreateBuffer(bufferSize, usage, props);
  if (hasBone) {
    bufferSize = sizeof(glm::ivec4) * totalVertexCount;
    model.BoneIndices = CreateBuffer(bufferSize, usage, props);

    bufferSize = sizeof(glm::vec4) * totalVertexCount;
    model.BoneWeights = CreateBuffer(bufferSize, usage, props);
  }

  bufferSize = sizeof(uint32_t) * totalIndexCount;
  usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  model.Indices = CreateBuffer(bufferSize, usage, props);

  // 頂点データの構築.
  std::vector<glm::vec3> vbPos, vbNrm, vbTan;
  std::vector<glm::vec2> vbUV0;
  std::vector<glm::ivec4> vbBIndices;
  std::vector<glm::vec4> vbBWeights;
  std::vector<uint32_t> ibIndices;
  vbPos.reserve(totalVertexCount);
  vbNrm.reserve(totalVertexCount);
  vbUV0.reserve(totalVertexCount);
  vbTan.reserve(totalVertexCount);
  ibIndices.reserve(totalIndexCount);
  if (hasBone) {
    vbBIndices.resize(totalVertexCount, glm::ivec4(-1, -1, -1, -1));
    vbBWeights.resize(totalVertexCount, glm::vec4(-1.0f, -1.0f, -1.0f, -1.0f));
  }

  totalVertexCount = 0;
  totalIndexCount = 0;
  nodeStack.push(scene->mRootNode);
  while (!nodeStack.empty()) {
    auto* node = nodeStack.top();
    nodeStack.pop();

    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
      nodeStack.push(node->mChildren[i]);
    }

    auto name = ConvertFromUTF8(node->mName.C_Str());
    auto meshCount = node->mNumMeshes;

    if (meshCount > 0) {
      for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        auto meshIndex = node->mMeshes[i];
        const auto* mesh = scene->mMeshes[meshIndex];

        DrawBatch batch{};
        batch.vertexOffsetCount = totalVertexCount;
        batch.indexOffsetCount = totalIndexCount;
        batch.indexCount = mesh->mNumFaces * 3;
        batch.materialIndex = mesh->mMaterialIndex;

        const auto* vPosStart = reinterpret_cast<const glm::vec3*>(mesh->mVertices);
        vbPos.insert(vbPos.end(), vPosStart, vPosStart + mesh->mNumVertices);

        const auto* vNrmStart = reinterpret_cast<const glm::vec3*>(mesh->mNormals);
        vbNrm.insert(vbNrm.end(), vNrmStart, vNrmStart + mesh->mNumVertices);

        std::vector<glm::vec2> uvWork(mesh->mNumVertices);
        for (int j = 0; j < int(mesh->mNumVertices); ++j) {
          const auto& src = mesh->mTextureCoords[0][j];
          uvWork[j].x = src.x;
          uvWork[j].y = src.y;
        }

        vbUV0.insert(vbUV0.end(), uvWork.begin(), uvWork.end());

        if (mesh->HasTangentsAndBitangents()) {
          const auto* vTanStart = reinterpret_cast<const glm::vec3*>(mesh->mTangents);
          vbTan.insert(vbTan.end(), vTanStart, vTanStart + mesh->mNumVertices);
        }

        for (int f = 0; f < int(mesh->mNumFaces); ++f) {
          for (int fi = 0; fi < int(mesh->mFaces[f].mNumIndices); ++fi) {
            auto vertexIndex = mesh->mFaces[f].mIndices[fi];
            ibIndices.push_back(vertexIndex);
          }
        }

        uint32_t imageCount = m_swapchain->GetImageCount();
        bufferSize = uint32_t(sizeof(ModelMeshParameters));
        batch.modelMeshParameterUBO = CreateUniformBuffers(bufferSize, imageCount);

        if (hasBone) {
          if (mesh->HasBones()) {
            // 有効なボーン情報を持つものを抽出.
            std::vector<std::string> boneNameList;
            std::vector<aiBone*> activeBones;
            std::vector<int> boneIndexList;
            for (uint32_t j = 0; j < mesh->mNumBones; ++j) {
              const auto bone = mesh->mBones[j];
              auto name = ConvertFromUTF8(bone->mName.C_Str());
              if (bone->mNumWeights > 0) {
                boneIndexList.push_back(int(boneNameList.size()));
                boneNameList.push_back(name);
                activeBones.push_back(bone);
              }
            }

            auto vertexBase = batch.vertexOffsetCount;
            for (int boneIndex = 0; boneIndex < int(activeBones.size()); ++boneIndex) {
              auto bone = activeBones[boneIndex];
              for (int j = 0; j < int(bone->mNumWeights); ++j) {
                auto weightInfo = bone->mWeights[j];
                auto vertexIndex = vertexBase + weightInfo.mVertexId;
                auto weight = weightInfo.mWeight;

                AddVertexIndex(vbBIndices[vertexIndex], boneIndex);
                AddVertexWeight(vbBWeights[vertexIndex], weight);
              }
            }

            batch.boneList = activeBones;

            for (int boneIndex = 0; boneIndex < int(activeBones.size()); ++boneIndex) {
              auto bone = activeBones[boneIndex];
              auto name = ConvertFromUTF8(bone->mName.C_Str());

              auto node = model.FindNode(name);
              assert(node != nullptr);
              node->offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);
              batch.boneList2.push_back(node);
            }

            uint32_t bufferSize = uint32_t(sizeof(glm::mat4) * activeBones.size());
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            batch.boneMatrixPalette = CreateUniformBuffers(bufferSize, imageCount);
          }
        }
        totalVertexCount += mesh->mNumVertices;
        totalIndexCount += mesh->mNumFaces * 3;

        model.DrawBatches.emplace_back(batch);
        hasBone |= mesh->HasBones();
      }
    }
  }

  for (int i = 0; i<int(model.scene->mNumMaterials); ++i) {
    Material m{};
    auto material = model.scene->mMaterials[i];

    std::filesystem::path baseDir(fileName);
    baseDir = baseDir.parent_path();
    aiString path;
    auto ret = material->GetTexture(aiTextureType_DIFFUSE, 0, &path);
    if (ret == aiReturn_SUCCESS) {
      auto texfileName = ConvertFromUTF8(path.C_Str());
      auto textureFilePath = (baseDir / texfileName);

      auto tex = LoadTexture(textureFilePath);
      m.albedo = tex;
    } else {
      auto tex = LoadTexture("assets/texture/white.png");
      m.albedo = tex;
    }

    ret = material->GetTexture(aiTextureType_SPECULAR, 0, &path);
    if (ret == aiReturn_SUCCESS) {
      auto texfileName = ConvertFromUTF8(path.C_Str());
      auto textureFilePath = (baseDir / texfileName).string();

      m.specular = LoadTexture(textureFilePath);
    } else {
      m.specular = LoadTexture("assets/texture/black.png");
    }

    float shininess = 0;
    ret = material->Get(AI_MATKEY_SHININESS, shininess);
    m.shininess = shininess;

    aiColor3D diffuse{};
    ret = material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
    m.diffuse = glm::vec3(diffuse.r, diffuse.g, diffuse.b);

    aiColor3D ambient{};
    ret = material->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
    m.ambient = glm::vec3(ambient.r, ambient.g, ambient.b);

    model.materials.push_back(m);
  }
  model.totalVertexCount = totalVertexCount;
  model.totalIndexCount = totalIndexCount;

  std::vector<BufferObject> stagingBufferList;
  auto command = CreateCommandBuffer();
  BufferObject staging;
  WriteToDeviceLocalMemory(model.Position, vbPos.data(), sizeof(glm::vec3)* vbPos.size(), command, &staging); stagingBufferList.push_back(staging);
  WriteToDeviceLocalMemory(model.Normal, vbNrm.data(), sizeof(glm::vec3)*vbNrm.size(), command, &staging); stagingBufferList.push_back(staging);
  WriteToDeviceLocalMemory(model.UV0, vbUV0.data(), sizeof(glm::vec2)*vbUV0.size(), command, &staging); stagingBufferList.push_back(staging);
  WriteToDeviceLocalMemory(model.Indices, ibIndices.data(), sizeof(uint32_t)*ibIndices.size(), command, &staging); stagingBufferList.push_back(staging);
  WriteToDeviceLocalMemory(model.Tangent, vbTan.data(), sizeof(glm::vec3)* vbTan.size(), command, &staging); stagingBufferList.push_back(staging);

  // ボーン情報未設定領域を掃除.
  if (hasBone) {
    for (auto& v : vbBIndices) {
      if (v.x < 0) { v.x = 0; }
      if (v.y < 0) { v.y = 0; }
      if (v.z < 0) { v.z = 0; }
      if (v.w < 0) { v.w = 0; }
    }
    for (auto& v : vbBWeights) {
      if (v.x < 0.0f) { v.x = 0.0f; }
      if (v.y < 0.0f) { v.y = 0.0f; }
      if (v.z < 0.0f) { v.z = 0.0f; }
      if (v.w < 0.0f) { v.w = 0.0f; }

      float total = v.x + v.y + v.z + v.w;
      assert(std::abs(total) > 0.999f && std::abs(total) < 1.01f);
    }

    WriteToDeviceLocalMemory(model.BoneIndices, vbBIndices.data(), sizeof(glm::uvec4)*vbBIndices.size(), command, &staging); stagingBufferList.push_back(staging);
    WriteToDeviceLocalMemory(model.BoneWeights, vbBWeights.data(), sizeof(glm::vec4)*vbBWeights.size(), command, &staging); stagingBufferList.push_back(staging);
  }
  FinishCommandBuffer(command);
  DestroyCommandBuffer(command);
  for (auto& buffer : stagingBufferList) {
    DestroyBuffer(buffer);
  }
  stagingBufferList.clear();

  auto mtx = ConvertMatrix(scene->mRootNode->mTransformation);
  model.invGlobalTransform = glm::inverse(mtx);

  model.rootNode->UpdateMatrices(glm::mat4(1.0f));
  return model;
}

VulkanAppBase::ImageObject VulkanAppBase::LoadTexture(std::filesystem::path fileName)
{
  auto it = m_textureDatabase.find(fileName.string());
  if (it != m_textureDatabase.end()) {
    return it->second;
  }
  ImageObject texture{};
  BufferObject stagingBuffer;

  int width = 0, height = 0;
  stbi_uc* imageData = nullptr;
  std::vector<char> imageDataKtx;
  VkFormat format;

  auto ext = fileName.extension().string();
  if (ext == ".tga" || ext == ".png" || ext == ".jpg") {
    imageData = stbi_load(fileName.string().c_str(), &width, &height, nullptr, 4);
    if (imageData == nullptr) {
      DebugBreak();//Texture Not found
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    format = VK_FORMAT_R8G8B8A8_UNORM;
    texture = CreateTexture(uint32_t(width), uint32_t(height), format, usage);

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkBufferUsageFlags usageBuffer = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto rowSize = uint32_t(width * sizeof(uint32_t));
    auto bufferSize = height * rowSize;
    stagingBuffer = CreateBuffer(bufferSize, usageBuffer, memProps);
    WriteToHostVisibleMemory(stagingBuffer.memory, bufferSize, imageData);
  }
  if (ext == ".ktx") {
    imageDataKtx = ktx_load(fileName.string().c_str(), &width, &height);
    // KTX では Float テクスチャのみ暫定対応.
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    format = VK_FORMAT_R32G32B32A32_SFLOAT;
    texture = CreateTexture(uint32_t(width), uint32_t(height), format, usage);

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkBufferUsageFlags usageBuffer = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto rowSize = uint32_t(width * sizeof(float)*4);
    auto bufferSize = height * rowSize;
    stagingBuffer = CreateBuffer(bufferSize, usageBuffer, memProps);
    WriteToHostVisibleMemory(stagingBuffer.memory, bufferSize, imageDataKtx.data());
  }

  VkBufferImageCopy region{};
  region.imageExtent = { uint32_t(width), uint32_t(height), 1 };
  region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
  TransferStageBufferToImage(stagingBuffer, texture, &region);

  if (imageData) {
    stbi_image_free(imageData);
  }
  DestroyBuffer(stagingBuffer);
  texture.width = width;
  texture.height = height;
  texture.format = format;

  m_textureDatabase[fileName.string()] = texture;
  return texture;
}

void VulkanAppBase::CreateInstance()
{
  VkApplicationInfo appinfo{};
  appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appinfo.pApplicationName = "VulkanBook4";
  appinfo.pEngineName = "VulkanBook4";
  appinfo.apiVersion = VK_API_VERSION_1_1;
  appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

  // 拡張情報の取得.
  uint32_t count;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> props(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());
  std::vector<const char*> extensions;
  extensions.reserve(count);
  for (const auto& v : props)
  {
    extensions.push_back(v.extensionName);
  }

  VkInstanceCreateInfo instanceCI{};
  instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCI.enabledExtensionCount = count;
  instanceCI.ppEnabledExtensionNames = extensions.data();
  instanceCI.pApplicationInfo = &appinfo;
#ifdef _DEBUG
  // デバッグビルド時には検証レイヤーを有効化
  const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
  if (VK_HEADER_VERSION_COMPLETE < VK_MAKE_VERSION(1, 1, 106)) {
	  // "VK_LAYER_LUNARG_standard_validation" は廃止になっているが昔の Vulkan SDK では動くので対処しておく.
	  layers[0] = "VK_LAYER_LUNARG_standard_validation";
  }
  instanceCI.enabledLayerCount = 1;
  instanceCI.ppEnabledLayerNames = layers;
#endif
  auto result = vkCreateInstance(&instanceCI, nullptr, &m_vkInstance);
  ThrowIfFailed(result, "vkCreateInstance Failed.");
}

void VulkanAppBase::SelectGraphicsQueue()
{
  // グラフィックスキューのインデックス値を取得.
  uint32_t queuePropCount;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queuePropCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilyProps(queuePropCount);
  vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queuePropCount, queueFamilyProps.data());
  uint32_t graphicsQueue = ~0u;
  for (uint32_t i = 0; i < queuePropCount; ++i)
  {
    if (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      graphicsQueue = i; break;
    }
  }
  m_gfxQueueIndex = graphicsQueue;
}

void VulkanAppBase::CreateDevice()
{
  const float defaultQueuePriority(1.0f);
  VkDeviceQueueCreateInfo devQueueCI{
    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    nullptr, 0,
    m_gfxQueueIndex,
    1, &defaultQueuePriority
  };
  uint32_t count;
  vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> deviceExtensions(count);
  vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &count, deviceExtensions.data());

  std::vector<const char*> extensions;
  extensions.reserve(count);
  for (const auto& v : deviceExtensions)
  {
    if (std::string(v.extensionName) == VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) {
      // VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME と VK_KHR_BUFFER_DEVICE_ADDRESS は同時に有効化すると検証レイヤーがエラーを報告する
      continue;
    }
    extensions.push_back(v.extensionName);
  }

  VkPhysicalDeviceFeatures features{};
  vkGetPhysicalDeviceFeatures(m_physicalDevice, &features);

  count = uint32_t(extensions.size());
  VkDeviceCreateInfo deviceCI{
    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    nullptr, 0,
    1, &devQueueCI,
    0, nullptr,
    count, extensions.data(),
    &features
  };

  VkPhysicalDeviceTransformFeedbackFeaturesEXT xfbEnable{};
  xfbEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT;
  xfbEnable.geometryStreams = true;
  xfbEnable.transformFeedback = true;
  deviceCI.pNext = &xfbEnable;

  VkPhysicalDeviceRobustness2FeaturesEXT physicalDevRobustnes2Features{};
  physicalDevRobustnes2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  physicalDevRobustnes2Features.nullDescriptor = true;
  physicalDevRobustnes2Features.robustImageAccess2 = true;
  xfbEnable.pNext = &physicalDevRobustnes2Features;

  auto result = vkCreateDevice(m_physicalDevice, &deviceCI, nullptr, &m_device);
  ThrowIfFailed(result, "vkCreateDevice Failed.");

  vkGetDeviceQueue(m_device, m_gfxQueueIndex, 0, &m_deviceQueue);
}

void VulkanAppBase::CreateCommandPool()
{
  VkCommandPoolCreateInfo cmdPoolCI{
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    nullptr,
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    m_gfxQueueIndex
  };
  auto result = vkCreateCommandPool(m_device, &cmdPoolCI, nullptr, &m_commandPool);
  ThrowIfFailed(result, "vkCreateCommandPool Failed.");
}

void VulkanAppBase::CreateDescriptorPool()
{
  VkResult result;
  VkDescriptorPoolSize poolSize[] = {
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100},
  };
  VkDescriptorPoolCreateInfo descPoolCI{
    VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    nullptr,  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    1000 * _countof(poolSize), // maxSets
    _countof(poolSize), poolSize,
  };
  result = vkCreateDescriptorPool(m_device, &descPoolCI, nullptr, &m_descriptorPool);
  ThrowIfFailed(result, "vkCreateDescriptorPool Failed.");
}

VkDeviceMemory VulkanAppBase::AllocateMemory(VkBuffer buffer, VkMemoryPropertyFlags memProps)
{
  VkDeviceMemory memory;
  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(m_device, buffer, &reqs);
  VkMemoryAllocateInfo info{
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    nullptr,
    reqs.size,
    GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  auto result = vkAllocateMemory(m_device, &info, nullptr, &memory);
  ThrowIfFailed(result, "vkAllocateMemory Failed.");
  return memory;
}

VkDeviceMemory VulkanAppBase::AllocateMemory(VkImage image, VkMemoryPropertyFlags memProps)
{
  VkDeviceMemory memory;
  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(m_device, image, &reqs);
  VkMemoryAllocateInfo info{
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    nullptr,
    reqs.size,
    GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
  };
  auto result = vkAllocateMemory(m_device, &info, nullptr, &memory);
  ThrowIfFailed(result, "vkAllocateMemory Failed.");
  return memory;
}


#define GetInstanceProcAddr(FuncName) \
  m_##FuncName = reinterpret_cast<PFN_##FuncName>(vkGetInstanceProcAddr(m_vkInstance, #FuncName))

void VulkanAppBase::EnableDebugReport()
{
  GetInstanceProcAddr(vkCreateDebugReportCallbackEXT);
  GetInstanceProcAddr(vkDebugReportMessageEXT);
  GetInstanceProcAddr(vkDestroyDebugReportCallbackEXT);

  VkDebugReportFlagsEXT flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

  VkDebugReportCallbackCreateInfoEXT drcCI{};
  drcCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  drcCI.flags = flags;
  drcCI.pfnCallback = &DebugReportCallback;
  auto result = m_vkCreateDebugReportCallbackEXT(m_vkInstance, &drcCI, nullptr, &m_debugReport);
  ThrowIfFailed(result, "vkCreateDebugReportCallback Failed.");
}

void VulkanAppBase::DisableDebugReport()
{
  if (m_vkDestroyDebugReportCallbackEXT)
  {
    m_vkDestroyDebugReportCallbackEXT(m_vkInstance, m_debugReport, nullptr);
  }
}

void VulkanAppBase::MsgLoopMinimizedWindow()
{
  int width, height;
  do
  {
    glfwGetWindowSize(m_window, &width, &height);
    glfwWaitEvents();
  } while (width == 0 || height == 0);
}

std::shared_ptr<VulkanAppBase::Node> VulkanAppBase::ModelAsset::FindNode(const std::string& name)
{
  std::stack<std::shared_ptr<Node>> nodes;
  nodes.push(rootNode);
  while (!nodes.empty()) {
    auto node = nodes.top();
    nodes.pop();

    if (node->name == name) {
      return node;
    }

    for (auto& child : node->children) {
      nodes.push(child);
    }
  }
  return nullptr;
}

void VulkanAppBase::ModelAsset::Release(VulkanAppBase* base)
{
  delete importer;
  scene = nullptr;
  importer = nullptr;

  base->DestroyBuffer(this->Position);
  base->DestroyBuffer(this->Normal);
  base->DestroyBuffer(this->UV0);
  base->DestroyBuffer(this->Indices);
  base->DestroyBuffer(this->BoneIndices);
  base->DestroyBuffer(this->BoneWeights);
  base->DestroyBuffer(this->Tangent);

  for (auto& b : extraBuffers) {
    base->DestroyBuffer(b.second);
  }
  extraBuffers.clear();

  for (auto& batch : this->DrawBatches) {
    for (auto& mtxPalette : batch.boneMatrixPalette) {
      base->DestroyBuffer(mtxPalette);
    }
    batch.boneMatrixPalette.clear();
    for (auto& modelUBO : batch.modelMeshParameterUBO) {
      base->DestroyBuffer(modelUBO);
    }
    batch.modelMeshParameterUBO.clear();
  }
}

void VulkanAppBase::Node::UpdateMatrices(glm::mat4 mtxParent)
{
  worldTransform = mtxParent * transform;
  for (auto& c : children) {
    c->UpdateMatrices(worldTransform);
  }
}
