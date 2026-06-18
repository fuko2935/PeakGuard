#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "common/LimiterSharedState.h"
#include "AudioPowerPolicy.h"
#include "LimiterSettings.h"
#include "LoopbackAudioEngine.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT_PTR kUiTimer = 1;
constexpr int kWindowWidth = 360;
constexpr int kWindowHeight = CodexLimiter::UiWindowHeightPx();

constexpr int kIdEnable = 1001;
constexpr int kIdSlider = 1002;
constexpr int kIdMeter = 1003;
constexpr int kIdPeakText = 1004;
constexpr int kIdCeilingText = 1005;
constexpr int kIdStatusText = 1006;
constexpr int kIdBoostEnable = 1007;
constexpr int kIdBoostSlider = 1008;
constexpr int kIdBoostText = 1009;
constexpr COLORREF kColorWindow = RGB(16, 18, 22);
constexpr COLORREF kColorPanel = RGB(27, 30, 36);
constexpr COLORREF kColorText = RGB(238, 241, 245);
constexpr COLORREF kColorMutedText = RGB(170, 178, 190);
constexpr COLORREF kColorRail = RGB(58, 63, 72);
constexpr COLORREF kColorAccent = RGB(245, 164, 18);
constexpr COLORREF kColorMeter = RGB(72, 164, 92);

CodexLimiter::SharedLimiterMapping g_mapping;
CodexLimiter::LimiterSharedState* g_state = nullptr;
CodexLimiter::LoopbackAudioEngine g_audioEngine;
HWND g_window = nullptr;
HWND g_enable = nullptr;
HWND g_slider = nullptr;
HWND g_boostEnable = nullptr;
HWND g_boostSlider = nullptr;
HWND g_boostText = nullptr;
HWND g_meter = nullptr;
HWND g_peakText = nullptr;
HWND g_ceilingText = nullptr;
HWND g_statusText = nullptr;
HINSTANCE g_instance = nullptr;
NOTIFYICONDATAW g_trayIcon{};
LONG g_lastMeterProcessCounter = 0;
bool g_processingActive = false;
std::wstring g_defaultDeviceName = L"Device: <unknown>";
HBRUSH g_windowBrush = nullptr;
HBRUSH g_panelBrush = nullptr;
HBRUSH g_meterFillBrush = nullptr;
HPEN g_ceilingPen = nullptr;
HBRUSH g_railBrush = nullptr;
HBRUSH g_activeBrush = nullptr;
HPEN g_tickPen = nullptr;
HBRUSH g_knobBrush = nullptr;
HPEN g_knobPen = nullptr;
std::wstring g_lastTrayTip;
std::wstring g_settingsPath;

std::wstring FormatDb(LONG milliDb) {
    if (milliDb <= -119000) {
        return L"-inf dBFS";
    }

    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.1f dBFS", static_cast<double>(milliDb) / 1000.0);
    return buffer;
}

std::wstring FormatBoostDb(LONG milliDb) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"+%.0f dB", static_cast<double>(CodexLimiter::ClampBoostMilliDb(milliDb)) / 1000.0);
    return buffer;
}

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

std::wstring BuildSettingsPath() {
    wchar_t localAppData[MAX_PATH]{};
    DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    if (count == 0 || count >= MAX_PATH) {
        return L"CodexLimiter.settings";
    }

    std::wstring directory = std::wstring(localAppData) + L"\\CodexLimiter";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\settings.ini";
}

bool TryParseLong(const std::string& value, LONG* parsed) {
    if (parsed == nullptr || value.empty()) {
        return false;
    }

    char* end = nullptr;
    long result = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        return false;
    }

    *parsed = static_cast<LONG>(result);
    return true;
}

CodexLimiter::LimiterSettings LoadSettings() {
    CodexLimiter::LimiterSettings settings{};
    const std::string path = WideToUtf8(g_settingsPath);
    if (path.empty()) {
        return settings;
    }

    std::ifstream file(path);
    if (!file) {
        return settings;
    }

    std::string line;
    while (std::getline(file, line)) {
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        LONG parsed = 0;
        if (key == "enabled" && TryParseLong(value, &parsed)) {
            settings.enabled = parsed != 0;
        } else if (key == "ceilingMilliDb" && TryParseLong(value, &parsed)) {
            settings.ceilingMilliDb = CodexLimiter::ClampMilliDb(parsed);
        } else if (key == "boostEnabled" && TryParseLong(value, &parsed)) {
            settings.boostEnabled = parsed != 0;
        } else if (key == "boostMilliDb" && TryParseLong(value, &parsed)) {
            settings.boostMilliDb = CodexLimiter::ClampBoostMilliDb(parsed);
        }
    }

    return settings;
}

