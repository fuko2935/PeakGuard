#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cwchar>
#include <exception>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

std::atomic_bool g_stopRequested{false};
constexpr int kMaxWatchSeconds = 3600;

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring WidenAscii(const char* value) {
    std::wstring result;
    if (value == nullptr) {
        return result;
    }
    while (*value != '\0') {
        result.push_back(static_cast<unsigned char>(*value));
        ++value;
    }
    return result;
}

void WriteText(HANDLE handle, const std::wstring& text) {
    if (text.empty() || handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD mode = 0;
    if (GetFileType(handle) == FILE_TYPE_CHAR && GetConsoleMode(handle, &mode)) {
        size_t offset = 0;
        while (offset < text.size()) {
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(text.size() - offset, 32767));
            DWORD written = 0;
            if (!WriteConsoleW(handle, text.data() + offset, chunk, &written, nullptr) || written == 0) {
                break;
            }
            offset += written;
        }
        return;
    }

    std::string utf8 = WideToUtf8(text);
    size_t offset = 0;
    while (offset < utf8.size()) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(utf8.size() - offset, 32767));
        DWORD written = 0;
        if (!WriteFile(handle, utf8.data() + offset, chunk, &written, nullptr) || written == 0) {
            break;
        }
        offset += written;
    }
}

void WriteOut(const std::wstring& text) {
    WriteText(GetStdHandle(STD_OUTPUT_HANDLE), text);
}

void WriteErr(const std::wstring& text) {
    WriteText(GetStdHandle(STD_ERROR_HANDLE), text);
}

class AppError final : public std::exception {
public:
    explicit AppError(std::wstring message) : message_(std::move(message)), narrow_(WideToUtf8(message_)) {}

    const char* what() const noexcept override {
        return narrow_.c_str();
    }

    const std::wstring& wide() const noexcept {
        return message_;
    }

private:
    std::wstring message_;
    std::string narrow_;
};

std::wstring FormatHresult(HRESULT hr) {
    std::wstringstream stream;
    stream << L"0x" << std::hex << hr << std::dec;
    return stream.str();
}

std::string Narrow(const std::wstring& value) {
    std::string result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        result.push_back(ch >= 0 && ch <= 0x7f ? static_cast<char>(ch) : '?');
    }
    return result;
}

[[noreturn]] void ThrowFailure(HRESULT hr, const wchar_t* operation) {
    throw AppError(std::wstring(operation) + L" failed hr=" + FormatHresult(hr));
}

void ThrowIfFailed(HRESULT hr, const wchar_t* operation) {
    if (FAILED(hr)) {
        ThrowFailure(hr, operation);
    }
}

class ComInitialization final {
public:
    ComInitialization() {
        ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED), L"CoInitializeEx");
    }

    ~ComInitialization() {
        CoUninitialize();
    }

    ComInitialization(const ComInitialization&) = delete;
    ComInitialization& operator=(const ComInitialization&) = delete;
};

BOOL WINAPI ConsoleControlHandler(DWORD controlType) {
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_stopRequested = true;
        return TRUE;
    default:
        return FALSE;
    }
}

class ConsoleControlRegistration final {
public:
    ConsoleControlRegistration() {
        if (!SetConsoleCtrlHandler(ConsoleControlHandler, TRUE)) {
            ThrowFailure(HRESULT_FROM_WIN32(GetLastError()), L"SetConsoleCtrlHandler");
        }
    }

    ~ConsoleControlRegistration() {
        SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);
    }

    ConsoleControlRegistration(const ConsoleControlRegistration&) = delete;
    ConsoleControlRegistration& operator=(const ConsoleControlRegistration&) = delete;
};

std::wstring GetDeviceString(IMMDevice* device, const PROPERTYKEY& key) {
    ComPtr<IPropertyStore> store;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr)) {
        return L"<property-store-error>";
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(key, &value);
    if (FAILED(hr)) {
        PropVariantClear(&value);
        return L"<property-error>";
    }

    std::wstring result = L"<empty>";
    if (value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        result = value.pwszVal;
    }
    PropVariantClear(&value);
    return result;
}

std::wstring GetDeviceId(IMMDevice* device) {
    LPWSTR id = nullptr;
    ThrowIfFailed(device->GetId(&id), L"IMMDevice::GetId");
    std::wstring result = id;
    CoTaskMemFree(id);
    return result;
}

const wchar_t* StateName(DWORD state) {
    switch (state) {
    case DEVICE_STATE_ACTIVE:
        return L"Active";
    case DEVICE_STATE_DISABLED:
        return L"Disabled";
    case DEVICE_STATE_NOTPRESENT:
        return L"NotPresent";
    case DEVICE_STATE_UNPLUGGED:
        return L"Unplugged";
    default:
        return L"Unknown";
    }
}

