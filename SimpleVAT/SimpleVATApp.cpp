#include "SimpleVATApp.h"
#include "VulkanBookUtil.h"

#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <array>

using namespace std;
using namespace glm;

SimpleVATApp::SimpleVATApp()
{
  m_camera.SetLookAt(
    vec3(10.0f, 10.0f, 10.0f),
    vec3(0.0f, 2.5f, -0.0f) 
  );
  m_sceneParameters.lightDir = glm::vec4(1, 1, 0.2f, 0);

  m_animeAuto = true;
  m_sceneParameters.drawFlag = m_animeAuto ? 1 : 0;
  m_sceneParameters.animationFrame = 0;

  m_materialFluid.mtxWorld = glm::mat4(1.0f);
  m_materialFluid.diffuse = glm::vec4(0.65f, 0.85f, 1, 1);
  m_materialFluid.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1);

  m_materialDestroy.mtxWorld = glm::mat4(1.0f);
  m_materialDestroy.diffuse = glm::vec4(0.85f, 0.25f, 0.3f, 1);
  m_materialDestroy.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1);

}


void SimpleVATApp::Prepare()
{
  CreateSampleLayouts();

  auto colorFormat = m_swapchain->GetSurfaceFormat().format;
  RegisterRenderPass("default", CreateRenderPass(colorFormat, VK_FORMAT_D32_SFLOAT) );
  
  // �f�v�X�o�b�t�@����������.
  auto extent = m_swapchain->GetSurfaceExtent();
  m_depthBuffer = CreateTexture(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  // �t���[���o�b�t�@�̏���.
  PrepareFramebuffers();
  auto imageCount = m_swapchain->GetImageCount();

  // �R�}���h�o�b�t�@�̏���.
  m_commandBuffers.resize(imageCount);
  for (auto& c : m_commandBuffers)
  {
    c.fence = CreateFence();
    c.commandBuffer = CreateCommandBuffer(false); // �R�}���h�o�b�t�@�J�n��Ԃɂ��Ȃ�.
  }

  // �萔�o�b�t�@�̏���.
  auto bufferSize = uint32_t(sizeof(ShaderParameters));
  m_uniformBuffers = CreateUniformBuffers(bufferSize, imageCount);


  CreatePipeline();

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

  m_texPlaneBase = LoadTexture("assets/texture/block.jpg");
  m_model = LoadModelData("assets/model/plane.obj");

  PrepareVATData();

  PrepareModelResource(m_model);

  m_model.rootNode->UpdateMatrices(glm::mat4(1.0f));

  m_camera.SetPerspective(
    radians(45.0f), float(extent.width) / float(extent.height), 0.1f, 1000.0f
  );


}

void SimpleVATApp::Cleanup()
{
  m_model.Release(this);

  for (auto& ubo : m_uniformBuffers)
  {
    DestroyBuffer(ubo);
  }
  for (auto& ubo : m_materialVATUBO) {
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

bool SimpleVATApp::OnMouseButtonDown(int msg)
{
  if (VulkanAppBase::OnMouseButtonDown(msg))
    return true;

  m_camera.OnMouseButtonDown(msg);
  return true;
}

bool SimpleVATApp::OnMouseMove(int dx, int dy)
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

bool SimpleVATApp::OnMouseButtonUp(int msg)
{
  if (VulkanAppBase::OnMouseButtonUp(msg))
    return true;

  m_camera.OnMouseButtonUp();
  return true;
}

void SimpleVATApp::Render()
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
  array<VkClearValue, 2> clearValue = {
    {
      { 0.1f, 0.1f, 0.1f, 0.0f}, // for Color
      { 1.0f, 0 }, // for Depth
    }
  };

  auto renderArea = VkRect2D{
    VkOffset2D{0,0},
    m_swapchain->GetSurfaceExtent(),
  };

  VkRenderPassBeginInfo rpBI{
    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    nullptr,
    GetRenderPass("default"),
    m_framebuffers[imageIndex],
    renderArea,
    uint32_t(clearValue.size()), clearValue.data()
  };

  VkCommandBufferBeginInfo commandBI{
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    nullptr, 0, nullptr
  };

  {
    // ���j�t�H�[���o�b�t�@�̍X�V.
    m_sceneParameters.view = m_camera.GetViewMatrix();
    auto extent = m_swapchain->GetSurfaceExtent();
    m_sceneParameters.proj = m_camera.GetProjectionMatrix();
    m_sceneParameters.cameraPosition = vec4(m_camera.GetPosition(), 0);
    m_sceneParameters.frameDeltaTime = float(GetFrameDeltaTime());
    m_sceneParameters.frameCountLow = uint32_t(m_frameCount & 0xFFFFFFFFu);
    auto ubo = m_uniformBuffers[imageIndex];
    WriteToHostVisibleMemory(ubo.memory, sizeof(ShaderParameters), &m_sceneParameters);
  }

  auto fence = m_commandBuffers[imageIndex].fence;
  vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

  auto command = m_commandBuffers[imageIndex].commandBuffer;

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

  // ���`��.
  m_model.rootNode->UpdateMatrices(glm::mat4(1.0f));
  DrawModel(command);

  // VAT �`��.
  auto pipeline = m_pipelines[DrawVATPipeline];
  uint32_t vertexCount = 0;
  uint32_t maxAnimationCount = 0;
  VkDescriptorSet ds;
  if ( m_mode == DrawMode_Fluid ) {
    ds = m_vatFluid.descriptorSet[imageIndex];
    vertexCount = m_vatFluid.vertexCount;
    maxAnimationCount = m_vatFluid.animationCount;
    WriteToHostVisibleMemory(m_materialVATUBO[imageIndex].memory, sizeof(m_materialFluid), &m_materialFluid);
  }
  if (m_mode == DrawMode_Destroy) {
    ds = m_vatDestroy.descriptorSet[imageIndex];
    vertexCount = m_vatDestroy.vertexCount;
    maxAnimationCount = m_vatDestroy.animationCount;
    WriteToHostVisibleMemory(m_materialVATUBO[imageIndex].memory, sizeof(m_materialDestroy), &m_materialDestroy);
  }

  auto pipelineLayout = GetPipelineLayout("u2t2");
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(
    command, VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0,
    1,
    &ds,
    0,
    nullptr
  );
  vkCmdDraw(command, vertexCount, 1, 0, 0);

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

  if (m_animeAuto) {
    m_sceneParameters.animationFrame = (++m_sceneParameters.animationFrame) % maxAnimationCount;
  }
}


void SimpleVATApp::PrepareFramebuffers()
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

bool SimpleVATApp::OnSizeChanged(uint32_t width, uint32_t height)
{
  auto result = VulkanAppBase::OnSizeChanged(width, height);
  if (result)
  {
    DestroyImage(m_depthBuffer);
    DestroyFramebuffers(uint32_t(m_framebuffers.size()), m_framebuffers.data());

    // �f�v�X�o�b�t�@���Đ���.
    auto extent = m_swapchain->GetSurfaceExtent();
    m_depthBuffer = CreateTexture(extent.width, extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // �t���[���o�b�t�@������.
    PrepareFramebuffers();
  }
  return result;
}


void SimpleVATApp::CreatePipeline()
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
  auto renderPass = GetRenderPass("default");
  auto layout = GetPipelineLayout("u1s1");

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


  // �p�C�v���C���\�z.
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
    layout,
    renderPass,
    0, // subpass
    VK_NULL_HANDLE, 0, // basePipeline
  };

  // ���f���`��p�p�C�v���C���̍\�z.
  {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      book_util::LoadShader(m_device,"assets/shader/shaderVS.spv", VK_SHADER_STAGE_VERTEX_BIT),
      book_util::LoadShader(m_device,"assets/shader/shaderFS.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.stageCount = uint32_t(shaderStages.size());
    pipelineCI.layout = GetPipelineLayout("u2t1");
    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
    ThrowIfFailed(result, "vkCreateGraphicsPipeline Failed. (shader)");

    book_util::DestroyShaderModules(m_device, shaderStages);
    m_pipelines[DrawFloorPipeline] = pipeline;
  }
  // VAT�`��p�p�C�v���C���̍\�z.
  {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages
    {
      book_util::LoadShader(m_device,"assets/shader/shaderVatVS.spv", VK_SHADER_STAGE_VERTEX_BIT),
      book_util::LoadShader(m_device,"assets/shader/shaderVatFS.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.stageCount = uint32_t(shaderStages.size());
    pipelineCI.layout = GetPipelineLayout("u2t2");

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
    ThrowIfFailed(result, "vkCreateGraphicsPipeline Failed. (shader)");

    book_util::DestroyShaderModules(m_device, shaderStages);
    m_pipelines[DrawVATPipeline] = pipeline;
  }
}

void SimpleVATApp::PrepareVATData()
{
  // VAT �f�[�^�`��p�̃}�e���A����ݒ�.
  m_materialVATUBO = CreateUniformBuffers(sizeof(ModelMeshParameters), m_swapchain->GetImageCount());

  m_vatFluid = LoadVAT("assets/vat/FluidSample");
  m_vatDestroy = LoadVAT("assets/vat/DestroyWall");
}


void SimpleVATApp::PrepareModelResource(ModelAsset& model)
{
  // ���f���p��Pipeline
  auto pipelineLayout = GetPipelineLayout("u2t1");
  auto dsLayout = GetDescriptorSetLayout("u2t1");


  model.pipelineLayout = pipelineLayout;
  auto imageCount = m_swapchain->GetImageCount();

  for (int i = 0; i<int(model.DrawBatches.size()); ++i) {
    auto& drawBatch = model.DrawBatches[i];
    drawBatch.descriptorSets.resize(imageCount);
    for (int j = 0; j < int(imageCount); ++j) {
      auto descriptorSet = AllocateDescriptorSet(dsLayout);
      drawBatch.descriptorSets[j] = descriptorSet;
      auto& material = model.materials[drawBatch.materialIndex];

      VkDescriptorBufferInfo sceneUniformUBO{
        m_uniformBuffers[j].buffer, 0, VK_WHOLE_SIZE,
      };
      VkDescriptorBufferInfo modelUniformUBO{
        drawBatch.modelMeshParameterUBO[j].buffer, 0, VK_WHOLE_SIZE,
      };

      // �e�N�X�`���Q�͍��͌Œ�̂��̂��㏑���ݒ�.
      VkDescriptorImageInfo modelTexture{};
      modelTexture.sampler = m_sampler;
      modelTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      modelTexture.imageView = m_texPlaneBase.view;

      VkWriteDescriptorSet writes[] = {
        book_util::CreateWriteDescriptorSet(descriptorSet, DS_SCENE_UNIFORM, &sceneUniformUBO),
        book_util::CreateWriteDescriptorSet(descriptorSet, DS_MODEL_UNIFORM, &modelUniformUBO),

        book_util::CreateWriteDescriptorSet(descriptorSet, DS_MATERIAL_ALBEDO, &modelTexture),
      };
      vkUpdateDescriptorSets(m_device, _countof(writes), writes, 0, nullptr);

    }
  }
  
}

void SimpleVATApp::RenderHUD(VkCommandBuffer command)
{
  // ImGui
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // ImGui �E�B�W�F�b�g��`�悷��.
  ImGui::Begin("Information");
  ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
  ImGui::InputFloat3("lightDir", (float*)&m_sceneParameters.lightDir);
  if (ImGui::Combo("Mode", (int*)&m_mode, "Fluid\0Destroy\0\0")) {
    m_sceneParameters.animationFrame = 0;
  }
  
  ImGui::Checkbox("Auto Animation", &m_animeAuto);

  ImGui::InputInt("AnimFrame", (int*) & m_sceneParameters.animationFrame);
  ImGui::End();

  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(
    ImGui::GetDrawData(), command
  );
}

void SimpleVATApp::DrawModel(VkCommandBuffer command)
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
    meshParameters.diffuse = glm::vec4(material.diffuse, material.shininess);
    meshParameters.ambient = glm::vec4(material.ambient, 0);

    WriteToHostVisibleMemory(
      batch.modelMeshParameterUBO[imageIndex].memory,
      sizeof(meshParameters),
      &meshParameters);
  }

  auto layout = m_model.pipelineLayout;
  auto pipeline = m_pipelines[DrawFloorPipeline];
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);


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

SimpleVATApp::VATData SimpleVATApp::LoadVAT(std::string path)
{
  VATData vatData{};
  vatData.texPosition = LoadTexture(path + ".ptex.ktx");
  vatData.texNormal = LoadTexture(path + ".ntex.ktx");

  vatData.vertexCount = vatData.texPosition.width;
  vatData.animationCount = vatData.texPosition.height;

  auto imageCount = m_swapchain->GetImageCount();
  vatData.descriptorSet.resize(imageCount);
  auto dsLayout = GetDescriptorSetLayout("u2t2");
  for (uint32_t i = 0; i < imageCount; ++i) {
    auto descriptorSet = AllocateDescriptorSet(dsLayout);
    vatData.descriptorSet[i] = descriptorSet;
    
    VkDescriptorBufferInfo sceneUniformUBO{
      m_uniformBuffers[i].buffer, 0, VK_WHOLE_SIZE,
    };
    VkDescriptorBufferInfo modelUniformUBO{
      m_materialVATUBO[i].buffer, 0, VK_WHOLE_SIZE,
    };
    VkDescriptorImageInfo vatPosition{};
    vatPosition.sampler = m_sampler;
    vatPosition.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vatPosition.imageView = vatData.texPosition.view;

    VkDescriptorImageInfo vatNormal{};
    vatNormal.sampler = m_sampler;
    vatNormal.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vatNormal.imageView = vatData.texNormal.view;

    VkWriteDescriptorSet writes[] = {
      book_util::CreateWriteDescriptorSet(descriptorSet, DS_VAT_SCENE_UNIFORM, &sceneUniformUBO),
      book_util::CreateWriteDescriptorSet(descriptorSet, DS_VAT_MODEL_UNIFORM, &modelUniformUBO),

      book_util::CreateWriteDescriptorSet(descriptorSet, DS_VAT_VATDATA_POSITION, &vatPosition),
      book_util::CreateWriteDescriptorSet(descriptorSet, DS_VAT_VATDATA_NORMAL, &vatNormal),
    };
    vkUpdateDescriptorSets(m_device, _countof(writes), writes, 0, nullptr);
  }
  
  return vatData;
}

void SimpleVATApp::CreateSampleLayouts()
{
  // �f�B�X�N���v�^�Z�b�g���C�A�E�g�̏���.
  std::vector<VkDescriptorSetLayoutBinding> dsLayoutBindings;
  VkDescriptorSetLayoutCreateInfo dsLayoutCI{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
  };
  VkResult result;
  VkDescriptorSetLayout dsLayout = VK_NULL_HANDLE;

  dsLayoutBindings = {
    { DS_SCENE_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_MODEL_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_MATERIAL_ALBEDO, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, },
  };
  dsLayoutCI.pBindings = dsLayoutBindings.data();
  dsLayoutCI.bindingCount = uint32_t(dsLayoutBindings.size());
  result = vkCreateDescriptorSetLayout(m_device, &dsLayoutCI, nullptr, &dsLayout);
  ThrowIfFailed(result, "vkCreateDescriptorSetLayout Failed (u2t1).");
  RegisterLayout("u2t1", dsLayout); dsLayout = VK_NULL_HANDLE;

  dsLayoutBindings = {
    { DS_VAT_SCENE_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_VAT_MODEL_UNIFORM, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, },
    { DS_VAT_VATDATA_POSITION, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, },
    { DS_VAT_VATDATA_NORMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, },
  };
  dsLayoutCI.pBindings = dsLayoutBindings.data();
  dsLayoutCI.bindingCount = uint32_t(dsLayoutBindings.size());
  result = vkCreateDescriptorSetLayout(m_device, &dsLayoutCI, nullptr, &dsLayout);
  ThrowIfFailed(result, "vkCreateDescriptorSetLayout Failed (u2t1).");
  RegisterLayout("u2t2", dsLayout); dsLayout = VK_NULL_HANDLE;


  // �p�C�v���C�����C�A�E�g�̏���.
  VkPipelineLayoutCreateInfo layoutCI{
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
  };
  VkPipelineLayout layout;
  
  dsLayout = GetDescriptorSetLayout("u2t1");
  layoutCI.setLayoutCount = 1;
  layoutCI.pSetLayouts = &dsLayout;
  result = vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &layout);
  ThrowIfFailed(result, "vkCreatePipelineLayout Failed(u2t1).");
  RegisterLayout("u2t1", layout); layout = VK_NULL_HANDLE;

  dsLayout = GetDescriptorSetLayout("u2t2");
  layoutCI.setLayoutCount = 1;
  layoutCI.pSetLayouts = &dsLayout;
  result = vkCreatePipelineLayout(m_device, &layoutCI, nullptr, &layout);
  ThrowIfFailed(result, "vkCreatePipelineLayout Failed(u2t2).");
  RegisterLayout("u2t2", layout); layout = VK_NULL_HANDLE;
}

