#include "ManualMoviePlayer.h"
#include "VulkanBookUtil.h"

// for MediaFoundation
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")


ManualMoviePlayer::ManualMoviePlayer()
{
  m_format = VK_FORMAT_B8G8R8A8_UNORM;
  //m_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
}

ManualMoviePlayer::~ManualMoviePlayer()
{
  Terminate();
}

void ManualMoviePlayer::Initialize(VulkanAppBase* appBase)
{
  m_appBase = appBase;
}

void ManualMoviePlayer::Terminate()
{
  Stop();
  if (m_appBase == nullptr) {
    return;
  }

  if ( m_format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    // 別の VkImageView を作成したため解放.
    vkDestroyImageView(m_appBase->GetDevice(), m_chromaView, nullptr);
    m_chromaView = VK_NULL_HANDLE;
  }
  m_appBase->DestroyImage(m_movieTextureRes);
  for (auto& buffer : m_frameDecoded) {
    m_appBase->DestroyBuffer(buffer);
  }
  m_appBase = nullptr;
}

void ManualMoviePlayer::Update(VkCommandBuffer command)
{
  if (!m_isPlaying) {
    return;
  }
  if (m_isLoop == true && m_lastFrameDisplayed == true) {
    QueryPerformanceCounter(&m_startedTime);
    m_lastFrameDisplayed = false;
    OutputDebugStringA("Reset started time.\n");
  }

  DecodeFrame();
  UpdateTexture(command);
}

void ManualMoviePlayer::SetMediaSource(const Path& fileName)
{
  auto fullPath = std::filesystem::absolute(fileName).wstring();

  // フォーマット変換のために必要.
  IMFAttributes* attribs;
  MFCreateAttributes(&attribs, 1);
  attribs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

  HRESULT hr = MFCreateSourceReaderFromURL(fullPath.c_str(), attribs, &m_mfSourceReader);
  if (FAILED(hr)) {
    throw std::runtime_error("MFCreateSourceReaderFromURL() Failed");
  }

  GUID desiredFormat = MFVideoFormat_RGB32;
  if (m_format == VK_FORMAT_B8G8R8A8_UNORM) {
    desiredFormat = MFVideoFormat_RGB32;
  }
  if (m_format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    desiredFormat = MFVideoFormat_NV12;
  }

  ComPtr<IMFMediaType> mediaType;
  MFCreateMediaType(&mediaType);
  mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  hr = mediaType->SetGUID(MF_MT_SUBTYPE, desiredFormat);
  hr = m_mfSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, mediaType.Get());

  // ビデオ情報を取得.
  m_mfSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mediaType);
  MFGetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE, &m_width, &m_height);
  UINT32 nume, denom;
  MFGetAttributeRatio(mediaType.Get(), MF_MT_FRAME_RATE, &nume, &denom);
  m_fps = (double)nume / denom;

  hr = mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, &m_srcStride);

  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  // テクスチャを用意する.
  if (m_format != VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    m_movieTextureRes = m_appBase->CreateTexture(
      m_width, m_height,
      m_format,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    );

    // dummy
    m_chromaView = m_movieTextureRes.view;
  } else {
    VkImageCreateInfo imageCI{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      nullptr, 0,
      VK_IMAGE_TYPE_2D,
      m_format, { m_width, m_height, 1 },
      1, 1, VK_SAMPLE_COUNT_1_BIT,
      VK_IMAGE_TILING_LINEAR,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_SHARING_MODE_EXCLUSIVE,
      0, nullptr,
      VK_IMAGE_LAYOUT_UNDEFINED
    };
    imageCI.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    auto vkdevice = m_appBase->GetDevice();
    vkCreateImage(vkdevice, &imageCI, nullptr, &m_movieTextureRes.image);

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(vkdevice, m_movieTextureRes.image, &reqs);
    VkMemoryAllocateInfo info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      nullptr,
      reqs.size,
      m_appBase->GetMemoryTypeIndex(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vkAllocateMemory(vkdevice, &info, nullptr, &m_movieTextureRes.memory);
    vkBindImageMemory(vkdevice, m_movieTextureRes.image, m_movieTextureRes.memory, 0);

    VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    imageAspect = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
    imageAspect = VK_IMAGE_ASPECT_PLANE_0_BIT;
    VkImageViewCreateInfo viewCI{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      nullptr, 0,
      m_movieTextureRes.image,
      VK_IMAGE_VIEW_TYPE_2D,
      imageCI.format,
      book_util::DefaultComponentMapping(),
      { imageAspect, 0, 1, 0, 1}
    };

    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
    viewCI.format = VK_FORMAT_R8_UNORM;
    vkCreateImageView(vkdevice, &viewCI, nullptr, &m_movieTextureRes.view);

    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
    viewCI.format = VK_FORMAT_R8G8_UNORM;
    vkCreateImageView(vkdevice, &viewCI, nullptr, &m_chromaView);

    VkImageSubresource subres{};
    subres.mipLevel = 0;
    subres.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
    subres.arrayLayer = 0;
    
    vkGetImageSubresourceLayout(vkdevice, 
      m_movieTextureRes.image, 
      &subres, &m_plane0);

    subres.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
    vkGetImageSubresourceLayout(vkdevice,
      m_movieTextureRes.image,
      &subres, &m_plane1);

    aspectFlags = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }

  VkImageMemoryBarrier imb{
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
    0, VK_ACCESS_SHADER_READ_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
    m_movieTextureRes.image,
    { aspectFlags, 0, 1, 0, 1 }
  };
  auto command = m_appBase->CreateCommandBuffer();
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr,
    0, nullptr, 1, &imb);
  m_appBase->FinishCommandBuffer(command);
  m_appBase->DestroyCommandBuffer(command);

  // テクスチャ転送用バッファ(ステージングバッファ).
  auto totalBytes = uint32_t(m_width * m_height * sizeof(UINT32));
  for (auto& buffer : m_frameDecoded) {
    buffer = m_appBase->CreateBuffer(totalBytes,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (buffer.buffer == VK_NULL_HANDLE) {
      throw std::runtime_error("CreateBuffer failed.");
    }
  }
}

