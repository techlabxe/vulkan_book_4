#include "DeferredRenderApp.h"
#include "TeapotModel.h"
#include "VulkanBookUtil.h"

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <array>
#include <random>

using namespace std;
using namespace glm;

DeferredRenderApp::DeferredRenderApp()
{
  m_camera.SetLookAt(
    vec3(918.0f, 280.0f, -50.0f),
    vec3(30.0f, 25.0f, -100.0f) 
  );
  m_sceneParameters.lightDir = glm::vec4(1, 0.5, 0.0f, 0);

  m_sceneParameters.drawFlag = 0;
  m_sceneParameters.animationFrame = 0;

  std::mt19937 mt(uint32_t(12356));
  std::uniform_real_distribution randRegionXZ(-650.0f, +650.0f);
  std::uniform_real_distribution randRegionY(0.0f, +300.0f);
  std::uniform_real_distribution randRadius(50.0f, 250.0f);
  for (int i = 0; i < _countof(m_sceneParameters.pointLights); ++i) {
    vec4 p{};
    p.x = randRegionXZ(mt);
    p.y = randRegionY(mt);
    p.z = randRegionXZ(mt);
    p.w = randRadius(mt);
    m_sceneParameters.pointLights[i] = p;
  }

  m_sceneParameters.pointLightColors[0] = vec4(1.0f, 0.1f, 0.1f, 0.0f);
  m_sceneParameters.pointLightColors[1] = vec4(0.1f, 1.0f, 0.1f, 0.0f);
  m_sceneParameters.pointLightColors[2] = vec4(0.1f, 0.1f, 1.0f, 0.0f);
  m_sceneParameters.pointLightColors[3] = vec4(0.8f, 0.8f, 0.8f, 0.0f);

  m_sceneParameters.pointLightColors[4] = vec4(1.0f, 1.0f, 0.1f, 0.0f);
  m_sceneParameters.pointLightColors[5] = vec4(0.1f, 1.0f, 1.0f, 0.0f);
  m_sceneParameters.pointLightColors[6] = vec4(1.0f, 1.0f, 0.0f, 0.0f);
  m_sceneParameters.pointLightColors[7] = vec4(0.5f, 0.8f, 0.2f, 0.0f);

  m_sceneParameters.pointLights[0].x = 0.0f;
  m_sceneParameters.pointLights[0].y = 200.0f;
  m_sceneParameters.pointLights[0].z = 1000.0f;
  m_sceneParameters.pointLights[0].w = 500.f;
}


