#include "LoopbackAudioEngine.h"

#include "AudioEndpointSelection.h"
#include "AudioPowerPolicy.h"

#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <algorithm>
#include <cmath>
#include <xmmintrin.h>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "oleaut32.lib")

namespace CodexLimiter {

namespace {

constexpr REFERENCE_TIME kBufferDuration = 1000000; // 100ms stability buffer

struct PROPERTYKEY;

MIDL_INTERFACE("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(LPCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(LPCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(LPCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(LPCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(LPCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(LPCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(LPCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(LPCWSTR, INT) = 0;
};

const CLSID CLSID_PolicyConfigClient = {
    0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}
};

std::wstring GetDeviceFriendlyName(IMMDevice* device) {
    if (device == nullptr) return L"<unknown>";

    Microsoft::WRL::ComPtr<IPropertyStore> store;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr)) return L"<property error>";

    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(PKEY_Device_FriendlyName, &value);
    if (SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        std::wstring name = value.pwszVal;
        PropVariantClear(&value);
        return name;
    }
    PropVariantClear(&value);
    return L"<unnamed>";
}

bool FindDefaultDevice(
    IMMDeviceEnumerator* enumerator,
    EDataFlow flow,
    Microsoft::WRL::ComPtr<IMMDevice>* device) {
    if (enumerator == nullptr || device == nullptr) return false;
    return SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, device->GetAddressOf()));
}

std::wstring GetDeviceId(IMMDevice* device) {
    if (device == nullptr) return {};
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || id == nullptr) return {};
    std::wstring result = id;
    CoTaskMemFree(id);
    return result;
}

bool FindDeviceById(
    IMMDeviceEnumerator* enumerator,
    const std::wstring& deviceId,
    Microsoft::WRL::ComPtr<IMMDevice>* selected,
    std::wstring* selectedName) {
    if (enumerator == nullptr || selected == nullptr || deviceId.empty()) return false;

    Microsoft::WRL::ComPtr<IMMDevice> device;
    HRESULT hr = enumerator->GetDevice(deviceId.c_str(), &device);
    if (FAILED(hr)) return false;

    DWORD state = 0;
    if (FAILED(device->GetState(&state)) || state != DEVICE_STATE_ACTIVE) return false;

    std::wstring name = GetDeviceFriendlyName(device.Get());
    if (IsVirtualAudioEndpointName(name)) return false;

    *selected = device;
    if (selectedName != nullptr) {
        *selectedName = name;
    }
    return true;
}

bool FindVirtualCaptureDevice(
    IMMDeviceEnumerator* enumerator,
    Microsoft::WRL::ComPtr<IMMDevice>* selected,
    std::wstring* selectedName) {
    Microsoft::WRL::ComPtr<IMMDevice> defaultCapture;
    if (FindDefaultDevice(enumerator, eCapture, &defaultCapture)) {
        std::wstring name = GetDeviceFriendlyName(defaultCapture.Get());
        if (IsVirtualAudioEndpointName(name)) {
            *selected = defaultCapture;
            *selectedName = name;
            return true;
        }
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return false;

    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return false;

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) continue;

        std::wstring name = GetDeviceFriendlyName(device.Get());
        if (IsVirtualAudioEndpointName(name)) {
            *selected = device;
            *selectedName = name;
            return true;
        }
    }

    return false;
}

bool FindVirtualRenderDevice(
    IMMDeviceEnumerator* enumerator,
    Microsoft::WRL::ComPtr<IMMDevice>* selected,
    std::wstring* selectedName,
    std::wstring* selectedId) {
    Microsoft::WRL::ComPtr<IMMDevice> defaultRender;
    if (FindDefaultDevice(enumerator, eRender, &defaultRender)) {
        std::wstring name = GetDeviceFriendlyName(defaultRender.Get());
        if (IsVirtualAudioEndpointName(name)) {
            *selected = defaultRender;
            if (selectedName != nullptr) *selectedName = name;
            if (selectedId != nullptr) *selectedId = GetDeviceId(defaultRender.Get());
            return true;
        }
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return false;

    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return false;

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) continue;

        std::wstring name = GetDeviceFriendlyName(device.Get());
        if (IsVirtualAudioEndpointName(name) && ContainsEndpointToken(LowerEndpointName(name), L"cable input")) {
            *selected = device;
            if (selectedName != nullptr) *selectedName = name;
            if (selectedId != nullptr) *selectedId = GetDeviceId(device.Get());
            return true;
        }
    }

    return false;
}

