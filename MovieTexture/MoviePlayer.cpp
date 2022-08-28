#include "MoviePlayer.h"

#include <sstream>
#include <stdexcept>

#pragma comment(lib, "d3d11.lib")


class MediaEngineNotify : public IMFMediaEngineNotify {
  long m_cRef;
  MoviePlayer* m_callback;
public:
  MediaEngineNotify() : m_cRef(0), m_callback(nullptr) {
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
  {
    if (__uuidof(IMFMediaEngineNotify) == riid) {
      *ppv = static_cast<IMFMediaEngineNotify*>(this);
    } else {
      *ppv = nullptr;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  STDMETHODIMP_(ULONG) AddRef()
  {
    return InterlockedIncrement(&m_cRef);
  }
  STDMETHODIMP_(ULONG) Release()
  {
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
      delete this;
    }
    return cRef;
  }
  void SetCallback(MoviePlayer* target)
  {
    m_callback = target;
  }

  STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2)
  {
    if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
    {
      SetEvent(reinterpret_cast<HANDLE>(param1));
    } else
    {
      m_callback->OnMediaEngineEvent(meEvent);
    }
    return S_OK;
  }
};

MoviePlayer::MoviePlayer()
{
  m_cRef = 1;
  m_hPrepare = CreateEvent(NULL, FALSE, FALSE, NULL);
  m_hSharedTexHandle = 0;
  //m_format = DXGI_FORMAT_NV12;
}

MoviePlayer::~MoviePlayer()
{
  Terminate();
}

void MoviePlayer::Initialize(
  ComPtr<IDXGIAdapter1> adapter, 
  VulkanAppBase* appBase)
{
  m_appBase = appBase;

  ComPtr<ID3D11Device> deviceD3D11;
  D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
  HRESULT hr = D3D11CreateDevice(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
    featureLevels, 1,
    D3D11_SDK_VERSION,
    deviceD3D11.ReleaseAndGetAddressOf(),
    nullptr,
    nullptr);
  deviceD3D11.As(&m_d3d11Device);

  ComPtr<ID3D10Multithread> multithread;
  deviceD3D11.As(&multithread);
  multithread->SetMultithreadProtected(TRUE);

  // Media Engine のセットアップ.
  ComPtr<IMFDXGIDeviceManager> dxgiManager;
  UINT resetToken = 0;
  MFCreateDXGIDeviceManager(&resetToken, dxgiManager.ReleaseAndGetAddressOf());
  dxgiManager->ResetDevice(m_d3d11Device.Get(), resetToken);

  ComPtr<MediaEngineNotify> notify = new MediaEngineNotify();
  notify->SetCallback(this);

  ComPtr<IMFAttributes> attributes;
  MFCreateAttributes(attributes.GetAddressOf(), 1);
  attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, dxgiManager.Get());
  attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, notify.Get());
  attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, m_format);

  // Create media engine
  ComPtr<IMFMediaEngineClassFactory> mfFactory;
  CoCreateInstance(CLSID_MFMediaEngineClassFactory,
    nullptr,
    CLSCTX_ALL,
    IID_PPV_ARGS(mfFactory.GetAddressOf()));

  mfFactory->CreateInstance(
    0, attributes.Get(), m_mediaEngine.ReleaseAndGetAddressOf());


  VkPhysicalDeviceImageFormatInfo2KHR imageFormatInfo{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR, nullptr
  };
  
  VkPhysicalDeviceExternalImageFormatInfoKHR physicalDeviceExternalImageFormatInfo{
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR, nullptr
  };
  
  VkImageFormatProperties2 imageFormatProperties{
    VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR, nullptr
  };

  VkExternalImageFormatProperties externalImageFormatProp{
    VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR, nullptr
  };
  VkExternalMemoryHandleTypeFlagBitsKHR handleList[] = {
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR,
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT_KHR,
  };

  auto vkdevice = m_appBase->GetDevice();
  PFN_vkGetPhysicalDeviceImageFormatProperties2KHR _vkGetPhysicalDeviceImageFormatProperties2KHR;
  _vkGetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)vkGetInstanceProcAddr(
    m_appBase->GetVulkanInstance(), "vkGetPhysicalDeviceImageFormatProperties2KHR");

  auto toStrFlags = [](VkExternalMemoryHandleTypeFlags flags) {
    std::ostringstream oss;
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT "; }
    if (flags & VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT) { oss << "VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT "; }
    return oss.str();
  };

  for (auto handleType : handleList) {
    physicalDeviceExternalImageFormatInfo.handleType = handleType;
    imageFormatInfo.format = m_format;
    imageFormatInfo.type = VK_IMAGE_TYPE_2D;
    imageFormatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageFormatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageFormatInfo.pNext = &physicalDeviceExternalImageFormatInfo;
    imageFormatProperties.pNext = &externalImageFormatProp;
    
    auto res = _vkGetPhysicalDeviceImageFormatProperties2KHR(
      m_appBase->GetPhysicalDevice(),
      &imageFormatInfo, &imageFormatProperties);

    auto prop = externalImageFormatProp.externalMemoryProperties;
    std::ostringstream oss;
    oss << "externalMemoryFeatures: " << std::hex << prop.externalMemoryFeatures;
    oss << "[ ";
    if (prop.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) {
      oss << "VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT ";
    }
    if (prop.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT) {
      oss << "VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT ";
    }
    if (prop.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) {
      oss << "VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT ";
    }
    oss << "] \n";
   
    oss << "exportFromImportedHandleTypes: " << std::hex << prop.exportFromImportedHandleTypes;
    oss << "[ " << toStrFlags(prop.exportFromImportedHandleTypes) << "]\n";

    oss << "compatibleHandleTypes:" << std::hex << prop.compatibleHandleTypes;
    oss << "[ " << toStrFlags(prop.compatibleHandleTypes) << "]\n";
    oss << std::endl;
    OutputDebugStringA(oss.str().c_str());

  }
}