void SaveSettings() {
    if (g_state == nullptr || g_settingsPath.empty()) {
        return;
    }

    const std::string path = WideToUtf8(g_settingsPath);
    if (path.empty()) {
        return;
    }

    const CodexLimiter::LimiterSettings settings = CodexLimiter::ReadSettingsFromState(g_state);
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return;
    }

    file << "enabled=" << (settings.enabled ? 1 : 0) << "\n";
    file << "ceilingMilliDb=" << settings.ceilingMilliDb << "\n";
    file << "boostEnabled=" << (settings.boostEnabled ? 1 : 0) << "\n";
    file << "boostMilliDb=" << settings.boostMilliDb << "\n";
}

int MilliDbToSliderPos(LONG milliDb) {
    milliDb = CodexLimiter::ClampMilliDb(milliDb);
    return static_cast<int>((milliDb - CodexLimiter::kMinCeilingMilliDb) / 1000);
}

LONG SliderPosToMilliDb(int position) {
    return CodexLimiter::ClampMilliDb(CodexLimiter::kMinCeilingMilliDb + (position * 1000));
}

int BoostMilliDbToSliderPos(LONG milliDb) {
    return static_cast<int>(CodexLimiter::ClampBoostMilliDb(milliDb) / 1000);
}

LONG BoostSliderPosToMilliDb(int position) {
    return CodexLimiter::ClampBoostMilliDb(position * 1000);
}

std::wstring ReadDefaultDeviceName();

CRITICAL_SECTION g_deviceNameLock;
bool g_deviceNameLockInitialized = false;

class TrayNotificationClient final : public IMMNotificationClient {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refCount_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (object == nullptr) return E_POINTER;
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
            std::wstring newName = ReadDefaultDeviceName();
            EnterCriticalSection(&g_deviceNameLock);
            g_defaultDeviceName = newName;
            LeaveCriticalSection(&g_deviceNameLock);
            g_audioEngine.OnDefaultDeviceChanged(deviceId);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    volatile LONG refCount_ = 1;
};

TrayNotificationClient* g_trayNotificationClient = nullptr;
IMMDeviceEnumerator* g_trayEnumerator = nullptr;

std::wstring ReadDefaultDeviceName() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        return L"Device: <enumerator error>";
    }

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        return L"Device: <none>";
    }

    ComPtr<IPropertyStore> store;
    hr = device->OpenPropertyStore(STGM_READ, &store);
    if (FAILED(hr)) {
        return L"Device: <property error>";
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    hr = store->GetValue(PKEY_Device_FriendlyName, &value);
    std::wstring name = L"<unnamed>";
    if (SUCCEEDED(hr) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        name = value.pwszVal;
    }
    PropVariantClear(&value);
    return L"Device: " + name;
}