std::wstring FormatPropertyKey(const PROPERTYKEY& key) {
    LPOLESTR guid = nullptr;
    HRESULT hr = StringFromCLSID(key.fmtid, &guid);
    if (FAILED(hr)) {
        return L"<fmtid-error> pid=" + std::to_wstring(key.pid);
    }

    std::wstring result = guid;
    CoTaskMemFree(guid);
    result += L" pid=" + std::to_wstring(key.pid);
    return result;
}

void PrintDefault(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDevice> defaultDevice;
    HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    if (FAILED(hr)) {
        WriteOut(L"Default render endpoint: <none> hr=" + FormatHresult(hr) + L"\n");
        return;
    }

    WriteOut(L"Default render endpoint:\n");
    WriteOut(L"  Name: " + GetDeviceString(defaultDevice.Get(), PKEY_Device_FriendlyName) + L"\n");
    WriteOut(L"  ID:   " + GetDeviceId(defaultDevice.Get()) + L"\n");
}

void PrintEndpoints(IMMDeviceEnumerator* enumerator) {
    ComPtr<IMMDeviceCollection> collection;
    ThrowIfFailed(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_NOTPRESENT | DEVICE_STATE_UNPLUGGED, &collection),
        L"IMMDeviceEnumerator::EnumAudioEndpoints");

    UINT count = 0;
    ThrowIfFailed(collection->GetCount(&count), L"IMMDeviceCollection::GetCount");
    WriteOut(L"Render endpoint count: " + std::to_wstring(count) + L"\n");

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        ThrowIfFailed(collection->Item(i, &device), L"IMMDeviceCollection::Item");

        DWORD state = 0;
        ThrowIfFailed(device->GetState(&state), L"IMMDevice::GetState");

        WriteOut(L"[" + std::to_wstring(i) + L"] " + GetDeviceString(device.Get(), PKEY_Device_FriendlyName) + L"\n");
        WriteOut(L"    State: " + std::wstring(StateName(state)) + L"\n");
        WriteOut(L"    ID:    " + GetDeviceId(device.Get()) + L"\n");
    }
}

