#pragma once

#include <filesystem>

#include <wrl.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <cstdint>

// for MediaFoundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>

#undef max
#undef min
#include "VulkanAppBase.h"

class MoviePlayer {
public:
  template<typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  MoviePlayer();
  ~MoviePlayer();

  void Initialize(ComPtr<IDXGIAdapter1> adapter, VulkanAppBase* appBase);
  void Terminate();

  void SetMediaSource(const std::filesystem::path& fileName);

  void Play();
  void Stop();

  void OnMediaEngineEvent(uint32_t meEvent);

  bool TransferFrame();

  bool IsPlaying() const { return m_isPlaying; }
  bool IsFinished() const { return m_isFinished; }

  void SetLoop(bool enable);
  VkImageView GetMovieTexture();

  //DescriptorHandle GetMovieTextureLum() { return m_srvLuminance; }
  //DescriptorHandle GetMovieTextureChrom() { return m_srvChrominance; }
private:
  ComPtr<ID3D11Device1> m_d3d11Device;
  ComPtr<IMFMediaEngine> m_mediaEngine;
  ComPtr<ID3D11Texture2D> m_movieTextureD3D11;

  long m_cRef;
  DWORD m_width = 0, m_height = 0;
  VkFormat m_format = VK_FORMAT_B8G8R8A8_UNORM;
  bool m_isPlaying = false;
  bool m_isFinished = false;

  VulkanAppBase::ImageObject m_videoTexture;

  //ComPtr<ID3D12Resource> m_videoTexture;
  //DescriptorHandle m_srvVideoTexture;
  //DescriptorHandle m_srvLuminance, m_srvChrominance;

  HANDLE m_hSharedTexHandle;
  ComPtr<IDXGIKeyedMutex> m_keyedMutex;

  HANDLE m_hPrepare;

  VulkanAppBase* m_appBase = nullptr;
};
