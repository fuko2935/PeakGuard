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

#include "common/PeakGuardSharedState.h"
#include "AudioEndpointSelection.h"
#include "AudioPowerPolicy.h"
#include "HotkeyPolicy.h"
#include "LimiterSettings.h"
#include "LoopbackAudioEngine.h"
#include "StartupPolicy.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace {

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kShowExistingMessage = WM_APP + 20;
constexpr UINT kExitExistingMessage = WM_APP + 21;
constexpr UINT_PTR kUiTimer = 1;
constexpr UINT_PTR kOverlayTimer = 2;
constexpr UINT kOverlayHideMs = 1400;
constexpr int kWindowWidth = 560;
constexpr int kWindowHeight = PeakGuard::UiWindowHeightPx();
constexpr int kOverlayWidth = 320;
constexpr int kOverlayHeight = 96;
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\PeakGuardTrayInstance";
constexpr wchar_t kMainWindowClassName[] = L"PeakGuardTrayWindow";

constexpr int kIdEnable = 1001;
constexpr int kIdSlider = 1002;
constexpr int kIdMeter = 1003;
constexpr int kIdPeakText = 1004;
constexpr int kIdCeilingText = 1005;
constexpr int kIdStatusText = 1006;
constexpr int kIdBoostEnable = 1007;
constexpr int kIdBoostSlider = 1008;
constexpr int kIdBoostText = 1009;
constexpr int kIdTabLimiter = 1010;
constexpr int kIdTabHotkeys = 1011;
constexpr int kIdTabStartup = 1012;
constexpr int kIdTabPower = 1013;
constexpr int kIdHotkeyLowerCeiling = 1020;
constexpr int kIdHotkeyRaiseCeiling = 1021;
constexpr int kIdHotkeyToggleLimiter = 1022;
constexpr int kIdHotkeyLowerBoost = 1023;
constexpr int kIdHotkeyRaiseBoost = 1024;
constexpr int kIdHotkeyReset = 1025;
constexpr int kIdStartupToggle = 1030;
constexpr int kIdOverlayToggle = 1031;
constexpr int kIdPowerText = 1032;
constexpr COLORREF kColorWindow = RGB(16, 18, 22);
constexpr COLORREF kColorPanel = RGB(27, 30, 36);
constexpr COLORREF kColorText = RGB(238, 241, 245);
constexpr COLORREF kColorMutedText = RGB(170, 178, 190);
constexpr COLORREF kColorRail = RGB(58, 63, 72);
constexpr COLORREF kColorAccent = RGB(245, 164, 18);
constexpr COLORREF kColorMeter = RGB(72, 164, 92);

PeakGuard::SharedLimiterMapping g_mapping;
PeakGuard::PeakGuardSharedState* g_state = nullptr;
PeakGuard::LoopbackAudioEngine g_audioEngine;
PeakGuard::LimiterSettings g_settings;
HANDLE g_singleInstanceMutex = nullptr;
HWND g_window = nullptr;
HWND g_overlay = nullptr;
HWND g_enable = nullptr;
HWND g_slider = nullptr;
HWND g_boostEnable = nullptr;
HWND g_boostSlider = nullptr;
HWND g_boostText = nullptr;
HWND g_meter = nullptr;
HWND g_peakText = nullptr;
HWND g_ceilingText = nullptr;
HWND g_statusText = nullptr;
HWND g_tabLimiter = nullptr;
HWND g_tabHotkeys = nullptr;
HWND g_tabStartup = nullptr;
HWND g_tabPower = nullptr;
HWND g_hotkeyLowerCeiling = nullptr;
HWND g_hotkeyRaiseCeiling = nullptr;
HWND g_hotkeyToggleLimiter = nullptr;
HWND g_hotkeyLowerBoost = nullptr;
HWND g_hotkeyRaiseBoost = nullptr;
HWND g_hotkeyReset = nullptr;
HWND g_startupToggle = nullptr;
HWND g_overlayToggle = nullptr;
HWND g_powerText = nullptr;
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
HFONT g_uiFont = nullptr;
std::wstring g_lastTrayTip;
std::wstring g_settingsPath;
std::wstring g_hotkeyStatus;
int g_capturingHotkeyId = 0;
std::wstring g_settingsMessage;
std::wstring g_overlayTitle = L"Limiter";
std::wstring g_overlayValue;
PeakGuard::HotkeyBinding g_hotkeys[] = {
    { PeakGuard::kHotkeyLowerCeilingId, MOD_CONTROL | MOD_ALT, 'A' },
    { PeakGuard::kHotkeyRaiseCeilingId, MOD_CONTROL | MOD_ALT, 'D' },
    { PeakGuard::kHotkeyToggleLimiterId, MOD_CONTROL | MOD_ALT, 'S' },
    { PeakGuard::kHotkeyLowerBoostId, MOD_CONTROL | MOD_ALT, VK_F6 },
    { PeakGuard::kHotkeyRaiseBoostId, MOD_CONTROL | MOD_ALT, VK_F7 },
};

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
    swprintf_s(buffer, L"+%.0f dB", static_cast<double>(PeakGuard::ClampBoostMilliDb(milliDb)) / 1000.0);
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
        return L"PeakGuard.settings";
    }

    std::wstring directory = std::wstring(localAppData) + L"\\PeakGuard";
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