bool SetDefaultRenderEndpoint(const std::wstring& endpointId) {
    if (endpointId.empty()) return false;

    Microsoft::WRL::ComPtr<IPolicyConfig> policy;
    HRESULT hr = CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL,
        __uuidof(IPolicyConfig), reinterpret_cast<void**>(policy.GetAddressOf()));
    if (FAILED(hr)) return false;

    bool ok = true;
    ok = SUCCEEDED(policy->SetDefaultEndpoint(endpointId.c_str(), eConsole)) && ok;
    ok = SUCCEEDED(policy->SetDefaultEndpoint(endpointId.c_str(), eMultimedia)) && ok;
    return ok;
}

bool FindPhysicalRenderDevice(
    IMMDeviceEnumerator* enumerator,
    const std::wstring& preferredDeviceId,
    Microsoft::WRL::ComPtr<IMMDevice>* selected,
    std::wstring* selectedName) {
    if (FindDeviceById(enumerator, preferredDeviceId, selected, selectedName)) {
        return true;
    }

    Microsoft::WRL::ComPtr<IMMDevice> defaultRender;
    if (FindDefaultDevice(enumerator, eRender, &defaultRender)) {
        std::wstring name = GetDeviceFriendlyName(defaultRender.Get());
        if (!IsVirtualAudioEndpointName(name)) {
            *selected = defaultRender;
            *selectedName = name;
            return true;
        }
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
    HRESULT hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return false;

    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) return false;

    for (UINT i = 0; i < count; ++i) {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, &device))) continue;

        std::wstring name = GetDeviceFriendlyName(device.Get());
        if (!IsVirtualAudioEndpointName(name)) {
            *selected = device;
            *selectedName = name;
            return true;
        }
    }

    return false;
}

void SetDeviceName(CRITICAL_SECTION* lock, std::wstring* target, const std::wstring& value) {
    EnterCriticalSection(lock);
    *target = value;
    LeaveCriticalSection(lock);
}

} // namespace

void LoopbackAudioEngine::SetSharedState(AudioEngineState newState) {
    state_ = newState;
    if (sharedState_ != nullptr) {
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->audioEngineState),
            static_cast<LONG>(newState));
    }
}

LoopbackAudioEngine::LoopbackAudioEngine() {
    InitializeCriticalSection(&deviceNameLock_);
}

LoopbackAudioEngine::~LoopbackAudioEngine() {
    Stop();
    DeleteCriticalSection(&deviceNameLock_);
}

bool LoopbackAudioEngine::Start(LimiterSharedState* state) {
    if (state == nullptr) return false;
    if (audioThread_ != nullptr) return true;

    sharedState_ = state;
    stopRequested_ = false;
    deviceChanged_ = false;
    consecutiveSilentBuffers_ = 0;
    limiterGain_ = 1.0f;

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    deviceChangedEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    captureReadyEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    renderReadyEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (stopEvent_ == nullptr || deviceChangedEvent_ == nullptr ||
        captureReadyEvent_ == nullptr || renderReadyEvent_ == nullptr) {
        return false;
    }

    audioThread_ = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            static_cast<LoopbackAudioEngine*>(param)->AudioThreadProc();
            return 0;
        },
        this, 0, nullptr);

    if (audioThread_ == nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
        CloseHandle(deviceChangedEvent_);
        deviceChangedEvent_ = nullptr;
        CloseHandle(captureReadyEvent_);
        captureReadyEvent_ = nullptr;
        CloseHandle(renderReadyEvent_);
        renderReadyEvent_ = nullptr;
        return false;
    }

    for (int i = 0; i < 50; ++i) {
        if (state_.load() == AudioEngineState::Running ||
            state_.load() == AudioEngineState::Error) {
            break;
        }
        Sleep(10);
    }

    return state_.load() != AudioEngineState::Error;
}