void DeferredRenderApp::Prepare()
{
  CreateSampleLayouts();

  auto extent = m_swapchain->GetSurfaceExtent();
  auto width = extent.width;
  auto height = extent.height;
  {
    // VkRenderPass を作る 
    // 0: Depth Prepass
    // 1: Draw GBuffer
    // 2: Lighting Pass


    VkAttachmentDescription attachments[] =
    {
      // Backbuffer
      { 
        0,
        m_swapchain->GetSurfaceFormat().format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  
        //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },
      // Depth buffer
      {
        0,
        VK_FORMAT_D32_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      },
      // G buffer (Position)
      {
        0,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
      // G buffer (Normal)
      {
        0,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },
      // G buffer (Albedo)
      {
        0,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      },

    };

    // DepthPrepass パス用.
    VkAttachmentReference depthRef{};
    depthRef.attachment = AttachmentDepth;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // GBuffer描画パス.
    VkAttachmentReference gbufferOutput[] = {
      {
        AttachmentGbufferPosition, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      },
      {
        AttachmentGbufferNormal, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      },
      {
        AttachmentGbufferAlbedo, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      },
    };
 
    // ライティングパスで前段のGBufferにアクセス.
    VkAttachmentReference gbufferInput[] = {
      {
        AttachmentGbufferPosition, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      },
      {
        AttachmentGbufferNormal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      },
      {
        AttachmentGbufferAlbedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      },
    };

    // 最終パスでバックバッファに向けて出力する用.
    VkAttachmentReference backbufferRef = {
      AttachmentBackbuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // サブパスの定義.
    VkSubpassDescription subpasses[] = {
      // Subpass 0: depth prepass
      {
        0, // Flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0, nullptr, // InputAttachments
        0, nullptr, // ColorAttachments
        nullptr,    // ResolveAttachments
        &depthRef,    // DepthStencilAttachments
        0, nullptr, // PreserveAttachments
      },
      // Subpass 1: draw gbuffer
      {
        0, // Flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0, nullptr, // InputAttachments
        _countof(gbufferOutput), gbufferOutput, // ColorAttachments
        nullptr,    // ResolveAttachments
        &depthRef,    // DepthStencilAttachments
        0, nullptr, // PreserveAttachments
      },
      // Subpass 2: lighting
      {
        0, // Flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        _countof(gbufferInput), gbufferInput, // InputAttachments
        1, &backbufferRef, // ColorAttachments
        nullptr,    // ResolveAttachments
        nullptr,    // DepthStencilAttachments
        0, nullptr, // PreserveAttachments
      },
    };

    VkSubpassDependency dependencies[] = {
      {
        VK_SUBPASS_EXTERNAL,
        SubpassDepthPrepass,  // dstSubpass
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
        VK_ACCESS_MEMORY_READ_BIT, // srcAccessMask
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT
      },
      {
        SubpassDepthPrepass,
        SubpassGbuffer,       // dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,            // dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT
      },
      {
        SubpassGbuffer,
        SubpassLighting,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, // srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,      // dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT
      },
      {
        SubpassLighting,
        VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
        VK_ACCESS_MEMORY_READ_BIT,      // dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT
      }
    };

    
    VkRenderPassCreateInfo rpCI{};
    rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = _countof(attachments);
    rpCI.pAttachments = attachments;
    rpCI.subpassCount = _countof(subpasses);
    rpCI.pSubpasses = subpasses;
    rpCI.dependencyCount = _countof(dependencies);
    rpCI.pDependencies = dependencies;
    VkRenderPass renderPass{};
    vkCreateRenderPass(m_device, &rpCI, nullptr, &renderPass);

    RegisterRenderPass("deferred", renderPass);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | /*VK_IMAGE_USAGE_SAMPLED_BIT |*/ VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    m_rtPosition = CreateTexture(width, height, VK_FORMAT_R32G32B32A32_SFLOAT, usage);
    m_rtNormal = CreateTexture(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, usage);
    m_rtAlbedo = CreateTexture(width, height, VK_FORMAT_R8G8B8A8_UNORM, usage);

  }
  {
    // UI を合成するレンダーパスではカラー/デプスバッファをクリアしない.
    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentDescription colorTarget, depthTarget;
    colorTarget = VkAttachmentDescription{
      0,  // Flags
      m_swapchain->GetSurfaceFormat().format,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_LOAD,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    depthTarget = VkAttachmentDescription{
      0,
      VK_FORMAT_D32_SFLOAT,
      VK_SAMPLE_COUNT_1_BIT,
      VK_ATTACHMENT_LOAD_OP_LOAD,
      VK_ATTACHMENT_STORE_OP_STORE,
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
      &depthRef,    // DepthStencilAttachments
      0, nullptr, // PreserveAttachments
    };
    attachments.push_back(colorTarget);
    attachments.push_back(depthTarget);


    VkRenderPassCreateInfo rpCI{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      nullptr, 0,
      uint32_t(attachments.size()), attachments.data(),
      1, &subpassDesc,
      0, nullptr, // Dependency
    };
    VkRenderPass renderPass{};
    auto result = vkCreateRenderPass(m_device, &rpCI, nullptr, &renderPass);
    ThrowIfFailed(result, "vkCreateRenderPass Failed.");

    RegisterRenderPass("default", renderPass);
  }
  
  // デプスバッファを準備する.
  m_depthBuffer = CreateTexture(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  // フレームバッファの準備.
  PrepareFramebuffers();
  auto imageCount = m_swapchain->GetImageCount();
  m_fbGbuffers.resize(imageCount);
  for (int i = 0; i < imageCount; ++i) {
    std::vector<VkImageView> fbImageViews{
      m_swapchain->GetImageView(i),
      m_depthBuffer.view,
      m_rtPosition.view, m_rtNormal.view, m_rtAlbedo.view,
    };

    VkFramebufferCreateInfo fbCI{};
    fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCI.renderPass = GetRenderPass("deferred");
    fbCI.attachmentCount = uint32_t(fbImageViews.size());
    fbCI.pAttachments = fbImageViews.data();
    fbCI.width = width;
    fbCI.height = height;
    fbCI.layers = 1;
    VkFramebuffer framebuffer{};
    vkCreateFramebuffer(m_device, &fbCI, nullptr, &framebuffer);
    m_fbGbuffers[i] = framebuffer;
  }


  // コマンドバッファの準備.
  m_commandBuffers.resize(imageCount);
  for (auto& c : m_commandBuffers)
  {
    c.fence = CreateFence();
    c.commandBuffer = CreateCommandBuffer(false); // コマンドバッファ開始状態にしない.
  }

  // 定数バッファの準備.
  auto bufferSize = uint32_t(sizeof(ShaderParameters));
  m_uniformBuffers = CreateUniformBuffers(bufferSize, imageCount);


  VkSamplerCreateInfo samplerCI{
    VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    nullptr, 0, VK_FILTER_LINEAR,VK_FILTER_LINEAR,
    VK_SAMPLER_MIPMAP_MODE_LINEAR,
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_REPEAT, 
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    0.0f, VK_FALSE, 1.0f, VK_FALSE,
    VK_COMPARE_OP_NEVER, 0.0f, 0.0f,
    VK_BORDER_COLOR_INT_OPAQUE_WHITE, VK_FALSE,
  };
  vkCreateSampler(m_device, &samplerCI, nullptr, &m_sampler);

  m_model = LoadModelData("assets/model/sponza/sponza.obj", true);

  PrepareModelResource(m_model);

  m_camera.SetPerspective(
    radians(45.0f), float(extent.width) / float(extent.height), 1.0f, 5000.0f
  );

  CreatePipeline();

}

void DeferredRenderApp::Cleanup()
{
  m_model.Release(this);
  DestroyImage(m_rtPosition);
  DestroyImage(m_rtNormal);
  DestroyImage(m_rtAlbedo);
  DestroyFramebuffers(uint32_t(m_fbGbuffers.size()), m_fbGbuffers.data());

  vkFreeDescriptorSets(m_device, m_descriptorPool, uint32_t(m_dsGbuffer.size()), m_dsGbuffer.data());
  vkFreeDescriptorSets(m_device, m_descriptorPool, uint32_t(m_dsDeferredLighting.size()), m_dsDeferredLighting.data());

  for (auto& ubo : m_uniformBuffers)
  {
    DestroyBuffer(ubo);
  }

  vkDestroySampler(m_device, m_sampler, nullptr);

  for (auto& v : m_pipelines)
  {
    vkDestroyPipeline(m_device, v.second, nullptr);
  }
  m_pipelines.clear();

  DestroyImage(m_depthBuffer);
  auto count = uint32_t(m_framebuffers.size());
  DestroyFramebuffers(count, m_framebuffers.data());

  for (auto& c : m_commandBuffers)
  {
    DestroyCommandBuffer(c.commandBuffer);
    DestroyFence(c.fence);
  }
  m_commandBuffers.clear();
}

bool DeferredRenderApp::OnMouseButtonDown(int msg)
{
  if (VulkanAppBase::OnMouseButtonDown(msg))
    return true;

  m_camera.OnMouseButtonDown(msg);
  return true;
}

bool DeferredRenderApp::OnMouseMove(int dx, int dy)
{
  if (VulkanAppBase::OnMouseMove(dx, dy))
    return true;

  if (!m_swapchain) {
    return false;
  }

  auto extent = m_swapchain->GetSurfaceExtent();
  float fdx = float(-dx) / extent.width;
  float fdy = float(dy) / extent.height;
  m_camera.OnMouseMove(fdx, fdy);
  return true;
}

bool DeferredRenderApp::OnMouseButtonUp(int msg)
{
  if (VulkanAppBase::OnMouseButtonUp(msg))
    return true;

  m_camera.OnMouseButtonUp();
  return true;
}

void DeferredRenderApp::Render()
{
  if (m_isMinimizedWindow)
  {
    MsgLoopMinimizedWindow();
  }
  uint32_t imageIndex = 0;
  auto result = m_swapchain->AcquireNextImage(&imageIndex, m_presentCompletedSem);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    return;
  }

  auto fence = m_commandBuffers[imageIndex].fence;
  vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

  VkCommandBufferBeginInfo commandBI{
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    nullptr, 0, nullptr
  };

  {
    // ユニフォームバッファの更新.
    m_sceneParameters.view = m_camera.GetViewMatrix();
    auto extent = m_swapchain->GetSurfaceExtent();
    m_sceneParameters.proj = m_camera.GetProjectionMatrix();
    m_sceneParameters.cameraPosition = vec4(m_camera.GetPosition(), 0);
    m_sceneParameters.frameDeltaTime = float(GetFrameDeltaTime());
    m_sceneParameters.frameCountLow = uint32_t(m_frameCount & 0xFFFFFFFFu);
    auto ubo = m_uniformBuffers[imageIndex];
    WriteToHostVisibleMemory(ubo.memory, sizeof(ShaderParameters), &m_sceneParameters);
  }


  auto command = m_commandBuffers[imageIndex].commandBuffer;

  VkClearValue clearVals[5];
  clearVals[0].color = { 0,0,0,0 };
  clearVals[1].depthStencil = { 1.0f, 0 };
  clearVals[2].color = { 0,0,0,0 };
  clearVals[3].color = { 0,0,0,0 };
  clearVals[4].color = { 0,0,0,0 };

  auto renderArea = VkRect2D{
    VkOffset2D{0,0},
    m_swapchain->GetSurfaceExtent(),
  };

  VkRenderPassBeginInfo rpBI{
    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    nullptr
  };

  rpBI.renderPass = GetRenderPass("deferred");
  rpBI.framebuffer = m_fbGbuffers[imageIndex];
  rpBI.renderArea = renderArea;
  rpBI.pClearValues = clearVals;
  rpBI.clearValueCount = _countof(clearVals);
  vkBeginCommandBuffer(command, &commandBI);


  vkCmdBeginRenderPass(command, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

  auto extent = m_swapchain->GetSurfaceExtent();
  VkViewport viewport = book_util::GetViewportFlipped(float(extent.width), float(extent.height));
  VkRect2D scissor{
    { 0, 0},
    extent
  };
  vkCmdSetScissor(command, 0, 1, &scissor);
  vkCmdSetViewport(command, 0, 1, &viewport);

  // Draw : Depth Prepass
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[DepthPrepassPipeline]);
  DrawModel(command);

  // Draw : GBuffer Pass
  vkCmdNextSubpass(command, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[DrawGBufferPipeline]);
  DrawModel(command);

  // Draw : Deferred Lighiting Pass.
  vkCmdNextSubpass(command, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[LightingPassPipeline]);
  vkCmdBindDescriptorSets(
    command, VK_PIPELINE_BIND_POINT_GRAPHICS,
    GetPipelineLayout("deferredLighting"),
    0, 1, &m_dsDeferredLighting[imageIndex],
    0, nullptr
  );
  vkCmdDraw(command, 4, 1, 0, 0);
  vkCmdEndRenderPass(command);
  
  // UI 描画.
  rpBI.framebuffer = m_framebuffers[imageIndex];
  rpBI.renderPass = GetRenderPass("default");
  vkCmdBeginRenderPass(command, &rpBI, VK_SUBPASS_CONTENTS_INLINE);
  RenderHUD(command);
  vkCmdEndRenderPass(command);


  vkEndCommandBuffer(command);

  VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo{
    VK_STRUCTURE_TYPE_SUBMIT_INFO,
    nullptr,
    1, &m_presentCompletedSem, // WaitSemaphore
    &waitStageMask, // DstStageMask
    1, &command, // CommandBuffer
    1, &m_renderCompletedSem, // SignalSemaphore
  };
  vkResetFences(m_device, 1, &fence);
  vkQueueSubmit(m_deviceQueue, 1, &submitInfo, fence);

  m_swapchain->QueuePresent(m_deviceQueue, imageIndex, m_renderCompletedSem);
  m_frameCount++;

}


void DeferredRenderApp::PrepareFramebuffers()
{
  auto imageCount = m_swapchain->GetImageCount();
  auto extent = m_swapchain->GetSurfaceExtent();
  auto renderPass = GetRenderPass("default");
  

  m_framebuffers.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    vector<VkImageView> views;
    views.push_back(m_swapchain->GetImageView(i));
    views.push_back(m_depthBuffer.view);

    m_framebuffers[i] = CreateFramebuffer(
      renderPass,
      extent.width, extent.height,
      uint32_t(views.size()), views.data()
    );
  }
}

bool DeferredRenderApp::OnSizeChanged(uint32_t width, uint32_t height)
{
  auto result = VulkanAppBase::OnSizeChanged(width, height);
  if (result)
  {
    DestroyImage(m_depthBuffer);
    DestroyFramebuffers(uint32_t(m_framebuffers.size()), m_framebuffers.data());

    // デプスバッファを再生成.
    auto extent = m_swapchain->GetSurfaceExtent();
    m_depthBuffer = CreateTexture(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // フレームバッファを準備.
    PrepareFramebuffers();
  }
  return result;
}


void DeferredRenderApp::CreatePipeline()
{
  auto stride = uint32_t(sizeof(glm::vec3) * 2 + sizeof(glm::vec2));
  std::vector<VkVertexInputBindingDescription> vibDescs{
    // binding, stride, rate
    { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
    { 1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX },
    { 2, sizeof(glm::vec2), VK_VERTEX_INPUT_RATE_VERTEX },
  };

  std::vector<VkVertexInputAttributeDescription> inputAttribs{
    // location, binding, format, offset
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 },
  };

  VkPipelineVertexInputStateCreateInfo pipelineVisCI{
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    nullptr, 0,
    uint32_t(vibDescs.size()), vibDescs.data(),
    uint32_t(inputAttribs.size()), inputAttribs.data()
  };

  auto blendAttachmentState = book_util::GetOpaqueColorBlendAttachmentState();
  VkPipelineColorBlendStateCreateInfo colorBlendStateCI{
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    nullptr, 0,
    VK_FALSE, VK_LOGIC_OP_CLEAR, // logicOpEnable
    1, &blendAttachmentState,
    { 0.0f, 0.0f, 0.0f,0.0f }
  };
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_FALSE,
  };
  VkPipelineMultisampleStateCreateInfo multisampleCI{
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    nullptr, 0,
    VK_SAMPLE_COUNT_1_BIT,
    VK_FALSE, // sampleShadingEnable
    0.0f, nullptr,
    VK_FALSE, VK_FALSE,
  };

  auto extent = m_swapchain->GetSurfaceExtent();
  VkViewport viewport = book_util::GetViewportFlipped(float(extent.width), float(extent.height));
  VkRect2D scissor{
    { 0, 0},
    extent
  };
  VkPipelineViewportStateCreateInfo viewportCI{
    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    nullptr, 0,
    1, &viewport,
    1, &scissor,
  };

  VkResult result;
  auto rasterizerState = book_util::GetDefaultRasterizerState();
  auto dsState = book_util::GetDefaultDepthStencilState();
  rasterizerState.cullMode = VK_CULL_MODE_BACK_BIT;

  // DynamicState
  vector<VkDynamicState> dynamicStates{
    VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT
  };
  VkPipelineDynamicStateCreateInfo pipelineDynamicStateCI{
    VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0,
    uint32_t(dynamicStates.size()), dynamicStates.data(),
  };

  VkGraphicsPipelineCreateInfo pipelineCI{
    VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    nullptr, 0,
    0, nullptr,
    &pipelineVisCI, &inputAssemblyCI,
    nullptr, // Tessellation
    &viewportCI, // ViewportState
    &rasterizerState,
    &multisampleCI,
    &dsState,
    &colorBlendStateCI,
    &pipelineDynamicStateCI, // DynamicState
    VK_NULL_HANDLE,
    VK_NULL_HANDLE,
    0, // subpass
    VK_NULL_HANDLE, 0, // basePipeline
  };

  // 描画用パイプラインの構築.
  {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      book_util::LoadShader(m_device,"assets/shader/depthPrepassVS.spv", VK_SHADER_STAGE_VERTEX_BIT),
      book_util::LoadShader(m_device,"assets/shader/depthPrepassFS.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.stageCount = uint32_t(shaderStages.size());
    pipelineCI.layout = GetPipelineLayout("u2t2");
    pipelineCI.renderPass = GetRenderPass("deferred");
    pipelineCI.subpass = SubpassDepthPrepass;

    // Depth Prepass ではカラーの書き込みはしない.
    VkPipelineColorBlendAttachmentState colorBlendStateNoWriteColor = {
        VK_FALSE,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, // color[Src/Dst] BlendFactor
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, // alpha[Src/Dst] BlendFactor
        VK_BLEND_OP_ADD,
        0x0 };
    colorBlendStateCI.pAttachments = &colorBlendStateNoWriteColor;
    colorBlendStateCI.attachmentCount = 1;

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
    ThrowIfFailed(result, "vkCreateGraphicsPipeline Failed. (shader)");
    m_pipelines[DepthPrepassPipeline] = pipeline;
    book_util::DestroyShaderModules(m_device, shaderStages);
  }

  {
    // GBuffer 描画パスではデプスの書き込みはしないが、デプステストは使う.
    // カラー書き込み無し / デプス書き込み ON
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      book_util::LoadShader(m_device,"assets/shader/gbufferVS.spv", VK_SHADER_STAGE_VERTEX_BIT),
      book_util::LoadShader(m_device,"assets/shader/gbufferFS.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.stageCount = uint32_t(shaderStages.size());
    pipelineCI.layout = GetPipelineLayout("u2t2");
    pipelineCI.renderPass = GetRenderPass("deferred");
    pipelineCI.subpass = SubpassGbuffer;
    VkPipelineColorBlendAttachmentState state = {
        VK_TRUE,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, // color[Src/Dst] BlendFactor
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, // alpha[Src/Dst] BlendFactor
        VK_BLEND_OP_ADD,
        0xF };
    VkPipelineColorBlendAttachmentState colors[3]{
      state, state, state,
    };
    colorBlendStateCI.attachmentCount = _countof(colors);
    colorBlendStateCI.pAttachments = colors;
    
    dsState = book_util::GetDefaultDepthStencilState();
    dsState.depthTestEnable = true;
    dsState.depthWriteEnable = false;
    dsState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineCI.pDepthStencilState = &dsState;

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
    ThrowIfFailed(result, "vkCreateGraphicsPipeline Failed. (shader)");
    m_pipelines[DrawGBufferPipeline] = pipeline;

    book_util::DestroyShaderModules(m_device, shaderStages);
  }

  // ライティング用パス
  {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      book_util::LoadShader(m_device,"assets/shader/deferredLightingVS.spv", VK_SHADER_STAGE_VERTEX_BIT),
      book_util::LoadShader(m_device,"assets/shader/deferredLightingFS.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.stageCount = uint32_t(shaderStages.size());
    pipelineCI.renderPass = GetRenderPass("deferred");
    pipelineCI.layout = GetPipelineLayout("deferredLighting");
    pipelineCI.subpass = SubpassLighting;

    pipelineCI.pColorBlendState = &colorBlendStateCI;
    blendAttachmentState = book_util::GetOpaqueColorBlendAttachmentState();
    colorBlendStateCI.attachmentCount = 1;
    colorBlendStateCI.pAttachments = &blendAttachmentState;

    // フルスクリーンを覆う板は頂点シェーダー内で作成するので不要.
    VkPipelineVertexInputStateCreateInfo visCI{};
    visCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineCI.pVertexInputState = &visCI;

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
    ThrowIfFailed(result, "vkCreateGraphicsPipeline Failed. (shader)");
    m_pipelines[LightingPassPipeline] = pipeline;
    book_util::DestroyShaderModules(m_device, shaderStages);
  }


  // DepthPrepass,Gbuffer パス用のディスクリプタセットの準備.
  for (int i = 0; i<int(m_swapchain->GetImageCount()); ++i) {
    VkDescriptorBufferInfo sceneUniformUBO{
      m_uniformBuffers[i].buffer, 0, VK_WHOLE_SIZE,
    };

    auto dsLayout = GetDescriptorSetLayout("u2t2");
    auto descriptorSet = AllocateDescriptorSet(dsLayout);
    VkWriteDescriptorSet writes[] = {
      book_util::CreateWriteDescriptorSet(
        descriptorSet, DS_DRAW_SCENE_UNIFORM, &sceneUniformUBO),
    };
    vkUpdateDescriptorSets(m_device, _countof(writes), writes, 0, nullptr);
    m_dsGbuffer.push_back(descriptorSet);
  }


  // DeferredLighting パス用のディスクリプタセットの準備.
  for (int i = 0; i<int(m_swapchain->GetImageCount());++i) {
    VkDescriptorBufferInfo sceneUniformUBO{
      m_uniformBuffers[i].buffer, 0, VK_WHOLE_SIZE,
    };
    VkDescriptorImageInfo attachmentPos{
      VK_NULL_HANDLE,   // sampler
      m_rtPosition.view,//imageView
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // imageLayout
    };
    VkDescriptorImageInfo attachmentNrm{
      VK_NULL_HANDLE,   // sampler
      m_rtNormal.view,//imageView
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // imageLayout
    };
    VkDescriptorImageInfo attachmentAlbedo{
      VK_NULL_HANDLE,   // sampler
      m_rtAlbedo.view,//imageView
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  // imageLayout
    };

    auto dsLayout = GetDescriptorSetLayout("deferredLighting");
    auto descriptorSet = AllocateDescriptorSet(dsLayout);
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    VkWriteDescriptorSet writes[] = {
      book_util::CreateWriteDescriptorSet(
        descriptorSet, DS_DEFERRED_IN_POSITION, type, &attachmentPos),
      book_util::CreateWriteDescriptorSet(
        descriptorSet, DS_DEFERRED_IN_NORMAL, type, &attachmentNrm),
      book_util::CreateWriteDescriptorSet(
        descriptorSet, DS_DEFERRED_IN_ALBEDO, type, &attachmentAlbedo),
      book_util::CreateWriteDescriptorSet(
        descriptorSet, DS_DEFERRED_SCENE_UNIFORM, &sceneUniformUBO),
    };
    vkUpdateDescriptorSets(m_device, _countof(writes), writes, 0, nullptr);
    m_dsDeferredLighting.push_back(descriptorSet);
  }
}

void DeferredRenderApp::PrepareModelResource(ModelAsset& model)
{
  // モデル用のPipeline
  auto pipelineLayout = GetPipelineLayout("u2t2");
  auto dsLayout = GetDescriptorSetLayout("u2t2");


  model.pipelineLayout = pipelineLayout;
  auto imageCount = m_swapchain->GetImageCount();

  for (int i = 0; i<int(model.DrawBatches.size()); ++i) {
    auto& drawBatch = model.DrawBatches[i];
    drawBatch.descriptorSets.resize(imageCount);
    for (int j = 0; j < imageCount; ++j) {
      auto descriptorSet = AllocateDescriptorSet(dsLayout);
      drawBatch.descriptorSets[j] = descriptorSet;
      auto& material = model.materials[drawBatch.materialIndex];

      VkDescriptorBufferInfo sceneUniformUBO{
        m_uniformBuffers[j].buffer, 0, VK_WHOLE_SIZE,
      };
      VkDescriptorBufferInfo modelUniformUBO{
        drawBatch.modelMeshParameterUBO[j].buffer, 0, VK_WHOLE_SIZE,
      };

      VkDescriptorImageInfo imageAlbedo{};
      imageAlbedo.sampler = m_sampler;
      imageAlbedo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageAlbedo.imageView = material.albedo.view;

      VkDescriptorImageInfo imageSpecular{};
      imageSpecular.sampler = m_sampler;
      imageSpecular.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageSpecular.imageView = material.specular.view;

      VkWriteDescriptorSet writes[] = {
        book_util::CreateWriteDescriptorSet(descriptorSet, DS_DRAW_SCENE_UNIFORM, &sceneUniformUBO),
        book_util::CreateWriteDescriptorSet(descriptorSet, DS_DRAW_MODEL_UNIFORM, &modelUniformUBO),

        book_util::CreateWriteDescriptorSet(descriptorSet, DS_DRAW_MATERIAL_ALBEDO, &imageAlbedo),
        book_util::CreateWriteDescriptorSet(descriptorSet, DS_DRAW_MATERIAL_SPECULAR, &imageSpecular),
      };
      vkUpdateDescriptorSets(m_device, _countof(writes), writes, 0, nullptr);

    }
  } 
}

void DeferredRenderApp::RenderHUD(VkCommandBuffer command)
{
  // ImGui
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // ImGui ウィジェットを描画する.
  ImGui::Begin("Information");
  ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
  ImGui::InputFloat3("lightDir", (float*)&m_sceneParameters.lightDir);
 
  ImGui::InputInt("Mode", (int*)&m_sceneParameters.drawFlag);
  ImGui::End();

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(
    ImGui::GetDrawData(), command
  );
}

void DeferredRenderApp::DrawModel(VkCommandBuffer command)
{
  auto imageIndex = m_swapchain->GetCurrentBufferIndex();
  VkBuffer buffers[] = {
    m_model.Position.buffer, m_model.Normal.buffer, m_model.UV0.buffer, m_model.Tangent.buffer
  };
  VkDeviceSize offsets[] = { 0,0,0,0 };
  vkCmdBindVertexBuffers(command, 0, 4, buffers, offsets);
  vkCmdBindIndexBuffer(command, m_model.Indices.buffer, 0, VK_INDEX_TYPE_UINT32);

  for (auto& batch : m_model.DrawBatches) {
    const auto& material = m_model.materials[batch.materialIndex];
    ModelMeshParameters meshParameters{};
    meshParameters.mtxWorld = glm::mat4(1.0f);
    meshParameters.diffuse = vec4(material.diffuse, material.shininess);
    meshParameters.ambient = vec4(material.ambient, 0);

    WriteToHostVisibleMemory(
      batch.modelMeshParameterUBO[imageIndex].memory,
      sizeof(meshParameters),
      &meshParameters);
  }

  // DepthPrepass,GBuffer描画中はどちらも u2t2 のレイアウトを使う.
  auto layout = GetPipelineLayout("u2t2");

  for (auto& batch : m_model.DrawBatches) {
    std::vector<VkDescriptorSet> descriptorSets = {
      batch.descriptorSets[imageIndex]
    };

    vkCmdBindDescriptorSets(command, 
      VK_PIPELINE_BIND_POINT_GRAPHICS, 
      layout, 
      0, 
      uint32_t(descriptorSets.size()),
      descriptorSets.data(),
      0, nullptr);
    vkCmdDrawIndexed(command, batch.indexCount, 1, batch.indexOffsetCount, batch.vertexOffsetCount, 0);
  }
}

void DeferredRenderApp::CreateSampleLayouts()
{
  // ディスクリプタセットレイアウトの準備.
  std::vector<VkDescriptorSetLayoutBinding> dsLayoutBindings;
  VkDescriptorSetLayoutCreateInfo dsLayoutCI{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
  };
  VkResult result;
  VkDescriptorSetLayout dsLayout = VK_NULL_HANDLE;

  dsLayoutBindings = {
    { DS_DRAW_SCENE_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_DRAW_MODEL_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_DRAW_MATERIAL_ALBEDO, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, },
    { DS_DRAW_MATERIAL_SPECULAR, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, },
  };
  dsLayoutCI.pBindings = dsLayoutBindings.data();
  dsLayoutCI.bindingCount = uint32_t(dsLayoutBindings.size());
  result = vkCreateDescriptorSetLayout(m_device, &dsLayoutCI, nullptr, &dsLayout);
  ThrowIfFailed(result, "vkCreateDescriptorSetLayout Failed (u2t2).");
  RegisterLayout("u2t2", dsLayout); dsLayout = VK_NULL_HANDLE;

  dsLayoutBindings = {
    { DS_DEFERRED_SCENE_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_DEFERRED_IN_POSITION, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, },
    { DS_DEFERRED_IN_NORMAL, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, },
    { DS_DEFERRED_IN_ALBEDO, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, },
  };
  dsLayoutCI.pBindings = dsLayoutBindings.data();
  dsLayoutCI.bindingCount = uint32_t(dsLayoutBindings.size());
  result = vkCreateDescriptorSetLayout(m_device, &dsLayoutCI, nullptr, &dsLayout);
  ThrowIfFailed(result, "vkCreateDescriptorSetLayout Failed (deferredLighting).");
  RegisterLayout("deferredLighting", dsLayout); dsLayout = VK_NULL_HANDLE;

  // パイプラインレイアウトの準備.
  VkPipelineLayoutCreateInfo layoutCI{
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
  };
  VkPipelineLayout layout;
  
  dsLayout = GetDescriptorSetLayout("u2t2");
  layoutCI.setLayoutCount = 1;
  layoutCI.pSetLayouts = &dsLayout;
  result = vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &layout);
  ThrowIfFailed(result, "vkCreatePipelineLayout Failed(u2t2).");
  RegisterLayout("u2t2", layout); layout = VK_NULL_HANDLE;

  dsLayout = GetDescriptorSetLayout("deferredLighting");
  layoutCI.setLayoutCount = 1;
  layoutCI.pSetLayouts = &dsLayout;
  result = vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &layout);
  ThrowIfFailed(result, "vkCreatePipelineLayout Failed(deferredLighting).");
  RegisterLayout("deferredLighting", layout); layout = VK_NULL_HANDLE;
}

