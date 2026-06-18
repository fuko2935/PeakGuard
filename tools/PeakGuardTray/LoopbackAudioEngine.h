#pragma once

#include <windows.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <wrl/client.h>

#include <atomic>
#include <string>

#include "common/PeakGuardSharedState.h"

namespace PeakGuard {

class LoopbackAudioEngine {
public:
    LoopbackAudioEngine();
    ~LoopbackAudioEngine();

    LoopbackAudioEngine(const LoopbackAudioEngine&) = delete;
    LoopbackAudioEngine& operator=(const LoopbackAudioEngine&) = delete;

    bool Start(PeakGuardSharedState* state);
    void Stop();
    bool IsRunning() const;

    void OnDefaultDeviceChanged(LPCWSTR deviceId = nullptr);

    AudioEngineState GetState() const;
    std::wstring GetDeviceName() const;
    UINT32 GetSampleRate() const;

private:
    bool InitializeAudioClients();
    void ShutdownAudioClients();

    void AudioThreadProc();
    void ProcessCaptureBuffer();
    void SyncEndpointVolume(bool force = false);

    void ProcessAudio(float* frames, UINT32 frameCount, UINT32 channels);
    void SetSharedState(AudioEngineState newState);

    PeakGuardSharedState* sharedState_ = nullptr;
    std::atomic<AudioEngineState> state_{AudioEngineState::Stopped};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> deviceChanged_{false};

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> captureClient_;
    Microsoft::WRL::ComPtr<IAudioClient> renderClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture_;
    Microsoft::WRL::ComPtr<IAudioRenderClient> render_;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> virtualRenderVolume_;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> physicalRenderVolume_;

    HANDLE stopEvent_ = nullptr;
    HANDLE deviceChangedEvent_ = nullptr;
    HANDLE captureReadyEvent_ = nullptr;
    HANDLE renderReadyEvent_ = nullptr;

    HANDLE audioThread_ = nullptr;
    HANDLE avrtHandle_ = nullptr;
    DWORD avrtTaskIndex_ = 0;

    WAVEFORMATEX* captureFormat_ = nullptr;
    UINT32 captureBufferFrames_ = 0;
    UINT32 renderBufferFrames_ = 0;
    bool captureFormatIsFloat_ = false;

    float limiterGain_ = 1.0f;

    UINT32 consecutiveSilentBuffers_ = 0;
    DWORD lastVolumeSyncTick_ = 0;
    float lastSyncedVolume_ = -1.0f;
    BOOL lastSyncedMute_ = FALSE;
    static constexpr UINT32 kSilenceThreshold = 50;
    static constexpr float kSilencePeakThreshold = 0.0001f;

    std::wstring deviceName_;
    std::wstring preferredRenderDeviceId_;
    CRITICAL_SECTION deviceNameLock_;
    UINT32 sampleRate_ = 0;
};

} // namespace PeakGuard