void MoviePlayer::Terminate()
{
  Stop();
  if (m_appBase == nullptr) {
    return;
  }

  if (m_mediaEngine) {
    m_mediaEngine->Shutdown();
  }
  m_mediaEngine.Reset();

  m_d3d11Device.Reset();
  if (m_hSharedTexHandle) {
    CloseHandle(m_hSharedTexHandle);
    m_hSharedTexHandle = 0;
  }
  if (m_hPrepare) {
    CloseHandle(m_hPrepare);
    m_hPrepare = 0;
  }

  m_appBase->DestroyImage(m_videoTexture);
  m_appBase = nullptr;
}

void MoviePlayer::SetMediaSource(const std::filesystem::path& fileName)
{
  BSTR bstrURL = nullptr;
  if (bstrURL != nullptr) {
    ::CoTaskMemFree(bstrURL);
    bstrURL = nullptr;
  }
  auto fullPath = std::filesystem::absolute(fileName).wstring();

  size_t cchAllocationSize = 1 + fullPath.size();
  bstrURL = reinterpret_cast<BSTR>(::CoTaskMemAlloc(sizeof(wchar_t) * (cchAllocationSize)));

  HRESULT hr;
  wcscpy_s(bstrURL, cchAllocationSize, fullPath.c_str());
  m_isPlaying = false;
  m_isFinished = false;
  hr = m_mediaEngine->SetSource(bstrURL);

  WaitForSingleObject(m_hPrepare, INFINITE);

  m_mediaEngine->GetNativeVideoSize(&m_width, &m_height);

  D3D11_TEXTURE2D_DESC texDesc{};
  texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  texDesc.Width = m_width;
  texDesc.Height = m_height;
  texDesc.MipLevels = 1;
  texDesc.ArraySize = 1;
  texDesc.SampleDesc.Count = 1;
  texDesc.Usage = D3D11_USAGE_DEFAULT;
  texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  hr = m_d3d11Device->CreateTexture2D(
    &texDesc, nullptr, &m_movieTextureD3D11
  );
  if (FAILED(hr)) {
    throw std::runtime_error("Failed CreateTexture2D(for MovieTexture)");
  }

  IDXGIResource1* tempResource = nullptr;
  hr = m_movieTextureD3D11->QueryInterface(__uuidof(IDXGIResource1), (void**)& tempResource);
  if (tempResource) {
    hr = tempResource->CreateSharedHandle(NULL, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &m_hSharedTexHandle);
    if (FAILED(hr)) {
      throw std::runtime_error("Failed CreateSharedHandle");
    }
    m_movieTextureD3D11.As(&m_keyedMutex);
    tempResource->Release();
    tempResource = nullptr;
  }



  VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo{};
  externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
  externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR;
  //externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;// for intel
  //externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;// for AMD

  auto vkdevice = m_appBase->GetDevice();
  VkImageCreateInfo imageCI{};
  imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCI.imageType = VK_IMAGE_TYPE_2D;
  imageCI.format = m_format;
  imageCI.extent.width = m_width;
  imageCI.extent.height = m_height;
  imageCI.extent.depth = 1;
  imageCI.mipLevels = 1;
  imageCI.arrayLayers = 1;
  imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCI.pNext = &externalMemoryImageCreateInfo;

  vkCreateImage(vkdevice, &imageCI, nullptr, &m_videoTexture.image);

  VkMemoryRequirements memReqs{};
  vkGetImageMemoryRequirements(vkdevice, m_videoTexture.image, &memReqs);
  auto memoryTypeIndex = m_appBase->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkMemoryAllocateInfo memAllocInfo{};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

  VkImportMemoryWin32HandleInfoKHR importMemInfo{};
  importMemInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
  importMemInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR;
  importMemInfo.handle = m_hSharedTexHandle;

  VkMemoryDedicatedRequirements dedicatedRequirements = {
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr,
  };
  VkMemoryRequirements2 memoryReqruirements = {
    VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
    &dedicatedRequirements,
  };
  const VkImageMemoryRequirementsInfo2 imageRequirementsInfo = {
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
    NULL,
    m_videoTexture.image,
  };
  vkGetImageMemoryRequirements2(vkdevice, &imageRequirementsInfo, &memoryReqruirements);
  memReqs = memoryReqruirements.memoryRequirements;

  VkMemoryDedicatedAllocateInfo dedicatedInfo = {
    VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
    &importMemInfo, m_videoTexture.image,
    0,
  };
  //memAllocInfo.pNext = &importMemInfo;
  memAllocInfo.pNext = &dedicatedInfo;
  memAllocInfo.allocationSize = memReqs.size;
  vkAllocateMemory(vkdevice, &memAllocInfo, 0, &m_videoTexture.memory);
  vkBindImageMemory(vkdevice, m_videoTexture.image, m_videoTexture.memory, 0);

  VkImageViewCreateInfo viewCreateInfo{};
  viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
  viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
  viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewCreateInfo.subresourceRange.baseMipLevel = 0;
  viewCreateInfo.subresourceRange.baseArrayLayer = 0;
  viewCreateInfo.subresourceRange.layerCount = 1;
  viewCreateInfo.subresourceRange.levelCount = 1;
  viewCreateInfo.image = m_videoTexture.image;
  vkCreateImageView(vkdevice, &viewCreateInfo, nullptr, &m_videoTexture.view);

  m_videoTexture.width = m_width;
  m_videoTexture.height = m_height;
  m_videoTexture.format = m_format;

  VkImageMemoryBarrier imb{
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
    0, VK_ACCESS_SHADER_READ_BIT,
    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
    m_videoTexture.image,
    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
  };
  auto command = m_appBase->CreateCommandBuffer();
  vkCmdPipelineBarrier(command,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0,
    0, nullptr,
    0, nullptr,
    1, &imb
    );
  m_appBase->FinishCommandBuffer(command);
  m_appBase->DestroyCommandBuffer(command);
}