void ManualMoviePlayer::Play()
{
  if (m_isPlaying) {
    return;
  }

  QueryPerformanceCounter(&m_startedTime);
  m_isPlaying = true;
}

void ManualMoviePlayer::Stop()
{
  if (!m_isPlaying) {
    return;
  }

  m_isPlaying = false;
  m_isFinished = false;
  m_isDecodeFinished = false;

  m_decoded.clear();
  m_writeBufferIndex = 0;
  if (m_mfSourceReader) {
    ResetPosition();
  }
}

void ManualMoviePlayer::SetLoop(bool enable)
{
  m_isLoop = true;
}

VkImageView ManualMoviePlayer::GetMovieTexture()
{
  return m_movieTextureRes.view;
}

void ManualMoviePlayer::DecodeFrame()
{
  if (m_decoded.size() == DecodeBufferCount) {
    return;
  }
  if (m_isDecodeFinished) {
    return;
  }

  if (m_mfSourceReader) {
    DWORD flags;
    ComPtr<IMFSample> sample;
    HRESULT hr = m_mfSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, NULL, &sample);
    if (SUCCEEDED(hr)) {
      if (flags == MF_SOURCE_READERF_ENDOFSTREAM) {
        OutputDebugStringA("Stream End.\n");

        ResetPosition();
        m_isDecodeFinished = true;
      } else {
        // 時間情報の取得.
        int64_t ts = 0;
        int64_t duration = 0;
        ComPtr<IMFMediaBuffer> buffer;
        sample->GetBufferByIndex(0, &buffer);

        sample->GetSampleTime(&ts);
        sample->GetSampleDuration(&duration);

        BYTE* src;
        DWORD size;
        buffer->Lock(&src, NULL, &size);

        if (m_format == VK_FORMAT_B8G8R8A8_UNORM) {
          // ステージングバッファへ書き込んでおく.
          auto writeBuffer = m_frameDecoded[m_writeBufferIndex];
          UINT srcPitch = m_width * sizeof(UINT);
          m_appBase->WriteToHostVisibleMemory(
            writeBuffer.memory,
            srcPitch * m_height,
            src
          );
        }
        if (m_format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
          // ステージングバッファへ書き込んでおく.
          auto writeBuffer = m_frameDecoded[m_writeBufferIndex];
          m_appBase->WriteToHostVisibleMemory(
            writeBuffer.memory,
            size,
            src
          );
        }
        buffer->Unlock();


        // 100ns単位 => ns 単位へ.
        ts *= 100;
        duration *= 100;
        double durationSec = double(duration) / 1000000000;
        double timestamp = double(ts) / 1000000000;

        //char buf[256];
        //sprintf_s(buf, "%f (%f)\n", timestamp, durationSec);
        //OutputDebugStringA(buf);

        MovieFrameInfo frameInfo{};
        frameInfo.timestamp = timestamp;
        frameInfo.bufferIndex = m_writeBufferIndex;

        m_decoded.push_back(frameInfo);

        m_writeBufferIndex = (m_writeBufferIndex + 1) % DecodeBufferCount;
      }
    }
  }
}