PeakGuard::LimiterSettings LoadSettings() {
    PeakGuard::LimiterSettings settings{};
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
            settings.ceilingMilliDb = PeakGuard::ClampMilliDb(parsed);
        } else if (key == "boostEnabled" && TryParseLong(value, &parsed)) {
            settings.boostEnabled = parsed != 0;
        } else if (key == "boostMilliDb" && TryParseLong(value, &parsed)) {
            settings.boostMilliDb = PeakGuard::ClampBoostMilliDb(parsed);
        } else if (key == "overlayEnabled" && TryParseLong(value, &parsed)) {
            settings.overlayEnabled = parsed != 0;
        } else if (key == "startWithWindows" && TryParseLong(value, &parsed)) {
            settings.startWithWindows = parsed != 0;
        } else if (key == "selectedTab" && TryParseLong(value, &parsed)) {
            settings.selectedTab = static_cast<PeakGuard::LimiterSettingsTab>(
                std::max<LONG>(0, std::min<LONG>(3, parsed)));
        } else if (key == "lowerCeilingHotkey" && TryParseLong(value, &parsed)) {
            settings.lowerCeilingHotkey = static_cast<UINT>(parsed);
        } else if (key == "raiseCeilingHotkey" && TryParseLong(value, &parsed)) {
            settings.raiseCeilingHotkey = static_cast<UINT>(parsed);
        } else if (key == "toggleLimiterHotkey" && TryParseLong(value, &parsed)) {
            settings.toggleLimiterHotkey = static_cast<UINT>(parsed);
        } else if (key == "lowerBoostHotkey" && TryParseLong(value, &parsed)) {
            settings.lowerBoostHotkey = static_cast<UINT>(parsed);
        } else if (key == "raiseBoostHotkey" && TryParseLong(value, &parsed)) {
            settings.raiseBoostHotkey = static_cast<UINT>(parsed);
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

    PeakGuard::LimiterSettings settings = PeakGuard::ReadSettingsFromState(g_state);
    settings.overlayEnabled = g_settings.overlayEnabled;
    settings.startWithWindows = g_settings.startWithWindows;
    settings.selectedTab = g_settings.selectedTab;
    settings.lowerCeilingHotkey = g_settings.lowerCeilingHotkey;
    settings.raiseCeilingHotkey = g_settings.raiseCeilingHotkey;
    settings.toggleLimiterHotkey = g_settings.toggleLimiterHotkey;
    settings.lowerBoostHotkey = g_settings.lowerBoostHotkey;
    settings.raiseBoostHotkey = g_settings.raiseBoostHotkey;
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return;
    }

    file << "enabled=" << (settings.enabled ? 1 : 0) << "\n";
    file << "ceilingMilliDb=" << settings.ceilingMilliDb << "\n";
    file << "boostEnabled=" << (settings.boostEnabled ? 1 : 0) << "\n";
    file << "boostMilliDb=" << settings.boostMilliDb << "\n";
    file << "overlayEnabled=" << (settings.overlayEnabled ? 1 : 0) << "\n";
    file << "startWithWindows=" << (settings.startWithWindows ? 1 : 0) << "\n";
    file << "selectedTab=" << static_cast<int>(settings.selectedTab) << "\n";
    file << "lowerCeilingHotkey=" << settings.lowerCeilingHotkey << "\n";
    file << "raiseCeilingHotkey=" << settings.raiseCeilingHotkey << "\n";
    file << "toggleLimiterHotkey=" << settings.toggleLimiterHotkey << "\n";
    file << "lowerBoostHotkey=" << settings.lowerBoostHotkey << "\n";
    file << "raiseBoostHotkey=" << settings.raiseBoostHotkey << "\n";
}

PeakGuard::HotkeyBinding* FindHotkeyBinding(int hotkeyId) {
    for (auto& binding : g_hotkeys) {
        if (binding.id == hotkeyId) {
            return &binding;
        }
    }
    return nullptr;
}

UINT SettingsHotkeyForId(const PeakGuard::LimiterSettings& settings, int hotkeyId) {
    switch (hotkeyId) {
    case PeakGuard::kHotkeyLowerCeilingId:
        return settings.lowerCeilingHotkey;
    case PeakGuard::kHotkeyRaiseCeilingId:
        return settings.raiseCeilingHotkey;
    case PeakGuard::kHotkeyToggleLimiterId:
        return settings.toggleLimiterHotkey;
    case PeakGuard::kHotkeyLowerBoostId:
        return settings.lowerBoostHotkey;
    case PeakGuard::kHotkeyRaiseBoostId:
        return settings.raiseBoostHotkey;
    default:
        return PeakGuard::LimiterHotkeyKey(hotkeyId);
    }
}

void ApplyHotkeysFromSettings() {
    for (auto& binding : g_hotkeys) {
        const UINT key = SettingsHotkeyForId(g_settings, binding.id);
        binding.modifiers = PeakGuard::LimiterHotkeyModifiers();
        binding.key = PeakGuard::IsSupportedLimiterHotkey(binding.modifiers, key)
            ? key
            : PeakGuard::LimiterHotkeyKey(binding.id);
    }
}

void StoreHotkeysToSettings() {
    for (const auto& binding : g_hotkeys) {
        switch (binding.id) {
        case PeakGuard::kHotkeyLowerCeilingId:
            g_settings.lowerCeilingHotkey = binding.key;
            break;
        case PeakGuard::kHotkeyRaiseCeilingId:
            g_settings.raiseCeilingHotkey = binding.key;
            break;
        case PeakGuard::kHotkeyToggleLimiterId:
            g_settings.toggleLimiterHotkey = binding.key;
            break;
        case PeakGuard::kHotkeyLowerBoostId:
            g_settings.lowerBoostHotkey = binding.key;
            break;
        case PeakGuard::kHotkeyRaiseBoostId:
            g_settings.raiseBoostHotkey = binding.key;
            break;
        default:
            break;
        }
    }
}

void ResetHotkeysToDefaults() {
    for (auto& binding : g_hotkeys) {
        binding = PeakGuard::DefaultHotkeyBinding(binding.id);
    }
    StoreHotkeysToSettings();
}

std::wstring HotkeyDisplayForId(int hotkeyId) {
    const PeakGuard::HotkeyBinding* binding = FindHotkeyBinding(hotkeyId);
    if (binding == nullptr) {
        return L"Unassigned";
    }
    return PeakGuard::LimiterHotkeyDisplay(binding->modifiers, binding->key);
}