void UpdateTrayTip() {
    std::wstring newTip;
    if (g_state == nullptr) {
        newTip = L"Codex Limiter: settings unavailable";
    } else if (g_state->enabled) {
        newTip = L"Codex Limiter: on";
    } else {
        newTip = L"Codex Limiter: off";
    }

    if (newTip == g_lastTrayTip) {
        return;
    }

    g_lastTrayTip = newTip;
    wcscpy_s(g_trayIcon.szTip, newTip.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
}

const wchar_t* EngineStateLabel(CodexLimiter::AudioEngineState engineState) {
    switch (engineState) {
    case CodexLimiter::AudioEngineState::Stopped:
        return L"Engine stopped";
    case CodexLimiter::AudioEngineState::Running:
        return L"Engine active";
    case CodexLimiter::AudioEngineState::Silence:
        return L"Engine idle (silence)";
    case CodexLimiter::AudioEngineState::DeviceChanged:
        return L"Switching device...";
    case CodexLimiter::AudioEngineState::Error:
        return L"Engine error";
    default:
        return L"Engine unknown";
    }
}

void UpdateControls() {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }

    if (g_state == nullptr) {
        SetWindowTextW(g_peakText, L"Output: unavailable");
        SetWindowTextW(g_ceilingText, L"Ceiling: unavailable");
        SetWindowTextW(g_statusText, L"Status: shared state unavailable");
        return;
    }

    const LONG ceiling = g_state->ceilingMilliDb;
    const LONG meterCounter = g_state->processCounter;
    if (meterCounter != g_lastMeterProcessCounter) {
        g_processingActive = true;
    } else {
        g_processingActive = false;
    }
    g_lastMeterProcessCounter = meterCounter;

    const auto engineState = g_audioEngine.GetState();
    const std::wstring peakText = g_processingActive
        ? (L"Output: " + FormatDb(g_state->outputPeakMilliDb))
        : L"Output: inactive";
    const std::wstring ceilingText = L"Ceiling: " + FormatDb(ceiling);
    const std::wstring boostText = L"Boost: " + FormatBoostDb(g_state->boostMilliDb);
    SetWindowTextW(g_peakText, peakText.c_str());
    SetWindowTextW(g_ceilingText, ceilingText.c_str());
    SetWindowTextW(g_boostText, boostText.c_str());
    SendMessageW(g_enable, BM_SETCHECK, g_state->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g_boostEnable, BM_SETCHECK, g_state->boostEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(g_boostSlider, g_state->boostEnabled != 0);
    InvalidateRect(g_slider, nullptr, FALSE);
    InvalidateRect(g_boostSlider, nullptr, FALSE);

    std::wstring deviceName = g_audioEngine.GetDeviceName();
    if (deviceName.empty()) {
        EnterCriticalSection(&g_deviceNameLock);
        deviceName = g_defaultDeviceName;
        LeaveCriticalSection(&g_deviceNameLock);
    }
    const std::wstring status = std::wstring(L"Status: ") + EngineStateLabel(engineState) + L", " + deviceName;
    SetWindowTextW(g_statusText, status.c_str());
    InvalidateRect(g_meter, nullptr, FALSE);
    UpdateTrayTip();
}

void AdjustCeilingByMilliDb(LONG deltaMilliDb) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    // Atomic read-modify-write via CAS loop to avoid cross-process TOCTOU race
    LONG oldVal, newVal;
    do {
        oldVal = InterlockedCompareExchange(
            const_cast<volatile LONG*>(&g_state->ceilingMilliDb), 0, 0);
        newVal = CodexLimiter::ClampMilliDb(oldVal + deltaMilliDb);
    } while (InterlockedCompareExchange(
        const_cast<volatile LONG*>(&g_state->ceilingMilliDb), newVal, oldVal) != oldVal);
    InterlockedExchange(const_cast<volatile LONG*>(&g_state->ceilingLinearScaled),
        CodexLimiter::MilliDbToLinearScaled(newVal));
    SaveSettings();
    UpdateControls();
}

void AdjustBoostByMilliDb(LONG deltaMilliDb) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    LONG oldVal, newVal;
    do {
        oldVal = InterlockedCompareExchange(
            const_cast<volatile LONG*>(&g_state->boostMilliDb), 0, 0);
        newVal = CodexLimiter::ClampBoostMilliDb(oldVal + deltaMilliDb);
    } while (InterlockedCompareExchange(
        const_cast<volatile LONG*>(&g_state->boostMilliDb), newVal, oldVal) != oldVal);
    InterlockedExchange64(const_cast<volatile LONGLONG*>(&g_state->boostLinearScaled),
        CodexLimiter::BoostMilliDbToLinearScaled(newVal));
    SaveSettings();
    UpdateControls();
}

bool IsWindowOrChild(HWND parent, HWND candidate) {
    return candidate == parent || IsChild(parent, candidate);
}