void LoopbackAudioEngine::Stop() {
    if (audioThread_ == nullptr) return;

    stopRequested_ = true;
    if (stopEvent_ != nullptr) {
        SetEvent(stopEvent_);
    }

    const DWORD waitResult = WaitForSingleObject(audioThread_, 5000);
    CloseHandle(audioThread_);
    audioThread_ = nullptr;

    std::wstring endpointToRestore;
    EnterCriticalSection(&deviceNameLock_);
    endpointToRestore = preferredRenderDeviceId_;
    LeaveCriticalSection(&deviceNameLock_);
    if (ShouldRestoreRenderEndpoint(waitResult, endpointToRestore)) {
        SetDefaultRenderEndpoint(endpointToRestore);
    }

    if (stopEvent_ != nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    if (deviceChangedEvent_ != nullptr) {
        CloseHandle(deviceChangedEvent_);
        deviceChangedEvent_ = nullptr;
    }
    if (captureReadyEvent_ != nullptr) {
        CloseHandle(captureReadyEvent_);
        captureReadyEvent_ = nullptr;
    }
    if (renderReadyEvent_ != nullptr) {
        CloseHandle(renderReadyEvent_);
        renderReadyEvent_ = nullptr;
    }

    state_ = AudioEngineState::Stopped;
    if (sharedState_ != nullptr) {
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->audioEngineState),
            static_cast<LONG>(AudioEngineState::Stopped));
    }
}

bool LoopbackAudioEngine::IsRunning() const {
    return state_.load() == AudioEngineState::Running;
}

void LoopbackAudioEngine::OnDefaultDeviceChanged(LPCWSTR deviceId) {
    if (deviceId != nullptr && enumerator_ != nullptr) {
        Microsoft::WRL::ComPtr<IMMDevice> device;
        if (SUCCEEDED(enumerator_->GetDevice(deviceId, &device))) {
            std::wstring name = GetDeviceFriendlyName(device.Get());
            if (!IsVirtualAudioEndpointName(name)) {
                EnterCriticalSection(&deviceNameLock_);
                preferredRenderDeviceId_ = deviceId;
                LeaveCriticalSection(&deviceNameLock_);
            }
        }
    }
    deviceChanged_ = true;
    if (deviceChangedEvent_ != nullptr) {
        SetEvent(deviceChangedEvent_);
    }
}

AudioEngineState LoopbackAudioEngine::GetState() const {
    return state_.load();
}

std::wstring LoopbackAudioEngine::GetDeviceName() const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&deviceNameLock_));
    std::wstring name = deviceName_;
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&deviceNameLock_));
    return name;
}

UINT32 LoopbackAudioEngine::GetSampleRate() const {
    return sampleRate_;
}