class NotificationClient final : public IMMNotificationClient {
public:
    explicit NotificationClient(IMMDeviceEnumerator* enumerator) : refCount_(1), enumerator_(enumerator) {}

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR deviceId) override {
        if (flow == eRender && role == eConsole) {
            WriteOut(L"\nDefault render endpoint changed: " + std::wstring(deviceId ? deviceId : L"<null>") + L"\n");
            PrintDefault(enumerator_.Get());
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override {
        WriteOut(L"\nDevice added: " + std::wstring(deviceId ? deviceId : L"<null>") + L"\n");
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override {
        WriteOut(L"\nDevice removed: " + std::wstring(deviceId ? deviceId : L"<null>") + L"\n");
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override {
        WriteOut(L"\nDevice state changed: " + std::wstring(deviceId ? deviceId : L"<null>")
            + L" -> " + StateName(newState) + L"\n");
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR deviceId, const PROPERTYKEY key) override {
        WriteOut(L"\nProperty changed: " + std::wstring(deviceId ? deviceId : L"<null>")
            + L" key=" + FormatPropertyKey(key) + L"\n");
        return S_OK;
    }

private:
    ~NotificationClient() = default;
    volatile LONG refCount_;
    ComPtr<IMMDeviceEnumerator> enumerator_;
};

class EndpointNotificationRegistration final {
public:
    EndpointNotificationRegistration(IMMDeviceEnumerator* enumerator, IMMNotificationClient* client)
        : enumerator_(enumerator), client_(client) {
        ThrowIfFailed(enumerator_->RegisterEndpointNotificationCallback(client_.Get()),
            L"RegisterEndpointNotificationCallback");
    }

    ~EndpointNotificationRegistration() {
        if (enumerator_ && client_) {
            HRESULT hr = enumerator_->UnregisterEndpointNotificationCallback(client_.Get());
            if (FAILED(hr)) {
                WriteErr(L"WARNING: UnregisterEndpointNotificationCallback failed hr=" + FormatHresult(hr) + L"\n");
            }
        }
    }

    EndpointNotificationRegistration(const EndpointNotificationRegistration&) = delete;
    EndpointNotificationRegistration& operator=(const EndpointNotificationRegistration&) = delete;

private:
    ComPtr<IMMDeviceEnumerator> enumerator_;
    ComPtr<IMMNotificationClient> client_;
};

struct Options {
    bool showHelp = false;
    bool watch = false;
    int watchSeconds = 0;
    int renderToneSeconds = 0;
};

void PrintUsage() {
    WriteOut(
        L"Usage: EndpointProbe [--watch | --watch-seconds N | --render-tone-seconds N]\n"
        L"\n"
        L"Options:\n"
        L"  --watch            Watch endpoint changes until Ctrl+C.\n"
        L"  --watch-seconds N  Watch endpoint changes for N seconds, then exit. Max: 3600.\n"
        L"  --render-tone-seconds N\n"
        L"                     Render a low-volume 440 Hz test tone to the default endpoint.\n"
        L"  --help             Show this help.\n");
}

bool ParsePositiveInt(const wchar_t* text, int* value) {
    wchar_t* end = nullptr;
    long parsed = std::wcstol(text, &end, 10);
    if (text == end || *end != L'\0' || parsed <= 0 || parsed > INT_MAX) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

Options ParseArgs(int argc, wchar_t** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--help" || arg == L"-h") {
            options.showHelp = true;
            continue;
        }
        if (arg == L"--watch") {
            if (options.watch || options.watchSeconds > 0) {
                throw AppError(L"Use only one watch mode.");
            }
            options.watch = true;
            continue;
        }
        if (arg == L"--watch-seconds") {
            if (options.watch || options.watchSeconds > 0 || options.renderToneSeconds > 0) {
                throw AppError(L"Use only one watch mode.");
            }
            if (i + 1 >= argc || !ParsePositiveInt(argv[i + 1], &options.watchSeconds)) {
                throw AppError(L"--watch-seconds requires a positive integer.");
            }
            if (options.watchSeconds > kMaxWatchSeconds) {
                throw AppError(L"--watch-seconds must be 3600 or less.");
            }
            ++i;
            continue;
        }
        if (arg == L"--render-tone-seconds") {
            if (options.watch || options.watchSeconds > 0 || options.renderToneSeconds > 0) {
                throw AppError(L"Use only one active mode.");
            }
            if (i + 1 >= argc || !ParsePositiveInt(argv[i + 1], &options.renderToneSeconds)) {
                throw AppError(L"--render-tone-seconds requires a positive integer.");
            }
            if (options.renderToneSeconds > 30) {
                throw AppError(L"--render-tone-seconds must be 30 or less.");
            }
            ++i;
            continue;
        }

        throw AppError(L"Unknown argument: " + arg);
    }
    return options;
}

void WatchEndpointChanges(IMMDeviceEnumerator* enumerator, int watchSeconds) {
    g_stopRequested = false;

    ComPtr<IMMNotificationClient> client;
    client.Attach(new NotificationClient(enumerator));
    EndpointNotificationRegistration registration(enumerator, client.Get());
    ConsoleControlRegistration consoleControl;

    if (watchSeconds > 0) {
        WriteOut(L"\nWatching endpoint changes for " + std::to_wstring(watchSeconds) + L" seconds.\n");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(watchSeconds);
        while (!g_stopRequested && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        WriteOut(L"\nWatching endpoint changes. Press Ctrl+C to exit.\n");
        while (!g_stopRequested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    WriteOut(L"Stopping endpoint watch.\n");
}

bool IsFloatFormat(const WAVEFORMATEX* format) {
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }
    return false;
}

bool IsPcmFormat(const WAVEFORMATEX* format) {
    if (format->wFormatTag == WAVE_FORMAT_PCM) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM);
    }
    return false;
}

void WriteToneFrames(BYTE* buffer, UINT32 frames, const WAVEFORMATEX* format, double* phase) {
    constexpr double kTwoPi = 6.28318530717958647692;
    constexpr double kFrequency = 440.0;
    constexpr double kAmplitude = 0.04;

    const UINT32 channels = format->nChannels;
    const UINT32 bytesPerSample = format->wBitsPerSample / 8;
    const double phaseStep = kTwoPi * kFrequency / static_cast<double>(format->nSamplesPerSec);
    const bool isFloat = IsFloatFormat(format);
    const bool isPcm = IsPcmFormat(format);

    for (UINT32 frame = 0; frame < frames; ++frame) {
        double sample = std::sin(*phase) * kAmplitude;
        *phase += phaseStep;
        if (*phase >= kTwoPi) {
            *phase -= kTwoPi;
        }

        for (UINT32 channel = 0; channel < channels; ++channel) {
            BYTE* target = buffer + (frame * format->nBlockAlign) + (channel * bytesPerSample);
            if (isFloat && bytesPerSample == sizeof(float)) {
                *reinterpret_cast<float*>(target) = static_cast<float>(sample);
            } else if (isPcm && bytesPerSample == sizeof(int16_t)) {
                *reinterpret_cast<int16_t*>(target) = static_cast<int16_t>(sample * 32767.0);
            } else if (isPcm && bytesPerSample == sizeof(int32_t)) {
                *reinterpret_cast<int32_t*>(target) = static_cast<int32_t>(sample * 2147483647.0);
            } else {
                ZeroMemory(target, bytesPerSample);
            }
        }
    }
}

void RenderTone(IMMDeviceEnumerator* enumerator, int seconds) {
    ComPtr<IMMDevice> device;
    ThrowIfFailed(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device),
        L"GetDefaultAudioEndpoint");

    WriteOut(L"\nRendering test tone to: " + GetDeviceString(device.Get(), PKEY_Device_FriendlyName) + L"\n");

    ComPtr<IAudioClient> audioClient;
    ThrowIfFailed(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient),
        L"IMMDevice::Activate(IAudioClient)");

    WAVEFORMATEX* mixFormatRaw = nullptr;
    ThrowIfFailed(audioClient->GetMixFormat(&mixFormatRaw), L"IAudioClient::GetMixFormat");
    std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mixFormat(mixFormatRaw, CoTaskMemFree);
    WriteOut(L"Mix format: tag=" + std::to_wstring(mixFormat->wFormatTag) +
        L" channels=" + std::to_wstring(mixFormat->nChannels) +
        L" sampleRate=" + std::to_wstring(mixFormat->nSamplesPerSec) +
        L" bits=" + std::to_wstring(mixFormat->wBitsPerSample) +
        L" blockAlign=" + std::to_wstring(mixFormat->nBlockAlign) +
        L" avgBytes=" + std::to_wstring(mixFormat->nAvgBytesPerSec) +
        L" cbSize=" + std::to_wstring(mixFormat->cbSize) + L"\n");
    WAVEFORMATEX* closestRaw = nullptr;
    HRESULT formatHr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, mixFormat.get(), &closestRaw);
    std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> closest(closestRaw, CoTaskMemFree);
    WriteOut(L"IsFormatSupported(shared): hr=0x" + std::to_wstring(static_cast<unsigned long>(formatHr)) + L"\n");

    constexpr REFERENCE_TIME kBufferDuration = 10000000; // 1 second
    ThrowIfFailed(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, kBufferDuration, 0, mixFormat.get(), nullptr),
        L"IAudioClient::Initialize");

    UINT32 bufferFrameCount = 0;
    ThrowIfFailed(audioClient->GetBufferSize(&bufferFrameCount), L"IAudioClient::GetBufferSize");

    ComPtr<IAudioRenderClient> renderClient;
    ThrowIfFailed(audioClient->GetService(IID_PPV_ARGS(&renderClient)), L"IAudioClient::GetService(IAudioRenderClient)");

    double phase = 0.0;
    BYTE* data = nullptr;
    ThrowIfFailed(renderClient->GetBuffer(bufferFrameCount, &data), L"IAudioRenderClient::GetBuffer");
    WriteToneFrames(data, bufferFrameCount, mixFormat.get(), &phase);
    ThrowIfFailed(renderClient->ReleaseBuffer(bufferFrameCount, 0), L"IAudioRenderClient::ReleaseBuffer");

    ThrowIfFailed(audioClient->Start(), L"IAudioClient::Start");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        UINT32 padding = 0;
        ThrowIfFailed(audioClient->GetCurrentPadding(&padding), L"IAudioClient::GetCurrentPadding");
        UINT32 available = bufferFrameCount - padding;
        if (available > 0) {
            ThrowIfFailed(renderClient->GetBuffer(available, &data), L"IAudioRenderClient::GetBuffer");
            WriteToneFrames(data, available, mixFormat.get(), &phase);
            ThrowIfFailed(renderClient->ReleaseBuffer(available, 0), L"IAudioRenderClient::ReleaseBuffer");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    audioClient->Stop();
    WriteOut(L"Rendered test tone for " + std::to_wstring(seconds) + L" seconds.\n");
}

int Run(int argc, wchar_t** argv) {
    Options options = ParseArgs(argc, argv);
    if (options.showHelp) {
        PrintUsage();
        return 0;
    }

    ComInitialization com;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ThrowIfFailed(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)),
        L"CoCreateInstance(MMDeviceEnumerator)");

    PrintDefault(enumerator.Get());
    PrintEndpoints(enumerator.Get());

    if (options.watch || options.watchSeconds > 0) {
        WatchEndpointChanges(enumerator.Get(), options.watchSeconds);
    }
    if (options.renderToneSeconds > 0) {
        RenderTone(enumerator.Get(), options.renderToneSeconds);
    }

    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        return Run(argc, argv);
    } catch (const AppError& ex) {
        WriteErr(L"ERROR: " + ex.wide() + L"\n");
        PrintUsage();
        return 1;
    } catch (const std::exception& ex) {
        WriteErr(L"ERROR: " + WidenAscii(ex.what()) + L"\n");
        PrintUsage();
        return 1;
    }
}