bool HandleWindowKey(MSG* message) {
    if (message == nullptr || message->message != WM_KEYDOWN || g_window == nullptr) {
        return false;
    }
    if (!IsWindowVisible(g_window) || !IsWindowOrChild(g_window, message->hwnd)) {
        return false;
    }

    const HWND focus = GetFocus();
    if (message->wParam == VK_LEFT) {
        if (focus == g_boostSlider) {
            AdjustBoostByMilliDb(-1000);
        } else {
            AdjustCeilingByMilliDb(-1000);
        }
        return true;
    }
    if (message->wParam == VK_RIGHT) {
        if (focus == g_boostSlider) {
            AdjustBoostByMilliDb(1000);
        } else {
            AdjustCeilingByMilliDb(1000);
        }
        return true;
    }
    return false;
}

void StartUiTimer() {
    if (g_window != nullptr) {
        SetTimer(g_window, kUiTimer, CodexLimiter::UiUpdateIntervalMs(), nullptr);
    }
}

void StopUiTimer() {
    if (g_window != nullptr) {
        KillTimer(g_window, kUiTimer);
    }
}

void ShowMainWindow() {
    ShowWindow(g_window, SW_SHOWNORMAL);
    SetForegroundWindow(g_window);
    StartUiTimer();
}

void ShowContextMenu() {
    POINT point{};
    GetCursorPos(&point);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"Open");
    AppendMenuW(menu, MF_STRING, 2, (g_state != nullptr && g_state->enabled) ? L"Disable limiter" : L"Enable limiter");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 3, L"Quit");
    SetForegroundWindow(g_window);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, g_window, nullptr);
    DestroyMenu(menu);

    if (command == 1) {
        ShowMainWindow();
    } else if (command == 2 && g_state != nullptr) {
        InterlockedExchange(const_cast<volatile LONG*>(&g_state->enabled), g_state->enabled ? 0 : 1);
        SaveSettings();
        UpdateControls();
    } else if (command == 3) {
        DestroyWindow(g_window);
    }
}

LRESULT CALLBACK MeterProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);

        FillRect(dc, &rect, g_panelBrush);

        LONG peak = (g_state != nullptr && g_processingActive) ? g_state->outputPeakMilliDb : -120000;
        peak = std::max<LONG>(CodexLimiter::kMinCeilingMilliDb, std::min<LONG>(0, peak));
        const int width = rect.right - rect.left;
        const int fill = static_cast<int>((static_cast<double>(peak - CodexLimiter::kMinCeilingMilliDb) /
            static_cast<double>(-CodexLimiter::kMinCeilingMilliDb)) * width);

        RECT fillRect = rect;
        fillRect.right = fillRect.left + std::max(0, std::min(width, fill));
        FillRect(dc, &fillRect, g_meterFillBrush);

        if (g_state != nullptr) {
            const int marker = static_cast<int>((static_cast<double>(g_state->ceilingMilliDb - CodexLimiter::kMinCeilingMilliDb) /
                static_cast<double>(-CodexLimiter::kMinCeilingMilliDb)) * width);
            HGDIOBJ oldPen = SelectObject(dc, g_ceilingPen);
            MoveToEx(dc, marker, rect.top, nullptr);
            LineTo(dc, marker, rect.bottom);
            SelectObject(dc, oldPen);
        }

        FrameRect(dc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int SliderXToPosition(HWND window, int x) {
    RECT rect{};
    GetClientRect(window, &rect);
    const int left = 12;
    const int right = std::max(left + 1, static_cast<int>(rect.right) - 12);
    x = std::max(left, std::min(right, x));
    const double normalized = static_cast<double>(x - left) / static_cast<double>(right - left);
    return static_cast<int>(normalized * 75.0 + 0.5);
}

void SetCeilingFromSliderX(HWND window, int x) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    CodexLimiter::SetCeilingMilliDb(g_state, SliderPosToMilliDb(SliderXToPosition(window, x)));
    SaveSettings();
    UpdateControls();
}

int BoostSliderXToPosition(HWND window, int x) {
    RECT rect{};
    GetClientRect(window, &rect);
    const int left = 12;
    const int right = std::max(left + 1, static_cast<int>(rect.right) - 12);
    x = std::max(left, std::min(right, x));
    const double normalized = static_cast<double>(x - left) / static_cast<double>(right - left);
    return static_cast<int>(normalized * 24.0 + 0.5);
}

void SetBoostFromSliderX(HWND window, int x) {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    CodexLimiter::SetBoostMilliDb(g_state, BoostSliderPosToMilliDb(BoostSliderXToPosition(window, x)));
    SaveSettings();
    UpdateControls();
}