bool LoopbackAudioEngine::InitializeAudioClients() {
    HRESULT hr;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&enumerator_));
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IMMDevice> virtualRenderDevice;
    std::wstring virtualRenderName;
    std::wstring virtualRenderId;
    if (!FindVirtualRenderDevice(enumerator_.Get(), &virtualRenderDevice, &virtualRenderName, &virtualRenderId)) {
        SetDeviceName(&deviceNameLock_, &deviceName_,
            L"Virtual audio output not found. Install/select VB-CABLE.");
        SetSharedState(AudioEngineState::Error);
        return false;
    }

    Microsoft::WRL::ComPtr<IMMDevice> defaultRender;
    if (FindDefaultDevice(enumerator_.Get(), eRender, &defaultRender)) {
        std::wstring defaultName = GetDeviceFriendlyName(defaultRender.Get());
        if (!IsVirtualAudioEndpointName(defaultName)) {
            EnterCriticalSection(&deviceNameLock_);
            preferredRenderDeviceId_ = GetDeviceId(defaultRender.Get());
            LeaveCriticalSection(&deviceNameLock_);
            SetDefaultRenderEndpoint(virtualRenderId);
        }
    }

    Microsoft::WRL::ComPtr<IMMDevice> renderDevice;
    std::wstring renderName;

    std::wstring preferredId;
    EnterCriticalSection(&deviceNameLock_);
    preferredId = preferredRenderDeviceId_;
    LeaveCriticalSection(&deviceNameLock_);

    if (!FindPhysicalRenderDevice(enumerator_.Get(), preferredId, &renderDevice, &renderName)) {
        SetDeviceName(&deviceNameLock_, &deviceName_,
            L"Physical audio output not found.");
        SetSharedState(AudioEngineState::Error);
        return false;
    }

    SetDeviceName(&deviceNameLock_, &deviceName_,
        L"Virtual loopback: " + virtualRenderName + L" -> Output: " + renderName);

    hr = virtualRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(captureClient_.GetAddressOf()));
    if (FAILED(hr)) return false;

    WAVEFORMATEX* mixFormat = nullptr;
    hr = captureClient_->GetMixFormat(&mixFormat);
    if (FAILED(hr)) return false;

    captureFormat_ = mixFormat;
    captureFormatIsFloat_ = IsFloatPcmFormat(mixFormat);
    sampleRate_ = mixFormat->nSamplesPerSec;

    hr = captureClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        CaptureStreamFlags(),
        kBufferDuration,
        0,
        mixFormat,
        nullptr);
    if (FAILED(hr)) {
        CoTaskMemFree(mixFormat);
        captureFormat_ = nullptr;
        return false;
    }

    captureClient_->GetBufferSize(&captureBufferFrames_);

    hr = captureClient_->SetEventHandle(captureReadyEvent_);
    if (FAILED(hr)) goto CleanupFormat;

    hr = captureClient_->GetService(IID_PPV_ARGS(&capture_));
    if (FAILED(hr)) goto CleanupFormat;

    hr = renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(renderClient_.GetAddressOf()));
    if (FAILED(hr)) goto CleanupFormat;

    WAVEFORMATEX* closestFormat = nullptr;
    hr = renderClient_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, mixFormat, &closestFormat);
    if (closestFormat != nullptr) {
        CoTaskMemFree(closestFormat);
    }
    if (hr != S_OK) {
        SetDeviceName(&deviceNameLock_, &deviceName_,
            L"Virtual capture format is not supported by selected output.");
        goto CleanupFormat;
    }

    hr = renderClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        RenderStreamFlags(),
        kBufferDuration,
        0,
        mixFormat,
        nullptr);
    if (FAILED(hr)) goto CleanupFormat;

    hr = renderClient_->SetEventHandle(renderReadyEvent_);
    if (FAILED(hr)) goto CleanupFormat;

    hr = renderClient_->GetService(IID_PPV_ARGS(&render_));
    if (FAILED(hr)) goto CleanupFormat;

    renderClient_->GetBufferSize(&renderBufferFrames_);

    goto Success;

CleanupFormat:
    CoTaskMemFree(mixFormat);
    return false;

Success:
    SetSharedState(AudioEngineState::Running);
    if (sharedState_ != nullptr) {
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->audioMode),
            static_cast<LONG>(AudioMode::Loopback));
        InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->sampleRate),
            static_cast<LONG>(sampleRate_));
    }

    // Start audio streaming
    captureClient_->Start();

    // Pre-fill render buffer with silence to prevent initial underrun
    {
        BYTE* renderData = nullptr;
        UINT32 padding = 0;
        hr = renderClient_->GetCurrentPadding(&padding);
        UINT32 preFill = renderBufferFrames_ > padding ? renderBufferFrames_ - padding : 0;
        if (preFill > 0 && render_ != nullptr && SUCCEEDED(render_->GetBuffer(preFill, &renderData))) {
            ZeroMemory(renderData, preFill * captureFormat_->nBlockAlign);
            render_->ReleaseBuffer(preFill, 0);
        }
    }

    renderClient_->Start();

    return true;
}