std::wstring HotkeyButtonText(const wchar_t* label, int hotkeyId) {
    return std::wstring(label) + L": " + HotkeyDisplayForId(hotkeyId);
}

void UpdateSettingsLabels() {
    if (g_hotkeyLowerCeiling != nullptr) {
        SetWindowTextW(g_hotkeyLowerCeiling,
            HotkeyButtonText(L"Limiter down", PeakGuard::kHotkeyLowerCeilingId).c_str());
    }
    if (g_hotkeyRaiseCeiling != nullptr) {
        SetWindowTextW(g_hotkeyRaiseCeiling,
            HotkeyButtonText(L"Limiter up", PeakGuard::kHotkeyRaiseCeilingId).c_str());
    }
    if (g_hotkeyToggleLimiter != nullptr) {
        SetWindowTextW(g_hotkeyToggleLimiter,
            HotkeyButtonText(L"Limiter toggle", PeakGuard::kHotkeyToggleLimiterId).c_str());
    }
    if (g_hotkeyLowerBoost != nullptr) {
        SetWindowTextW(g_hotkeyLowerBoost,
            HotkeyButtonText(L"Boost down", PeakGuard::kHotkeyLowerBoostId).c_str());
    }
    if (g_hotkeyRaiseBoost != nullptr) {
        SetWindowTextW(g_hotkeyRaiseBoost,
            HotkeyButtonText(L"Boost up", PeakGuard::kHotkeyRaiseBoostId).c_str());
    }
    if (g_startupToggle != nullptr) {
        SetWindowTextW(g_startupToggle, g_settings.startWithWindows
            ? L"Start with Windows: On"
            : L"Start with Windows: Off");
    }
    if (g_overlayToggle != nullptr) {
        SetWindowTextW(g_overlayToggle, g_settings.overlayEnabled
            ? L"Overlay: On"
            : L"Overlay: Off");
    }
    if (g_powerText != nullptr) {
        SetWindowTextW(g_powerText, L"Power: UI refresh runs only while a window or overlay is visible.");
    }
}

void RegisterLimiterHotkeys();
void UnregisterLimiterHotkeys();