LRESULT CALLBACK SliderProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        SetFocus(window);
        SetCapture(window);
        if (window == g_boostSlider) {
            SetBoostFromSliderX(window, GET_X_LPARAM(lParam));
        } else {
            SetCeilingFromSliderX(window, GET_X_LPARAM(lParam));
        }
        return 0;
    case WM_MOUSEMOVE:
        if ((wParam & MK_LBUTTON) != 0 && GetCapture() == window) {
            if (window == g_boostSlider) {
                SetBoostFromSliderX(window, GET_X_LPARAM(lParam));
            } else {
                SetCeilingFromSliderX(window, GET_X_LPARAM(lParam));
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (GetCapture() == window) {
            ReleaseCapture();
        }
        if (window == g_boostSlider) {
            SetBoostFromSliderX(window, GET_X_LPARAM(lParam));
        } else {
            SetCeilingFromSliderX(window, GET_X_LPARAM(lParam));
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_LEFT) {
            if (window == g_boostSlider) {
                AdjustBoostByMilliDb(-1000);
            } else {
                AdjustCeilingByMilliDb(-1000);
            }
            return 0;
        }
        if (wParam == VK_RIGHT) {
            if (window == g_boostSlider) {
                AdjustBoostByMilliDb(1000);
            } else {
                AdjustCeilingByMilliDb(1000);
            }
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);

        FillRect(dc, &rect, g_windowBrush);

        const int left = 12;
        const int right = std::max(left + 1, static_cast<int>(rect.right) - 12);
        const int centerY = (rect.bottom - rect.top) / 2;
        const int railHeight = 6;
        RECT rail{ left, centerY - railHeight / 2, right, centerY + railHeight / 2 };

        FillRect(dc, &rail, g_railBrush);

        const bool isBoostSlider = window == g_boostSlider;
        const int maxPosition = isBoostSlider ? 24 : 75;
        const int position = isBoostSlider
            ? BoostMilliDbToSliderPos(g_state != nullptr ? g_state->boostMilliDb : CodexLimiter::kDefaultBoostMilliDb)
            : MilliDbToSliderPos(g_state != nullptr ? g_state->ceilingMilliDb : CodexLimiter::kDefaultCeilingMilliDb);
        const int knobX = left + static_cast<int>((static_cast<double>(position) / static_cast<double>(maxPosition)) * (right - left));

        RECT active{ left, rail.top, knobX, rail.bottom };
        FillRect(dc, &active, g_activeBrush);

        HGDIOBJ oldPen = SelectObject(dc, g_tickPen);
        for (int i = 0; i <= maxPosition; i += (isBoostSlider ? 4 : 5)) {
            const int x = left + static_cast<int>((static_cast<double>(i) / static_cast<double>(maxPosition)) * (right - left));
            MoveToEx(dc, x, centerY + 12, nullptr);
            LineTo(dc, x, centerY + 17);
        }
        SelectObject(dc, oldPen);

        RECT knob{ knobX - 6, centerY - 14, knobX + 6, centerY + 14 };
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, g_knobBrush));
        oldPen = SelectObject(dc, g_knobPen);
        RoundRect(dc, knob.left, knob.top, knob.right, knob.bottom, 8, 8);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);

        if (GetFocus() == window) {
            RECT focus{ left - 4, centerY - 20, right + 4, centerY + 22 };
            DrawFocusRect(dc, &focus);
        }

        EndPaint(window, &paint);
        return 0;
    }
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void AddTrayIcon(HWND window) {
    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = window;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = kTrayMessage;
    g_trayIcon.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (g_trayIcon.hIcon == nullptr) {
        g_trayIcon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(g_trayIcon.szTip, L"Codex Limiter");
    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
}

void CreateControls(HWND window) {
    g_peakText = CreateWindowExW(0, L"STATIC", L"Output: -inf dBFS", WS_CHILD | WS_VISIBLE,
        16, 14, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPeakText)), g_instance, nullptr);
    g_meter = CreateWindowExW(0, L"CodexLimiterMeter", nullptr, WS_CHILD | WS_VISIBLE,
        16, 38, 310, 18, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdMeter)), g_instance, nullptr);
    g_ceilingText = CreateWindowExW(0, L"STATIC", L"Ceiling: -30.0 dBFS", WS_CHILD | WS_VISIBLE,
        16, 66, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCeilingText)), g_instance, nullptr);
    g_slider = CreateWindowExW(0, L"CodexLimiterSlider", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        16, 90, 310, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSlider)), g_instance, nullptr);

    g_enable = CreateWindowExW(0, L"BUTTON", L"Limiter enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 128, 160, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEnable)), g_instance, nullptr);

    g_boostEnable = CreateWindowExW(0, L"BUTTON", L"Soundbooster enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 160, 190, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostEnable)), g_instance, nullptr);
    g_boostText = CreateWindowExW(0, L"STATIC", L"Boost: +0 dB", WS_CHILD | WS_VISIBLE,
        16, 192, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostText)), g_instance, nullptr);
    g_boostSlider = CreateWindowExW(0, L"CodexLimiterSlider", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        16, 216, 310, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostSlider)), g_instance, nullptr);

    g_statusText = CreateWindowExW(0, L"STATIC", L"Status: starting", WS_CHILD | WS_VISIBLE,
        16, 260, 320, 40, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusText)), g_instance, nullptr);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls(window);
        AddTrayIcon(window);
        g_mapping.Open();
        g_state = g_mapping.Get();
        if (g_state != nullptr) {
            CodexLimiter::ApplySettings(g_state, LoadSettings());
        }
        g_defaultDeviceName = ReadDefaultDeviceName();
        {
            g_trayNotificationClient = new TrayNotificationClient();
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                IID_PPV_ARGS(&g_trayEnumerator));
            if (g_trayEnumerator != nullptr) {
                g_trayEnumerator->RegisterEndpointNotificationCallback(g_trayNotificationClient);
            }
        }
        g_audioEngine.Start(g_state);
        UpdateControls();
        return 0;
    case WM_ERASEBKGND: {
        RECT rect{};
        GetClientRect(window, &rect);
        FillRect(reinterpret_cast<HDC>(wParam), &rect, g_windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), message == WM_CTLCOLORSTATIC ? kColorText : kColorMutedText);
        return reinterpret_cast<LRESULT>(g_windowBrush);
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdEnable && g_state != nullptr) {
            const bool checked = SendMessageW(g_enable, BM_GETCHECK, 0, 0) == BST_CHECKED;
            InterlockedExchange(const_cast<volatile LONG*>(&g_state->enabled), checked ? 1 : 0);
            SaveSettings();
            UpdateControls();
            return 0;
        }
        if (LOWORD(wParam) == kIdBoostEnable && g_state != nullptr) {
            const bool checked = SendMessageW(g_boostEnable, BM_GETCHECK, 0, 0) == BST_CHECKED;
            InterlockedExchange(const_cast<volatile LONG*>(&g_state->boostEnabled), checked ? 1 : 0);
            SaveSettings();
            UpdateControls();
            return 0;
        }
        return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == g_slider && g_state != nullptr) {
            const int position = static_cast<int>(SendMessageW(g_slider, TBM_GETPOS, 0, 0));
            CodexLimiter::SetCeilingMilliDb(g_state, SliderPosToMilliDb(position));
            SaveSettings();
            UpdateControls();
            return 0;
        }
        return 0;
    case WM_TIMER:
        if (wParam == kUiTimer) {
            UpdateControls();
            return 0;
        }
        return 0;
    case kTrayMessage:
        if (lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
            ShowMainWindow();
        } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowContextMenu();
        }
        return 0;
    case WM_CLOSE:
        StopUiTimer();
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_DESTROY:
        g_audioEngine.Stop();
        if (g_trayEnumerator != nullptr) {
            g_trayEnumerator->UnregisterEndpointNotificationCallback(g_trayNotificationClient);
            g_trayEnumerator->Release();
            g_trayEnumerator = nullptr;
        }
        if (g_trayNotificationClient != nullptr) {
            g_trayNotificationClient->Release();
            g_trayNotificationClient = nullptr;
        }
        KillTimer(window, kUiTimer);
        RemoveTrayIcon();
        if (g_windowBrush != nullptr) {
            DeleteObject(g_windowBrush);
            g_windowBrush = nullptr;
        }
        if (g_panelBrush != nullptr) {
            DeleteObject(g_panelBrush);
            g_panelBrush = nullptr;
        }
        if (g_meterFillBrush != nullptr) {
            DeleteObject(g_meterFillBrush);
            g_meterFillBrush = nullptr;
        }
        if (g_ceilingPen != nullptr) {
            DeleteObject(g_ceilingPen);
            g_ceilingPen = nullptr;
        }
        if (g_railBrush != nullptr) {
            DeleteObject(g_railBrush);
            g_railBrush = nullptr;
        }
        if (g_activeBrush != nullptr) {
            DeleteObject(g_activeBrush);
            g_activeBrush = nullptr;
        }
        if (g_tickPen != nullptr) {
            DeleteObject(g_tickPen);
            g_tickPen = nullptr;
        }
        if (g_knobBrush != nullptr) {
            DeleteObject(g_knobBrush);
            g_knobBrush = nullptr;
        }
        if (g_knobPen != nullptr) {
            DeleteObject(g_knobPen);
            g_knobPen = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool RegisterWindowClasses() {
    WNDCLASSW mainClass{};
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.hInstance = g_instance;
    mainClass.lpszClassName = L"CodexLimiterTrayWindow";
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    mainClass.hbrBackground = g_windowBrush;
    if (!RegisterClassW(&mainClass)) {
        return false;
    }

    WNDCLASSW meterClass{};
    meterClass.lpfnWndProc = MeterProc;
    meterClass.hInstance = g_instance;
    meterClass.lpszClassName = L"CodexLimiterMeter";
    meterClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    meterClass.hbrBackground = g_panelBrush;
    if (!RegisterClassW(&meterClass)) {
        return false;
    }

    WNDCLASSW sliderClass{};
    sliderClass.lpfnWndProc = SliderProc;
    sliderClass.hInstance = g_instance;
    sliderClass.lpszClassName = L"CodexLimiterSlider";
    sliderClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sliderClass.hbrBackground = g_windowBrush;
    return RegisterClassW(&sliderClass) != 0;
}

void ApplyDarkTitleBar(HWND window) {
    if (window == nullptr) {
        return;
    }

    BOOL enabled = TRUE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &enabled, sizeof(enabled));

    COLORREF caption = kColorWindow;
    COLORREF text = kColorText;
    DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &text, sizeof(text));
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    g_instance = instance;
    InitializeCriticalSection(&g_deviceNameLock);
    g_deviceNameLockInitialized = true;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_windowBrush = CreateSolidBrush(kColorWindow);
    g_panelBrush = CreateSolidBrush(kColorPanel);
    g_meterFillBrush = CreateSolidBrush(kColorMeter);
    g_ceilingPen = CreatePen(PS_SOLID, 2, kColorText);
    g_railBrush = CreateSolidBrush(kColorRail);
    g_activeBrush = CreateSolidBrush(kColorAccent);
    g_tickPen = CreatePen(PS_SOLID, 1, RGB(90, 96, 108));
    g_knobBrush = CreateSolidBrush(RGB(235, 238, 244));
    g_knobPen = CreatePen(PS_SOLID, 1, RGB(110, 116, 128));

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    g_mapping.Open();
    g_state = g_mapping.Get();
    g_settingsPath = BuildSettingsPath();
    if (g_state != nullptr) {
        CodexLimiter::ApplySettings(g_state, LoadSettings());
    }

    if (!RegisterWindowClasses()) {
        CoUninitialize();
        return 1;
    }

    g_window = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"CodexLimiterTrayWindow",
        L"Codex Limiter",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kWindowWidth,
        kWindowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (g_window == nullptr) {
        CoUninitialize();
        return 1;
    }
    ApplyDarkTitleBar(g_window);

    if (commandLine != nullptr && wcsstr(commandLine, L"--show") != nullptr) {
        ShowMainWindow();
    } else {
        ShowWindow(g_window, SW_HIDE);
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (HandleWindowKey(&message)) {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CoUninitialize();

    if (g_deviceNameLockInitialized) {
        DeleteCriticalSection(&g_deviceNameLock);
        g_deviceNameLockInitialized = false;
    }

    return 0;
}