void LoopbackAudioEngine::ShutdownAudioClients() {
    if (captureClient_ != nullptr) {
        captureClient_->Stop();
    }
    if (renderClient_ != nullptr) {
        renderClient_->Stop();
    }

    capture_.Reset();
    render_.Reset();
    captureClient_.Reset();
    renderClient_.Reset();
    enumerator_.Reset();
    device_.Reset();

    if (captureFormat_ != nullptr) {
        CoTaskMemFree(captureFormat_);
        captureFormat_ = nullptr;
    }
    captureFormatIsFloat_ = false;
    captureBufferFrames_ = 0;
    renderBufferFrames_ = 0;
}

void LoopbackAudioEngine::AudioThreadProc() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    avrtHandle_ = AvSetMmThreadCharacteristicsW(L"Audio", &avrtTaskIndex_);
    if (avrtHandle_ != nullptr) {
        AvSetMmThreadPriority(avrtHandle_, AVRT_PRIORITY_HIGH);
    }

    // Enable flush-to-zero and denormals-are-zero for SSE floating-point processing.
    // This prevents denormal slow paths from impacting audio thread performance.
    {
        unsigned int mxcsr = _mm_getcsr();
        mxcsr |= (1 << 15); // FTZ: Flush To Zero
        mxcsr |= (1 << 6);  // DAZ: Denormals Are Zero
        _mm_setcsr(mxcsr);
    }

    if (!InitializeAudioClients()) {
        SetSharedState(AudioEngineState::Error);
        CoUninitialize();
        return;
    }

    if (stopRequested_.load()) {
        ShutdownAudioClients();
        SetSharedState(AudioEngineState::Stopped);
        CoUninitialize();
        return;
    }

    HANDLE waitHandles[3] = { stopEvent_, deviceChangedEvent_, captureReadyEvent_ };

    while (!stopRequested_.load()) {
        if (deviceChanged_.load()) {
            ShutdownAudioClients();
            deviceChanged_ = false;
            consecutiveSilentBuffers_ = 0;

            if (!InitializeAudioClients()) {
                SetSharedState(AudioEngineState::Error);
                break;
            }
            continue;
        }

        DWORD result = WaitForMultipleObjects(3, waitHandles, FALSE, AudioThreadWaitTimeoutMs());

        if (result == WAIT_OBJECT_0) {
            break;
        } else if (result == WAIT_OBJECT_0 + 1) {
            continue;
        } else if (result != WAIT_OBJECT_0 + 2) {
            continue;
        }

        ProcessCaptureBuffer();
    }

    ShutdownAudioClients();

    if (avrtHandle_ != nullptr) {
        AvRevertMmThreadCharacteristics(avrtHandle_);
        avrtHandle_ = nullptr;
    }

    SetSharedState(AudioEngineState::Stopped);

    CoUninitialize();
}