void MoviePlayer::Play()
{
  if (m_isPlaying) {
    return;
  }
  if (m_mediaEngine) {
    m_mediaEngine->Play();
    m_isPlaying = true;
  }
}

void MoviePlayer::Stop()
{
  if (!m_isPlaying) {
    return;
  }

  if (m_mediaEngine) {
    m_mediaEngine->Pause();
    m_mediaEngine->SetCurrentTime(0.0);
  }

  m_isPlaying = false;
  m_isFinished = false;
}

void MoviePlayer::OnMediaEngineEvent(uint32_t meEvent)
{
  switch (meEvent)
  {
  case MF_MEDIA_ENGINE_EVENT_LOADSTART:
    OutputDebugStringA("MF_MEDIA_ENGINE_EVENT_LOADSTART\n");
    break;
  case MF_MEDIA_ENGINE_EVENT_PROGRESS:
    OutputDebugStringA("MF_MEDIA_ENGINE_EVENT_PROGRESS\n");
    break;
  case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
    SetEvent(m_hPrepare);
    break;
  case MF_MEDIA_ENGINE_EVENT_PLAY:
    OutputDebugStringA("MF_MEDIA_ENGINE_EVENT_PLAY\n");
    break;

  case MF_MEDIA_ENGINE_EVENT_CANPLAY:
    break;

  case MF_MEDIA_ENGINE_EVENT_WAITING:
    OutputDebugStringA("MF_MEDIA_ENGINE_EVENT_WAITING\n");
    break;

  case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
    break;
  case MF_MEDIA_ENGINE_EVENT_ENDED:
    OutputDebugStringA("MF_MEDIA_ENGINE_EVENT_ENDED\n");
    if (m_mediaEngine) {
      m_mediaEngine->Pause();
      m_isPlaying = false;
      m_isFinished = true;
    }
    break;

  case MF_MEDIA_ENGINE_EVENT_ERROR:
    if (m_mediaEngine) {
      ComPtr<IMFMediaError> err;
      if (SUCCEEDED(m_mediaEngine->GetError(&err))) {
        USHORT errCode = err->GetErrorCode();
        HRESULT hr = err->GetExtendedErrorCode();
        char buf[256] = { 0 };
        sprintf_s(buf, "ERROR: Media Foundation Event Error %u (%08X)\n", errCode, static_cast<unsigned int>(hr));
        OutputDebugStringA(buf);
      } else
      {
        OutputDebugStringA("ERROR: Media Foundation Event Error *FAILED GetError*\n");
      }
    }
    break;
  }
}

bool MoviePlayer::TransferFrame()
{
  if (m_mediaEngine && m_isPlaying) {
    LONGLONG pts;
    HRESULT hr = m_mediaEngine->OnVideoStreamTick(&pts);
    if (SUCCEEDED(hr)) {
      m_keyedMutex->AcquireSync(0, INFINITE);

      MFVideoNormalizedRect rect{ 0.0f, 0.0f, 1.0f, 1.0f };
      RECT rcTarget{ 0, 0, LONG(m_width), LONG(m_height) };

      MFARGB  m_bkgColor{ 0xFF, 0xFF, 0xFF, 0xFF };
      hr = m_mediaEngine->TransferVideoFrame(
        m_movieTextureD3D11.Get(), &rect, &rcTarget, &m_bkgColor);
      m_keyedMutex->ReleaseSync(0);

      if (SUCCEEDED(hr)) {
        return true;
      }
    }
  }
  return false;
}

void MoviePlayer::SetLoop(bool enable)
{
  if (m_mediaEngine) {
    m_mediaEngine->SetLoop(enable);
  }
}

VkImageView MoviePlayer::GetMovieTexture()
{
  return m_videoTexture.view;
}
