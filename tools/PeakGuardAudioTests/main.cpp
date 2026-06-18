#include <windows.h>
#include <mmreg.h>

#include <iostream>
#include <string>

#include "tools/LimiterTray/AudioEndpointSelection.h"
#include "tools/LimiterTray/AudioPowerPolicy.h"
#include "tools/LimiterTray/HotkeyPolicy.h"
#include "tools/LimiterTray/LimiterSettings.h"
#include "tools/LimiterTray/StartupPolicy.h"

namespace {

int failures = 0;

void Expect(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << "\n";
        ++failures;
    }
}

WAVEFORMATEX MakeWaveFormat(WORD tag, WORD bitsPerSample) {
    WAVEFORMATEX format{};
    format.wFormatTag = tag;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.wBitsPerSample = bitsPerSample;
    format.nBlockAlign = static_cast<WORD>((format.nChannels * bitsPerSample) / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    return format;
}

} // namespace

int main() {
    Expect(CodexLimiter::IsVirtualAudioEndpointName(L"CABLE Output (VB-Audio Virtual Cable)"),
        "VB-CABLE capture endpoint is virtual");
    Expect(CodexLimiter::IsVirtualAudioEndpointName(L"VoiceMeeter Output"),
        "Voicemeeter endpoint is virtual");
    Expect(!CodexLimiter::IsVirtualAudioEndpointName(L"Kulakliklar (HAVIT H655BT PRO)"),
        "Bluetooth headset is physical");
    Expect(!CodexLimiter::IsVirtualAudioEndpointName(L"Speaker (Realtek(R) Audio)"),
        "Realtek speaker is physical");

    WAVEFORMATEX floatFormat = MakeWaveFormat(WAVE_FORMAT_IEEE_FLOAT, 32);
    WAVEFORMATEX pcmFormat = MakeWaveFormat(WAVE_FORMAT_PCM, 16);
    Expect(CodexLimiter::IsFloatPcmFormat(&floatFormat), "32-bit IEEE float is processable");
    Expect(!CodexLimiter::IsFloatPcmFormat(&pcmFormat), "16-bit PCM is not processed as float");
    Expect(!CodexLimiter::IsFloatPcmFormat(nullptr), "null format is not processable");
    Expect((CodexLimiter::CaptureStreamFlags() & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) != 0,
        "capture stream uses event callback");
    Expect((CodexLimiter::CaptureStreamFlags() & AUDCLNT_STREAMFLAGS_LOOPBACK) != 0,
        "capture stream uses render loopback instead of microphone capture");
    Expect((CodexLimiter::RenderStreamFlags() & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) != 0,
        "render stream uses event callback");
    Expect(CodexLimiter::AudioThreadWaitTimeoutMs() == INFINITE,
        "audio thread blocks until audio or control event");
    Expect(CodexLimiter::UiUpdateIntervalMs() <= 100,
        "UI meter updates smoothly enough for live output");
    Expect(CodexLimiter::UiWindowHeightPx() >= 240,
        "main window leaves room for status text");
    Expect(CodexLimiter::UiWindowMinWidthPx() >= 420,
        "main window minimum width prevents text clipping");
    Expect(CodexLimiter::UiWindowMinHeightPx() >= 420,
        "main window minimum height prevents status clipping");
    Expect(CodexLimiter::HasRestorableRenderEndpoint(L"{physical-render-endpoint-id}"),
        "shutdown restores a remembered physical render endpoint");
    Expect(!CodexLimiter::HasRestorableRenderEndpoint(L""),
        "shutdown skips default restore without a remembered endpoint");
    Expect(CodexLimiter::ShouldRestoreRenderEndpoint(WAIT_OBJECT_0, L"{physical-render-endpoint-id}"),
        "shutdown restores only after the audio thread exits");
    Expect(!CodexLimiter::ShouldRestoreRenderEndpoint(WAIT_TIMEOUT, L"{physical-render-endpoint-id}"),
        "shutdown skips restore if the audio thread is still running");
    Expect(CodexLimiter::IsActiveEndpointState(DEVICE_STATE_ACTIVE),
        "active endpoint state can trigger output retargeting");
    Expect(!CodexLimiter::IsActiveEndpointState(DEVICE_STATE_DISABLED),
        "inactive endpoint states do not trigger output retargeting");
    Expect(CodexLimiter::EndpointNotificationRequiresRetarget(nullptr, DEVICE_STATE_ACTIVE),
        "endpoint removal notifications trigger output retargeting");
    Expect(CodexLimiter::EndpointNotificationRequiresRetarget(L"{endpoint}", DEVICE_STATE_DISABLED),
        "disabled endpoint notifications trigger output retargeting");
    Expect(!CodexLimiter::EndpointNotificationRequiresRetarget(L"{endpoint}", DEVICE_STATE_ACTIVE),
        "active endpoint notifications can be evaluated before retargeting");
    Expect(CodexLimiter::PhysicalRenderEndpointScore(L"Kulaklıklar (HAVIT H655BT PRO)", Headphones) >
            CodexLimiter::PhysicalRenderEndpointScore(L"Speaker (Realtek(R) Audio)", Speakers),
        "headphones are preferred over speakers when both are active");
    Expect(CodexLimiter::PhysicalRenderEndpointScore(L"TS35505 (NVIDIA High Definition Audio)", DigitalAudioDisplayDevice) <
            CodexLimiter::PhysicalRenderEndpointScore(L"Speaker (Realtek(R) Audio)", Speakers),
        "display audio endpoints are not preferred over real speakers");
    Expect(CodexLimiter::PhysicalRenderEndpointScore(L"CABLE Input (VB-Audio Virtual Cable)", Speakers) < 0,
        "virtual render endpoints are rejected as physical outputs");
    Expect(CodexLimiter::LimiterHotkeyModifiers() == (MOD_CONTROL | MOD_ALT),
        "limiter hotkeys use Ctrl+Alt modifiers");
    Expect(CodexLimiter::kHotkeyLowerCeilingId != CodexLimiter::kHotkeyRaiseCeilingId &&
            CodexLimiter::kHotkeyRaiseCeilingId != CodexLimiter::kHotkeyToggleLimiterId,
        "limiter hotkey ids are unique");
    Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyLowerCeilingId) == 'A',
        "Ctrl+Alt+A lowers limiter ceiling");
    Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyRaiseCeilingId) == 'D',
        "Ctrl+Alt+D raises limiter ceiling");
    Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyToggleLimiterId) == 'S',
        "Ctrl+Alt+S toggles limiter");
    Expect(CodexLimiter::LimiterHotkeyStepMilliDb() == 1000,
        "limiter hotkey step is 1 dB");
    Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyLowerBoostId) == VK_F6,
        "Ctrl+Alt+F6 lowers soundbooster");
    Expect(CodexLimiter::LimiterHotkeyKey(CodexLimiter::kHotkeyRaiseBoostId) == VK_F7,
        "Ctrl+Alt+F7 raises soundbooster");
    Expect(CodexLimiter::kHotkeyLowerBoostId != CodexLimiter::kHotkeyLowerCeilingId,
        "boost hotkey ids do not overlap limiter ids");
    Expect(CodexLimiter::LimiterHotkeyDisplay(CodexLimiter::kHotkeyLowerCeilingId) == std::wstring(L"Ctrl+Alt+A"),
        "lower ceiling hotkey display is stable");
    Expect(CodexLimiter::LimiterHotkeyDisplay(CodexLimiter::kHotkeyRaiseBoostId) == std::wstring(L"Ctrl+Alt+F7"),
        "raise boost hotkey display is stable");
    CodexLimiter::HotkeyBinding duplicateBindings[] = {
        { CodexLimiter::kHotkeyLowerCeilingId, CodexLimiter::LimiterHotkeyModifiers(), 'A' },
        { CodexLimiter::kHotkeyRaiseCeilingId, CodexLimiter::LimiterHotkeyModifiers(), 'A' },
    };
    Expect(CodexLimiter::HasDuplicateLimiterHotkey(
            duplicateBindings, 2, CodexLimiter::kHotkeyLowerCeilingId,
            CodexLimiter::LimiterHotkeyModifiers(), 'A'),
        "duplicate hotkey bindings are rejected before saving");
    Expect(!CodexLimiter::HasDuplicateLimiterHotkey(
            duplicateBindings, 2, CodexLimiter::kHotkeyLowerCeilingId,
            CodexLimiter::LimiterHotkeyModifiers(), 'D'),
        "unique hotkey bindings are accepted before saving");
    Expect(CodexLimiter::IsSupportedLimiterHotkey(CodexLimiter::LimiterHotkeyModifiers(), 'A'),
        "Ctrl+Alt letter hotkeys are accepted");
    Expect(!CodexLimiter::IsSupportedLimiterHotkey(MOD_ALT, 'A'),
        "hotkey validation rejects missing Ctrl modifier");
    Expect(CodexLimiter::IsHotkeyCaptureModifierKey(VK_CONTROL),
        "hotkey capture ignores Control until the main key is pressed");
    Expect(CodexLimiter::IsHotkeyCaptureModifierKey(VK_LCONTROL),
        "hotkey capture ignores left Control");
    Expect(CodexLimiter::IsHotkeyCaptureModifierKey(VK_MENU),
        "hotkey capture ignores Alt until the main key is pressed");
    Expect(CodexLimiter::IsHotkeyCaptureModifierKey(VK_RMENU),
        "hotkey capture ignores right Alt");
    Expect(CodexLimiter::IsHotkeyCaptureModifierKey(VK_SHIFT),
        "hotkey capture ignores Shift instead of cancelling capture");
    Expect(!CodexLimiter::IsHotkeyCaptureModifierKey('A'),
        "hotkey capture treats letters as assignable keys");

    CodexLimiter::LimiterSettings defaultSettings{};
    Expect(defaultSettings.overlayEnabled, "overlay defaults enabled");
    Expect(!defaultSettings.startWithWindows, "start with Windows defaults off");
    Expect(defaultSettings.selectedTab == CodexLimiter::LimiterSettingsTab::Limiter,
        "settings default to limiter tab");
    Expect(CodexLimiter::StartupRegistryValueName() == std::wstring(L"CodexLimiter"),
        "startup registry value name is stable");
    Expect(CodexLimiter::QuoteStartupCommand(L"C:\\Apps\\LimiterTray.exe") ==
            std::wstring(L"\"C:\\Apps\\LimiterTray.exe\""),
        "startup command quotes executable path");

    CodexLimiter::LimiterSettings settings{};
    settings.enabled = false;
    settings.ceilingMilliDb = -120000;
    settings.boostEnabled = true;
    settings.boostMilliDb = 25000;
    CodexLimiter::LimiterSharedState state{};
    CodexLimiter::InitializeStateFields(&state);
    Expect(state.boostEnabled == 0, "shared state defaults booster off");
    Expect(state.boostMilliDb == CodexLimiter::kDefaultBoostMilliDb,
        "shared state defaults boost to 0 dB");
    Expect(state.boostLinearScaled == CodexLimiter::BoostMilliDbToLinearScaled(CodexLimiter::kDefaultBoostMilliDb),
        "shared state initializes linear boost");
    Expect(CodexLimiter::ClampBoostMilliDb(-1000) == CodexLimiter::kMinBoostMilliDb,
        "boost clamp rejects negative values");
    Expect(CodexLimiter::ClampBoostMilliDb(0) == 0,
        "boost clamp accepts 0 dB");
    Expect(CodexLimiter::ClampBoostMilliDb(24000) == 24000,
        "boost clamp accepts 24 dB");
    Expect(CodexLimiter::ClampBoostMilliDb(25000) == CodexLimiter::kMaxBoostMilliDb,
        "boost clamp rejects above-range values");
    Expect(CodexLimiter::BoostMilliDbToLinearScaled(0) == CodexLimiter::kLinearScale,
        "0 dB boost maps to unity gain");
    Expect(CodexLimiter::BoostMilliDbToLinearScaled(24000) == 15848931925LL,
        "24 dB boost maps to rounded 10^(24/20) scaled gain");
    CodexLimiter::ApplySettings(&state, settings);
    Expect(state.enabled == 0, "settings restore disabled state");
    Expect(state.ceilingMilliDb == CodexLimiter::kMinCeilingMilliDb,
        "settings restore clamps too-low ceiling");
    Expect(state.ceilingLinearScaled == CodexLimiter::MilliDbToLinearScaled(CodexLimiter::kMinCeilingMilliDb),
        "settings restore updates linear ceiling");
    Expect(state.boostEnabled == 1, "settings restore enabled booster state");
    Expect(state.boostMilliDb == CodexLimiter::kMaxBoostMilliDb,
        "settings restore clamps too-high boost");
    Expect(state.boostLinearScaled == CodexLimiter::BoostMilliDbToLinearScaled(CodexLimiter::kMaxBoostMilliDb),
        "settings restore updates linear boost");

    CodexLimiter::LimiterSettings readSettings = CodexLimiter::ReadSettingsFromState(&state);
    Expect(readSettings.boostEnabled, "settings read returns booster enabled state");
    Expect(readSettings.boostMilliDb == CodexLimiter::kMaxBoostMilliDb,
        "settings read clamps boost value");

    if (failures != 0) {
        return 1;
    }

    std::cout << "LimiterAudioTests passed\n";
    return 0;
}