void LoopbackAudioEngine::ProcessCaptureBuffer() {
    if (capture_ == nullptr || render_ == nullptr || captureFormat_ == nullptr) return;

    UINT32 packetLength = 0;
    HRESULT hr = capture_->GetNextPacketSize(&packetLength);
    if (FAILED(hr)) return;

    while (packetLength > 0) {
        BYTE* data = nullptr;
        UINT32 framesAvailable = 0;
        DWORD flags = 0;
        UINT64 devicePosition = 0;
        UINT64 qpcPosition = 0;

        hr = capture_->GetBuffer(&data, &framesAvailable, &flags,
            &devicePosition, &qpcPosition);
        if (FAILED(hr) || framesAvailable == 0) return;

        bool isSilent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

        if (!isSilent && captureFormatIsFloat_) {
            const UINT32 channels = captureFormat_->nChannels;
            ApplyLimiter(reinterpret_cast<float*>(data), framesAvailable, channels);
        }

        UINT32 renderPadding = 0;
        hr = renderClient_->GetCurrentPadding(&renderPadding);
        if (FAILED(hr)) {
            capture_->ReleaseBuffer(framesAvailable);
            return;
        }

        UINT32 renderAvailable = renderBufferFrames_ > renderPadding ? renderBufferFrames_ - renderPadding : 0;
        if (renderAvailable < framesAvailable) {
            capture_->ReleaseBuffer(framesAvailable);
            return;
        }

        BYTE* renderData = nullptr;
        hr = render_->GetBuffer(framesAvailable, &renderData);
        if (SUCCEEDED(hr)) {
            if (isSilent) {
                ZeroMemory(renderData, framesAvailable * captureFormat_->nBlockAlign);
            } else {
                CopyMemory(renderData, data, framesAvailable * captureFormat_->nBlockAlign);
            }
            render_->ReleaseBuffer(framesAvailable, 0);
        }

        capture_->ReleaseBuffer(framesAvailable);

        if (isSilent) {
            consecutiveSilentBuffers_++;
            if (consecutiveSilentBuffers_ >= kSilenceThreshold &&
                state_.load() != AudioEngineState::Silence) {
                SetSharedState(AudioEngineState::Silence);
            }
        } else {
            consecutiveSilentBuffers_ = 0;
            if (state_.load() == AudioEngineState::Silence) {
                SetSharedState(AudioEngineState::Running);
            }
        }

        hr = capture_->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) return;
    }
}

void LoopbackAudioEngine::ApplyLimiter(float* frames, UINT32 frameCount, UINT32 channels) {
    const bool enabled = (sharedState_ == nullptr) || (sharedState_->enabled != 0);

    // Fast path: limiter disabled — skip DSP, just track output peak for meter
    if (!enabled) {
        limiterGain_ = 1.0f;

        float outputPeak = 0.0f;
        for (UINT32 frame = 0; frame < frameCount; ++frame) {
            const UINT32 offset = frame * channels;
            for (UINT32 ch = 0; ch < channels; ++ch) {
                const float value = fabsf(frames[offset + ch]);
                if (value > outputPeak) {
                    outputPeak = value;
                }
            }
        }

        if (sharedState_ != nullptr) {
            InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->outputPeakMilliDb),
                LinearToMilliDb(outputPeak));
            InterlockedIncrement(const_cast<volatile LONG*>(&sharedState_->processCounter));
        }
        return;
    }

    // Full limiter path
    constexpr float kReleasePerFrame = 0.00008f;
    float gain = limiterGain_;
    float inputPeak = 0.0f;
    float outputPeak = 0.0f;

    const float ceiling = (sharedState_ != nullptr)
        ? ScaledLinearToFloat(sharedState_->ceilingLinearScaled)
        : ScaledLinearToFloat(kDefaultLinearScaled);

    for (UINT32 frame = 0; frame < frameCount; ++frame) {
        float peak = 0.0f;
        const UINT32 offset = frame * channels;
        for (UINT32 ch = 0; ch < channels; ++ch) {
            const float value = fabsf(frames[offset + ch]);
            if (value > peak) {
                peak = value;
            }
        }
        if (peak > inputPeak) {
            inputPeak = peak;
        }

        const float targetGain = (peak > ceiling) ? (ceiling / peak) : 1.0f;
        if (targetGain < gain) {
            gain = targetGain;
        } else if (gain < 1.0f) {
            gain += kReleasePerFrame;
            if (gain > 1.0f) {
                gain = 1.0f;
            }
        }

        for (UINT32 ch = 0; ch < channels; ++ch) {
            frames[offset + ch] *= gain;
            const float processed = fabsf(frames[offset + ch]);
            if (processed > outputPeak) {
                outputPeak = processed;
            }
        }
    }

    limiterGain_ = gain;

    if (sharedState_ != nullptr) {
        if (inputPeak > 0.000001f) {
            InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->inputPeakMilliDb),
                LinearToMilliDb(inputPeak));
            InterlockedExchange(const_cast<volatile LONG*>(&sharedState_->outputPeakMilliDb),
                LinearToMilliDb(outputPeak));
        }
        InterlockedIncrement(const_cast<volatile LONG*>(&sharedState_->processCounter));
    }
}

} // namespace CodexLimiter
