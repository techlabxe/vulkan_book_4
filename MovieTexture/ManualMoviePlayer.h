#pragma once

#include <vector>
#include <list>
#include <filesystem>

#include <wrl.h>

// for MediaFoundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#undef min
#undef max

#include "VulkanAppBase.h"


class ManualMoviePlayer {
public:
  template<typename T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;
  using Path = std::filesystem::path;

  ManualMoviePlayer();
  ~ManualMoviePlayer();

  void Initialize(
    VulkanAppBase* appBase
    /*
    ComPtr<ID3D12Device> device12,
    std::shared_ptr<DescriptorManager> descManager
    */);
  void Terminate();

  void Update(VkCommandBuffer command);

  void SetMediaSource(const Path& fileName);


  void Play();
  void Stop();

  bool IsPlaying() const { return m_isPlaying; }
  bool IsFinished() const { return m_isFinished; }

  void SetLoop(bool enable);

  //DescriptorHandle GetMovieTexture();
  VkImageView GetMovieTexture();

  VkImageView GetMovieTextureChroma() { return m_chromaView; }
private:
  void DecodeFrame();
  void UpdateTexture(VkCommandBuffer command);
  void ResetPosition();
private:
  IMFSourceReader* m_mfSourceReader = nullptr;
  static const int DecodeBufferCount = 2;

  VkFormat m_format;
  bool m_isPlaying = false;
  bool m_isFinished = false;
  bool m_isDecodeFinished = false;
  bool m_isLoop = false;
  bool m_lastFrameDisplayed = false;

  struct MovieFrameInfo {
    double timestamp;
    int bufferIndex;
  };
  int m_writeBufferIndex = 0;

  std::list<MovieFrameInfo> m_decoded;
  VulkanAppBase::BufferObject m_frameDecoded[DecodeBufferCount];

  LARGE_INTEGER m_startedTime;
  VulkanAppBase::ImageObject m_movieTextureRes;
  
  UINT m_width = 0, m_height = 0;
  double m_fps = 0;
  UINT m_srcStride = 0;

  UINT64 m_rowPitchBytes = 0;

  VulkanAppBase* m_appBase = nullptr;
  VkSubresourceLayout m_plane0, m_plane1;

  VkImageView m_chromaView;
};