void ShowControl(HWND control, bool visible) {
    if (control != nullptr) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void ApplyTabVisibility() {
    const auto tab = g_settings.selectedTab;
    const bool limiterTab = tab == PeakGuard::LimiterSettingsTab::Limiter;
    const bool hotkeysTab = tab == PeakGuard::LimiterSettingsTab::Hotkeys;
    const bool startupTab = tab == PeakGuard::LimiterSettingsTab::Startup;
    const bool powerTab = tab == PeakGuard::LimiterSettingsTab::Power;

    ShowControl(g_peakText, limiterTab);
    ShowControl(g_meter, limiterTab);
    ShowControl(g_ceilingText, limiterTab);
    ShowControl(g_slider, limiterTab);
    ShowControl(g_enable, limiterTab);
    ShowControl(g_boostEnable, limiterTab);
    ShowControl(g_boostText, limiterTab);
    ShowControl(g_boostSlider, limiterTab);

    ShowControl(g_hotkeyLowerCeiling, hotkeysTab);
    ShowControl(g_hotkeyRaiseCeiling, hotkeysTab);
    ShowControl(g_hotkeyToggleLimiter, hotkeysTab);
    ShowControl(g_hotkeyLowerBoost, hotkeysTab);
    ShowControl(g_hotkeyRaiseBoost, hotkeysTab);
    ShowControl(g_hotkeyReset, hotkeysTab);

    ShowControl(g_startupToggle, startupTab);
    ShowControl(g_overlayToggle, startupTab);
    ShowControl(g_powerText, powerTab);
    ShowControl(g_statusText, true);
}

void LayoutControls(HWND window) {
    RECT rect{};
    GetClientRect(window, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int contentWidth = std::max(360, std::min(540, width - 40));
    const int left = (width - contentWidth) / 2;
    const int tabWidth = (contentWidth - 24) / 4;

    MoveWindow(g_tabLimiter, left, 16, tabWidth, 28, TRUE);
    MoveWindow(g_tabHotkeys, left + tabWidth + 8, 16, tabWidth, 28, TRUE);
    MoveWindow(g_tabStartup, left + (tabWidth + 8) * 2, 16, tabWidth, 28, TRUE);
    MoveWindow(g_tabPower, left + (tabWidth + 8) * 3, 16, tabWidth, 28, TRUE);

    MoveWindow(g_peakText, left, 64, contentWidth, 24, TRUE);
    MoveWindow(g_meter, left, 94, contentWidth, 18, TRUE);
    MoveWindow(g_ceilingText, left, 126, contentWidth, 28, TRUE);
    MoveWindow(g_slider, left, 160, contentWidth, 34, TRUE);

    const int tileWidth = (contentWidth - 12) / 2;
    MoveWindow(g_enable, left, 212, tileWidth, 30, TRUE);
    MoveWindow(g_boostEnable, left + tileWidth + 12, 212, tileWidth, 30, TRUE);
    MoveWindow(g_boostText, left, 258, contentWidth, 24, TRUE);
    MoveWindow(g_boostSlider, left, 290, contentWidth, 34, TRUE);

    MoveWindow(g_hotkeyLowerCeiling, left, 76, tileWidth, 34, TRUE);
    MoveWindow(g_hotkeyRaiseCeiling, left + tileWidth + 12, 76, tileWidth, 34, TRUE);
    MoveWindow(g_hotkeyToggleLimiter, left, 122, contentWidth, 34, TRUE);
    MoveWindow(g_hotkeyLowerBoost, left, 176, tileWidth, 34, TRUE);
    MoveWindow(g_hotkeyRaiseBoost, left + tileWidth + 12, 176, tileWidth, 34, TRUE);
    MoveWindow(g_hotkeyReset, left, 230, contentWidth, 34, TRUE);

    MoveWindow(g_startupToggle, left, 110, contentWidth, 36, TRUE);
    MoveWindow(g_overlayToggle, left, 162, contentWidth, 36, TRUE);
    MoveWindow(g_powerText, left, 130, contentWidth, 70, TRUE);

    MoveWindow(g_statusText, left, std::max(350, height - 58), contentWidth, 36, TRUE);
    ApplyTabVisibility();
}

std::wstring CurrentExecutablePath() {
    wchar_t path[MAX_PATH]{};
    DWORD count = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (count == 0 || count >= MAX_PATH) {
        return {};
    }
    return path;
}

bool OpenStartupRunKey(REGSAM access, HKEY* key) {
    if (key == nullptr) {
        return false;
    }

    return RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        access,
        nullptr,
        key,
        nullptr) == ERROR_SUCCESS;
}

bool SetStartWithWindows(bool enabled) {
    HKEY key = nullptr;
    if (!OpenStartupRunKey(KEY_SET_VALUE, &key)) {
        return false;
    }

    const std::wstring valueName = PeakGuard::StartupRegistryValueName();
    bool success = false;
    if (enabled) {
        const std::wstring path = CurrentExecutablePath();
        const std::wstring command = PeakGuard::QuoteStartupCommand(path);
        success = !path.empty() && RegSetValueExW(key, valueName.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    } else {
        const LSTATUS status = RegDeleteValueW(key, valueName.c_str());
        success = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return success;
}

bool IsStartWithWindowsEnabled() {
    HKEY key = nullptr;
    if (!OpenStartupRunKey(KEY_QUERY_VALUE, &key)) {
        return false;
    }

    const std::wstring valueName = PeakGuard::StartupRegistryValueName();
    const LSTATUS status = RegQueryValueExW(key, valueName.c_str(), nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

int MilliDbToSliderPos(LONG milliDb) {
    milliDb = PeakGuard::ClampMilliDb(milliDb);
    return static_cast<int>((milliDb - PeakGuard::kMinCeilingMilliDb) / 1000);
}

LONG SliderPosToMilliDb(int position) {
    return PeakGuard::ClampMilliDb(PeakGuard::kMinCeilingMilliDb + (position * 1000));
}

int BoostMilliDbToSliderPos(LONG milliDb) {
    return static_cast<int>(PeakGuard::ClampBoostMilliDb(milliDb) / 1000);
}

LONG BoostSliderPosToMilliDb(int position) {
    return PeakGuard::ClampBoostMilliDb(position * 1000);
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
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR deviceId) override {
        g_audioEngine.OnDefaultDeviceChanged(deviceId);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        g_audioEngine.OnDefaultDeviceChanged(nullptr);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD newState) override {
        if (PeakGuard::IsActiveEndpointState(newState)) {
            g_audioEngine.OnDefaultDeviceChanged(deviceId);
        } else if (PeakGuard::EndpointNotificationRequiresRetarget(deviceId, newState)) {
            g_audioEngine.OnDefaultDeviceChanged(nullptr);
        }
        return S_OK;
    }
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
        newTip = L"PeakGuard: settings unavailable";
    } else if (g_state->enabled) {
        newTip = L"PeakGuard: on";
    } else {
        newTip = L"PeakGuard: off";
    }

    if (newTip == g_lastTrayTip) {
        return;
    }

    g_lastTrayTip = newTip;
    wcscpy_s(g_trayIcon.szTip, newTip.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
}

const wchar_t* EngineStateLabel(PeakGuard::AudioEngineState engineState) {
    switch (engineState) {
    case PeakGuard::AudioEngineState::Stopped:
        return L"Engine stopped";
    case PeakGuard::AudioEngineState::Running:
        return L"Engine active";
    case PeakGuard::AudioEngineState::Silence:
        return L"Engine idle (silence)";
    case PeakGuard::AudioEngineState::DeviceChanged:
        return L"Switching device...";
    case PeakGuard::AudioEngineState::Error:
        return L"Engine error";
    default:
        return L"Engine unknown";
    }
}

std::wstring ShortEngineStateLabel(PeakGuard::AudioEngineState engineState) {
    switch (engineState) {
    case PeakGuard::AudioEngineState::Running:
        return L"Active";
    case PeakGuard::AudioEngineState::Silence:
        return L"Idle";
    case PeakGuard::AudioEngineState::DeviceChanged:
        return L"Device changed";
    case PeakGuard::AudioEngineState::Error:
        return L"Error";
    case PeakGuard::AudioEngineState::Stopped:
        return L"Stopped";
    default:
        return L"Unknown";
    }
}

std::wstring ShortDeviceLabel(const std::wstring& deviceName) {
    if (deviceName.find(L"CABLE") != std::wstring::npos ||
        deviceName.find(L"VB-Audio") != std::wstring::npos ||
        deviceName.find(L"Virtual") != std::wstring::npos) {
        return L"CABLE loopback";
    }
    constexpr wchar_t prefix[] = L"Device: ";
    if (deviceName.rfind(prefix, 0) == 0) {
        return deviceName.substr(wcslen(prefix));
    }
    return deviceName.empty() ? L"No device" : deviceName;
}

void UpdateControls() {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }

    if (g_state == nullptr) {
        SetWindowTextW(g_peakText, L"Output: unavailable");
        SetWindowTextW(g_ceilingText, L"Ceiling: unavailable");
        SetWindowTextW(g_statusText, L"Shared state unavailable");
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
    std::wstring status = ShortEngineStateLabel(engineState) + L" - " + ShortDeviceLabel(deviceName);
    if (!g_hotkeyStatus.empty()) {
        status += L" - " + g_hotkeyStatus;
    }
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
        newVal = PeakGuard::ClampMilliDb(oldVal + deltaMilliDb);
    } while (InterlockedCompareExchange(
        const_cast<volatile LONG*>(&g_state->ceilingMilliDb), newVal, oldVal) != oldVal);
    InterlockedExchange(const_cast<volatile LONG*>(&g_state->ceilingLinearScaled),
        PeakGuard::MilliDbToLinearScaled(newVal));
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
        newVal = PeakGuard::ClampBoostMilliDb(oldVal + deltaMilliDb);
    } while (InterlockedCompareExchange(
        const_cast<volatile LONG*>(&g_state->boostMilliDb), newVal, oldVal) != oldVal);
    InterlockedExchange64(const_cast<volatile LONGLONG*>(&g_state->boostLinearScaled),
        PeakGuard::BoostMilliDbToLinearScaled(newVal));
    SaveSettings();
    UpdateControls();
}

void ToggleLimiterEnabled() {
    if (g_state == nullptr) {
        g_mapping.Open();
        g_state = g_mapping.Get();
    }
    if (g_state == nullptr) {
        return;
    }

    InterlockedExchange(const_cast<volatile LONG*>(&g_state->enabled), g_state->enabled ? 0 : 1);
    SaveSettings();
    UpdateControls();
}

void SelectTab(PeakGuard::LimiterSettingsTab tab) {
    g_settings.selectedTab = tab;
    SaveSettings();
    UpdateSettingsLabels();
    LayoutControls(g_window);
}

void ReRegisterLimiterHotkeys() {
    UnregisterLimiterHotkeys();
    RegisterLimiterHotkeys();
}

void BeginHotkeyCapture(int hotkeyId, HWND button) {
    g_capturingHotkeyId = hotkeyId;
    if (button != nullptr) {
        SetWindowTextW(button, L"Press Ctrl+Alt+key...");
    }
    SetFocus(g_window);
}

bool CompleteHotkeyCapture(WPARAM key) {
    if (g_capturingHotkeyId == 0) {
        return false;
    }

    const UINT hotkey = static_cast<UINT>(key);
    if (PeakGuard::IsHotkeyCaptureModifierKey(hotkey)) {
        return true;
    }
    if (hotkey == VK_ESCAPE) {
        g_capturingHotkeyId = 0;
        g_settingsMessage = L"Hotkey capture cancelled";
        UpdateSettingsLabels();
        UpdateControls();
        return true;
    }

    const UINT modifiers = ((GetKeyState(VK_CONTROL) & 0x8000) != 0 ? MOD_CONTROL : 0) |
        ((GetKeyState(VK_MENU) & 0x8000) != 0 ? MOD_ALT : 0);
    PeakGuard::HotkeyBinding* binding = FindHotkeyBinding(g_capturingHotkeyId);
    if (binding != nullptr && PeakGuard::IsSupportedLimiterHotkey(modifiers, hotkey)) {
        if (PeakGuard::HasDuplicateLimiterHotkey(
                g_hotkeys, ARRAYSIZE(g_hotkeys), g_capturingHotkeyId, modifiers, hotkey)) {
            g_settingsMessage = L"Hotkey already assigned";
            UpdateControls();
            return true;
        }
        binding->modifiers = modifiers;
        binding->key = hotkey;
        StoreHotkeysToSettings();
        SaveSettings();
        ReRegisterLimiterHotkeys();
        g_settingsMessage = L"Hotkey saved";
    } else {
        g_settingsMessage = L"Use Ctrl+Alt with A-Z or F1-F24";
        UpdateControls();
        return true;
    }

    g_capturingHotkeyId = 0;
    UpdateSettingsLabels();
    UpdateControls();
    return true;
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

bool ShouldRunUiTimer() {
    return (g_window != nullptr && IsWindowVisible(g_window)) ||
        (g_overlay != nullptr && IsWindowVisible(g_overlay));
}

void StartUiTimer() {
    if (g_window != nullptr && ShouldRunUiTimer()) {
        SetTimer(g_window, kUiTimer, PeakGuard::UiUpdateIntervalMs(), nullptr);
    }
}

void StopUiTimer() {
    if (g_window != nullptr) {
        KillTimer(g_window, kUiTimer);
    }
}

void PositionOverlay() {
    if (g_overlay == nullptr) {
        return;
    }

    RECT workArea{};
    HMONITOR monitor = MonitorFromWindow(g_window, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &info)) {
        workArea = info.rcWork;
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    }

    const int x = workArea.left + ((workArea.right - workArea.left) - kOverlayWidth) / 2;
    const int y = workArea.bottom - kOverlayHeight - 80;
    SetWindowPos(g_overlay, HWND_TOPMOST, x, y,
        kOverlayWidth, kOverlayHeight, SWP_NOACTIVATE);
}

void ShowLimiterOverlay(const std::wstring& title, const std::wstring& value) {
    if (g_overlay == nullptr || !g_settings.overlayEnabled) {
        return;
    }

    g_overlayTitle = title;
    g_overlayValue = value;
    PositionOverlay();
    InvalidateRect(g_overlay, nullptr, TRUE);
    ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
    StartUiTimer();
    SetTimer(g_window, kOverlayTimer, kOverlayHideMs, nullptr);
}

void HideLimiterOverlay() {
    if (g_overlay != nullptr) {
        ShowWindow(g_overlay, SW_HIDE);
    }
    if (g_window != nullptr) {
        KillTimer(g_window, kOverlayTimer);
        if (!ShouldRunUiTimer()) {
            KillTimer(g_window, kUiTimer);
        }
    }
}

bool RegisterOneHotkey(int hotkeyId, const wchar_t* label) {
    PeakGuard::HotkeyBinding* binding = FindHotkeyBinding(hotkeyId);
    if (binding != nullptr && RegisterHotKey(g_window, hotkeyId, binding->modifiers, binding->key)) {
        return true;
    }

    if (!g_hotkeyStatus.empty()) {
        g_hotkeyStatus += L"; ";
    }
    g_hotkeyStatus += label;
    g_hotkeyStatus += L" unavailable";
    return false;
}

void RegisterLimiterHotkeys() {
    g_hotkeyStatus.clear();
    RegisterOneHotkey(PeakGuard::kHotkeyLowerCeilingId, HotkeyDisplayForId(PeakGuard::kHotkeyLowerCeilingId).c_str());
    RegisterOneHotkey(PeakGuard::kHotkeyRaiseCeilingId, HotkeyDisplayForId(PeakGuard::kHotkeyRaiseCeilingId).c_str());
    RegisterOneHotkey(PeakGuard::kHotkeyToggleLimiterId, HotkeyDisplayForId(PeakGuard::kHotkeyToggleLimiterId).c_str());
    RegisterOneHotkey(PeakGuard::kHotkeyLowerBoostId, HotkeyDisplayForId(PeakGuard::kHotkeyLowerBoostId).c_str());
    RegisterOneHotkey(PeakGuard::kHotkeyRaiseBoostId, HotkeyDisplayForId(PeakGuard::kHotkeyRaiseBoostId).c_str());
    if (g_hotkeyStatus.empty()) {
        g_hotkeyStatus = L"hotkeys OK";
    }
}

void UnregisterLimiterHotkeys() {
    for (const auto& binding : g_hotkeys) {
        UnregisterHotKey(g_window, binding.id);
    }
}

bool HandleLimiterHotkey(WPARAM hotkeyId) {
    switch (static_cast<int>(hotkeyId)) {
    case PeakGuard::kHotkeyLowerCeilingId:
        AdjustCeilingByMilliDb(-PeakGuard::LimiterHotkeyStepMilliDb());
        ShowLimiterOverlay(L"Limiter", g_state != nullptr ? FormatDb(g_state->ceilingMilliDb) : L"Unavailable");
        return true;
    case PeakGuard::kHotkeyRaiseCeilingId:
        AdjustCeilingByMilliDb(PeakGuard::LimiterHotkeyStepMilliDb());
        ShowLimiterOverlay(L"Limiter", g_state != nullptr ? FormatDb(g_state->ceilingMilliDb) : L"Unavailable");
        return true;
    case PeakGuard::kHotkeyToggleLimiterId:
        ToggleLimiterEnabled();
        ShowLimiterOverlay(L"Limiter", (g_state != nullptr && g_state->enabled != 0) ? L"On" : L"Off");
        return true;
    case PeakGuard::kHotkeyLowerBoostId:
        AdjustBoostByMilliDb(-PeakGuard::LimiterHotkeyStepMilliDb());
        ShowLimiterOverlay(L"Soundbooster", g_state != nullptr ? FormatBoostDb(g_state->boostMilliDb) : L"Unavailable");
        return true;
    case PeakGuard::kHotkeyRaiseBoostId:
        AdjustBoostByMilliDb(PeakGuard::LimiterHotkeyStepMilliDb());
        ShowLimiterOverlay(L"Soundbooster", g_state != nullptr ? FormatBoostDb(g_state->boostMilliDb) : L"Unavailable");
        return true;
    default:
        return false;
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
        peak = std::max<LONG>(PeakGuard::kMinCeilingMilliDb, std::min<LONG>(0, peak));
        const int width = rect.right - rect.left;
        const int fill = static_cast<int>((static_cast<double>(peak - PeakGuard::kMinCeilingMilliDb) /
            static_cast<double>(-PeakGuard::kMinCeilingMilliDb)) * width);

        RECT fillRect = rect;
        fillRect.right = fillRect.left + std::max(0, std::min(width, fill));
        FillRect(dc, &fillRect, g_meterFillBrush);

        if (g_state != nullptr) {
            const int marker = static_cast<int>((static_cast<double>(g_state->ceilingMilliDb - PeakGuard::kMinCeilingMilliDb) /
                static_cast<double>(-PeakGuard::kMinCeilingMilliDb)) * width);
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

LRESULT CALLBACK OverlayProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT rect{};
        GetClientRect(window, &rect);

        HBRUSH overlayBrush = CreateSolidBrush(RGB(16, 16, 18));
        FillRect(dc, &rect, overlayBrush);
        DeleteObject(overlayBrush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kColorText);

        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(dc, font);

        RECT leftTitle{ 18, 12, (rect.right / 2) - 12, 34 };
        RECT centerDash{ (rect.right / 2) - 8, 12, (rect.right / 2) + 8, 34 };
        RECT rightTitle{ (rect.right / 2) + 12, 12, rect.right - 18, 34 };
        SetTextColor(dc, kColorMutedText);
        DrawTextW(dc, g_overlayTitle.c_str(), -1, &leftTitle, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        DrawTextW(dc, L"-", -1, &centerDash, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        SetTextColor(dc, kColorText);
        DrawTextW(dc, g_overlayValue.empty() ? L"-" : g_overlayValue.c_str(), -1, &rightTitle, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        const LONG ceiling = g_state != nullptr ? g_state->ceilingMilliDb : PeakGuard::kDefaultCeilingMilliDb;
        RECT rail{ 24, 54, rect.right - 24, 64 };
        FillRect(dc, &rail, g_railBrush);
        const int width = rail.right - rail.left;
        const int position = MilliDbToSliderPos(ceiling);
        const int fillWidth = static_cast<int>((static_cast<double>(position) / 75.0) * width);
        const int center = rail.left + width / 2;
        RECT active{ center - std::max(0, std::min(width, fillWidth)) / 2, rail.top,
            center + std::max(0, std::min(width, fillWidth)) / 2, rail.bottom };
        FillRect(dc, &active, g_activeBrush);

        FrameRect(dc, &rect, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        SelectObject(dc, oldFont);
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

    PeakGuard::SetCeilingMilliDb(g_state, SliderPosToMilliDb(SliderXToPosition(window, x)));
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

    PeakGuard::SetBoostMilliDb(g_state, BoostSliderPosToMilliDb(BoostSliderXToPosition(window, x)));
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
            ? BoostMilliDbToSliderPos(g_state != nullptr ? g_state->boostMilliDb : PeakGuard::kDefaultBoostMilliDb)
            : MilliDbToSliderPos(g_state != nullptr ? g_state->ceilingMilliDb : PeakGuard::kDefaultCeilingMilliDb);
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
    wcscpy_s(g_trayIcon.szTip, L"PeakGuard");
    Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
}

void CreateOverlayWindow() {
    g_overlay = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"PeakGuardOverlay",
        L"PeakGuard",
        WS_POPUP,
        0,
        0,
        kOverlayWidth,
        kOverlayHeight,
        nullptr,
        nullptr,
        g_instance,
        nullptr);
    if (g_overlay != nullptr) {
        SetLayeredWindowAttributes(g_overlay, 0, 236, LWA_ALPHA);
        PositionOverlay();
    }
}

void SetControlFont(HWND control) {
    if (control != nullptr && g_uiFont != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
    }
}

void CreateControls(HWND window) {
    g_tabLimiter = CreateWindowExW(0, L"BUTTON", L"Limiter", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabLimiter)), g_instance, nullptr);
    g_tabHotkeys = CreateWindowExW(0, L"BUTTON", L"Keys", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabHotkeys)), g_instance, nullptr);
    g_tabStartup = CreateWindowExW(0, L"BUTTON", L"Startup", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabStartup)), g_instance, nullptr);
    g_tabPower = CreateWindowExW(0, L"BUTTON", L"Power", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTabPower)), g_instance, nullptr);

    g_peakText = CreateWindowExW(0, L"STATIC", L"Output: -inf dBFS", WS_CHILD | WS_VISIBLE | SS_CENTER,
        16, 14, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPeakText)), g_instance, nullptr);
    g_meter = CreateWindowExW(0, L"PeakGuardMeter", nullptr, WS_CHILD | WS_VISIBLE,
        16, 38, 310, 18, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdMeter)), g_instance, nullptr);
    g_ceilingText = CreateWindowExW(0, L"STATIC", L"Ceiling: -30.0 dBFS", WS_CHILD | WS_VISIBLE | SS_CENTER,
        16, 66, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCeilingText)), g_instance, nullptr);
    g_slider = CreateWindowExW(0, L"PeakGuardSlider", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        16, 90, 310, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSlider)), g_instance, nullptr);

    g_enable = CreateWindowExW(0, L"BUTTON", L"Limiter enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 128, 160, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdEnable)), g_instance, nullptr);

    g_boostEnable = CreateWindowExW(0, L"BUTTON", L"Soundbooster enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 160, 190, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostEnable)), g_instance, nullptr);
    g_boostText = CreateWindowExW(0, L"STATIC", L"Boost: +0 dB", WS_CHILD | WS_VISIBLE | SS_CENTER,
        16, 192, 310, 20, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostText)), g_instance, nullptr);
    g_boostSlider = CreateWindowExW(0, L"PeakGuardSlider", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        16, 216, 310, 34, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBoostSlider)), g_instance, nullptr);

    g_hotkeyLowerCeiling = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyLowerCeiling)), g_instance, nullptr);
    g_hotkeyRaiseCeiling = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyRaiseCeiling)), g_instance, nullptr);
    g_hotkeyToggleLimiter = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyToggleLimiter)), g_instance, nullptr);
    g_hotkeyLowerBoost = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyLowerBoost)), g_instance, nullptr);
    g_hotkeyRaiseBoost = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyRaiseBoost)), g_instance, nullptr);
    g_hotkeyReset = CreateWindowExW(0, L"BUTTON", L"Reset hotkeys to defaults", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyReset)), g_instance, nullptr);
    g_startupToggle = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStartupToggle)), g_instance, nullptr);
    g_overlayToggle = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 100, 32, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOverlayToggle)), g_instance, nullptr);
    g_powerText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 100, 60, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPowerText)), g_instance, nullptr);

    g_statusText = CreateWindowExW(0, L"STATIC", L"Starting", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_ENDELLIPSIS,
        16, 260, 320, 40, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStatusText)), g_instance, nullptr);
    SetControlFont(g_tabLimiter);
    SetControlFont(g_tabHotkeys);
    SetControlFont(g_tabStartup);
    SetControlFont(g_tabPower);
    SetControlFont(g_peakText);
    SetControlFont(g_ceilingText);
    SetControlFont(g_enable);
    SetControlFont(g_boostEnable);
    SetControlFont(g_boostText);
    SetControlFont(g_hotkeyLowerCeiling);
    SetControlFont(g_hotkeyRaiseCeiling);
    SetControlFont(g_hotkeyToggleLimiter);
    SetControlFont(g_hotkeyLowerBoost);
    SetControlFont(g_hotkeyRaiseBoost);
    SetControlFont(g_hotkeyReset);
    SetControlFont(g_startupToggle);
    SetControlFont(g_overlayToggle);
    SetControlFont(g_powerText);
    SetControlFont(g_statusText);
    UpdateSettingsLabels();
    LayoutControls(window);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_window = window;
        CreateControls(window);
        AddTrayIcon(window);
        CreateOverlayWindow();
        g_mapping.Open();
        g_state = g_mapping.Get();
        if (g_state != nullptr) {
            PeakGuard::ApplySettings(g_state, g_settings);
        }
        RegisterLimiterHotkeys();
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
    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = PeakGuard::UiWindowMinWidthPx();
        info->ptMinTrackSize.y = PeakGuard::UiWindowMinHeightPx();
        return 0;
    }
    case WM_SIZE:
        LayoutControls(window);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (CompleteHotkeyCapture(wParam)) {
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kIdTabLimiter:
            SelectTab(PeakGuard::LimiterSettingsTab::Limiter);
            return 0;
        case kIdTabHotkeys:
            SelectTab(PeakGuard::LimiterSettingsTab::Hotkeys);
            return 0;
        case kIdTabStartup:
            SelectTab(PeakGuard::LimiterSettingsTab::Startup);
            return 0;
        case kIdTabPower:
            SelectTab(PeakGuard::LimiterSettingsTab::Power);
            return 0;
        case kIdHotkeyLowerCeiling:
            BeginHotkeyCapture(PeakGuard::kHotkeyLowerCeilingId, g_hotkeyLowerCeiling);
            return 0;
        case kIdHotkeyRaiseCeiling:
            BeginHotkeyCapture(PeakGuard::kHotkeyRaiseCeilingId, g_hotkeyRaiseCeiling);
            return 0;
        case kIdHotkeyToggleLimiter:
            BeginHotkeyCapture(PeakGuard::kHotkeyToggleLimiterId, g_hotkeyToggleLimiter);
            return 0;
        case kIdHotkeyLowerBoost:
            BeginHotkeyCapture(PeakGuard::kHotkeyLowerBoostId, g_hotkeyLowerBoost);
            return 0;
        case kIdHotkeyRaiseBoost:
            BeginHotkeyCapture(PeakGuard::kHotkeyRaiseBoostId, g_hotkeyRaiseBoost);
            return 0;
        case kIdHotkeyReset:
            ResetHotkeysToDefaults();
            SaveSettings();
            ReRegisterLimiterHotkeys();
            UpdateSettingsLabels();
            UpdateControls();
            return 0;
        case kIdStartupToggle: {
            const bool next = !g_settings.startWithWindows;
            if (SetStartWithWindows(next)) {
                g_settings.startWithWindows = next;
                SaveSettings();
            }
            UpdateSettingsLabels();
            UpdateControls();
            return 0;
        }
        case kIdOverlayToggle:
            g_settings.overlayEnabled = !g_settings.overlayEnabled;
            SaveSettings();
            UpdateSettingsLabels();
            UpdateControls();
            return 0;
        default:
            break;
        }
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
            PeakGuard::SetCeilingMilliDb(g_state, SliderPosToMilliDb(position));
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
        if (wParam == kOverlayTimer) {
            HideLimiterOverlay();
            return 0;
        }
        return 0;
    case WM_HOTKEY:
        if (HandleLimiterHotkey(wParam)) {
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
    case kShowExistingMessage:
        ShowMainWindow();
        return 0;
    case kExitExistingMessage:
        DestroyWindow(window);
        return 0;
    case WM_CLOSE:
        StopUiTimer();
        ShowWindow(window, SW_HIDE);
        return 0;
    case WM_DESTROY:
        UnregisterLimiterHotkeys();
        HideLimiterOverlay();
        if (g_overlay != nullptr) {
            DestroyWindow(g_overlay);
            g_overlay = nullptr;
        }
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
        if (g_uiFont != nullptr) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
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
    mainClass.lpszClassName = kMainWindowClassName;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    mainClass.hbrBackground = g_windowBrush;
    if (!RegisterClassW(&mainClass)) {
        return false;
    }

    WNDCLASSW meterClass{};
    meterClass.lpfnWndProc = MeterProc;
    meterClass.hInstance = g_instance;
    meterClass.lpszClassName = L"PeakGuardMeter";
    meterClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    meterClass.hbrBackground = g_panelBrush;
    if (!RegisterClassW(&meterClass)) {
        return false;
    }

    WNDCLASSW sliderClass{};
    sliderClass.lpfnWndProc = SliderProc;
    sliderClass.hInstance = g_instance;
    sliderClass.lpszClassName = L"PeakGuardSlider";
    sliderClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sliderClass.hbrBackground = g_windowBrush;
    if (!RegisterClassW(&sliderClass)) {
        return false;
    }

    WNDCLASSW overlayClass{};
    overlayClass.lpfnWndProc = OverlayProc;
    overlayClass.hInstance = g_instance;
    overlayClass.lpszClassName = L"PeakGuardOverlay";
    overlayClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    overlayClass.hbrBackground = g_panelBrush;
    return RegisterClassW(&overlayClass) != 0;
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
    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (g_singleInstanceMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kMainWindowClassName, nullptr);
        if (existing != nullptr) {
            const bool quitRequested = commandLine != nullptr && wcsstr(commandLine, L"--quit") != nullptr;
            PostMessageW(existing, quitRequested ? kExitExistingMessage : kShowExistingMessage, 0, 0);
        }
        if (g_singleInstanceMutex != nullptr) {
            CloseHandle(g_singleInstanceMutex);
            g_singleInstanceMutex = nullptr;
        }
        return 0;
    }

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
    g_uiFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    g_mapping.Open();
    g_state = g_mapping.Get();
    g_settingsPath = BuildSettingsPath();
    g_settings = LoadSettings();
    g_settings.startWithWindows = IsStartWithWindowsEnabled();
    ApplyHotkeysFromSettings();
    if (g_state != nullptr) {
        PeakGuard::ApplySettings(g_state, g_settings);
    }

    if (!RegisterWindowClasses()) {
        CoUninitialize();
        return 1;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int initialX = workArea.left + ((workArea.right - workArea.left) - kWindowWidth) / 2;
    const int initialY = workArea.top + ((workArea.bottom - workArea.top) - kWindowHeight) / 2;

    g_window = CreateWindowExW(
        WS_EX_APPWINDOW,
        kMainWindowClassName,
        L"PeakGuard",
        WS_OVERLAPPEDWINDOW,
        initialX,
        initialY,
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

    if (g_singleInstanceMutex != nullptr) {
        ReleaseMutex(g_singleInstanceMutex);
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = nullptr;
    }

    return 0;
}