void ManualMoviePlayer::UpdateTexture(VkCommandBuffer command)
{
  if (m_decoded.empty()) {
    return;
  }
  auto frameInfo = m_decoded.front();

  LARGE_INTEGER now, freq;
  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&freq);
  double playTime = double(now.QuadPart - m_startedTime.QuadPart) / freq.QuadPart;

  // 表示してよい時間かチェック.
  if (playTime < frameInfo.timestamp) {
    //OutputDebugStringA("Stay...\n");
    return;
  }
  m_decoded.pop_front();

  if (m_isDecodeFinished == true && m_decoded.empty()) {
    m_isFinished = true;
    if (m_isLoop) {
      m_isFinished = false;
      m_isDecodeFinished = false;
      m_lastFrameDisplayed = true;
    }
  }

  // テクスチャの転送
  auto srcBuffer = m_frameDecoded[frameInfo.bufferIndex];
  VkBufferImageCopy region{};
  region.imageExtent = { m_width, m_height, 1};
  region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };


  VkImageMemoryBarrier imb{
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
    0, VK_ACCESS_TRANSFER_WRITE_BIT,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
    m_movieTextureRes.image,
    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
  };
  if (m_format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  // 転送までのバリアを設定.
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr,
    0, nullptr, 1, &imb);

  // Staging から転送.
  if (m_format != VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) {
    vkCmdCopyBufferToImage(
      command,
      srcBuffer.buffer, m_movieTextureRes.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  } else {
    // for Plane0
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
    vkCmdCopyBufferToImage(
      command,
      srcBuffer.buffer, m_movieTextureRes.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // for Plane1
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
    region.bufferOffset = m_width * m_height;
    
    uint32_t h = (m_height + 15) & ~15;
    region.bufferOffset = m_width * h;
    region.imageExtent.width = m_width / 2;
    region.imageExtent.height = m_height / 2;
    vkCmdCopyBufferToImage(
      command,
      srcBuffer.buffer, m_movieTextureRes.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

  // 転送完了後はシェーダーリソースとして読めるようにバリアを設定.
  imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr,
    0, nullptr,
    1, &imb);
}

void ManualMoviePlayer::ResetPosition()
{
  PROPVARIANT varPosition;
  PropVariantInit(&varPosition);
  varPosition.vt = VT_I8;
  varPosition.hVal.QuadPart = 0;
  m_mfSourceReader->SetCurrentPosition(GUID_NULL, varPosition);
  PropVariantClear(&varPosition);
}

