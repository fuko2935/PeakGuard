#include <windows.h>
#include <mmreg.h>

#include <iostream>
#include <string>

#include "tools/PeakGuardTray/AudioEndpointSelection.h"
#include "tools/PeakGuardTray/AudioPowerPolicy.h"
#include "tools/PeakGuardTray/HotkeyPolicy.h"
#include "tools/PeakGuardTray/LimiterSettings.h"
#include "tools/PeakGuardTray/StartupPolicy.h"

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
    Expect(PeakGuard::IsVirtualAudioEndpointName(L"CABLE Output (VB-Audio Virtual Cable)"),
        "VB-CABLE capture endpoint is virtual");
    Expect(PeakGuard::IsVirtualAudioEndpointName(L"VoiceMeeter Output"),
        "Voicemeeter endpoint is virtual");
    Expect(!PeakGuard::IsVirtualAudioEndpointName(L"Kulakliklar (HAVIT H655BT PRO)"),
        "Bluetooth headset is physical");
    Expect(!PeakGuard::IsVirtualAudioEndpointName(L"Speaker (Realtek(R) Audio)"),
        "Realtek speaker is physical");

    WAVEFORMATEX floatFormat = MakeWaveFormat(WAVE_FORMAT_IEEE_FLOAT, 32);
    WAVEFORMATEX pcmFormat = MakeWaveFormat(WAVE_FORMAT_PCM, 16);
    Expect(PeakGuard::IsFloatPcmFormat(&floatFormat), "32-bit IEEE float is processable");
    Expect(!PeakGuard::IsFloatPcmFormat(&pcmFormat), "16-bit PCM is not processed as float");
    Expect(!PeakGuard::IsFloatPcmFormat(nullptr), "null format is not processable");
    Expect((PeakGuard::CaptureStreamFlags() & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) != 0,
        "capture stream uses event callback");
    Expect((PeakGuard::CaptureStreamFlags() & AUDCLNT_STREAMFLAGS_LOOPBACK) != 0,
        "capture stream uses render loopback instead of microphone capture");
    Expect((PeakGuard::RenderStreamFlags() & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) != 0,
        "render stream uses event callback");
    Expect(PeakGuard::AudioThreadWaitTimeoutMs() == INFINITE,
        "audio thread blocks until audio or control event");
    Expect(PeakGuard::UiUpdateIntervalMs() <= 100,
        "UI meter updates smoothly enough for live output");
    Expect(PeakGuard::UiWindowHeightPx() >= 240,
        "main window leaves room for status text");
    Expect(PeakGuard::UiWindowMinWidthPx() >= 420,
        "main window minimum width prevents text clipping");
    Expect(PeakGuard::UiWindowMinHeightPx() >= 420,
        "main window minimum height prevents status clipping");
    Expect(PeakGuard::HasRestorableRenderEndpoint(L"{physical-render-endpoint-id}"),
        "shutdown restores a remembered physical render endpoint");
    Expect(!PeakGuard::HasRestorableRenderEndpoint(L""),
        "shutdown skips default restore without a remembered endpoint");
    Expect(PeakGuard::ShouldRestoreRenderEndpoint(WAIT_OBJECT_0, L"{physical-render-endpoint-id}"),
        "shutdown restores only after the audio thread exits");
    Expect(!PeakGuard::ShouldRestoreRenderEndpoint(WAIT_TIMEOUT, L"{physical-render-endpoint-id}"),
        "shutdown skips restore if the audio thread is still running");
    Expect(PeakGuard::IsActiveEndpointState(DEVICE_STATE_ACTIVE),
        "active endpoint state can trigger output retargeting");
    Expect(!PeakGuard::IsActiveEndpointState(DEVICE_STATE_DISABLED),
        "inactive endpoint states do not trigger output retargeting");
    Expect(PeakGuard::EndpointNotificationRequiresRetarget(nullptr, DEVICE_STATE_ACTIVE),
        "endpoint removal notifications trigger output retargeting");
    Expect(PeakGuard::EndpointNotificationRequiresRetarget(L"{endpoint}", DEVICE_STATE_DISABLED),
        "disabled endpoint notifications trigger output retargeting");
    Expect(!PeakGuard::EndpointNotificationRequiresRetarget(L"{endpoint}", DEVICE_STATE_ACTIVE),
        "active endpoint notifications can be evaluated before retargeting");
    Expect(PeakGuard::PhysicalRenderEndpointScore(L"Kulaklıklar (HAVIT H655BT PRO)", Headphones) >
            PeakGuard::PhysicalRenderEndpointScore(L"Speaker (Realtek(R) Audio)", Speakers),
        "headphones are preferred over speakers when both are active");
    Expect(PeakGuard::PhysicalRenderEndpointScore(L"TS35505 (NVIDIA High Definition Audio)", DigitalAudioDisplayDevice) <
            PeakGuard::PhysicalRenderEndpointScore(L"Speaker (Realtek(R) Audio)", Speakers),
        "display audio endpoints are not preferred over real speakers");
    Expect(PeakGuard::PhysicalRenderEndpointScore(L"CABLE Input (VB-Audio Virtual Cable)", Speakers) < 0,
        "virtual render endpoints are rejected as physical outputs");
    Expect(PeakGuard::LimiterHotkeyModifiers() == (MOD_CONTROL | MOD_ALT),
        "limiter hotkeys use Ctrl+Alt modifiers");
    Expect(PeakGuard::kHotkeyLowerCeilingId != PeakGuard::kHotkeyRaiseCeilingId &&
            PeakGuard::kHotkeyRaiseCeilingId != PeakGuard::kHotkeyToggleLimiterId,
        "limiter hotkey ids are unique");
    Expect(PeakGuard::LimiterHotkeyKey(PeakGuard::kHotkeyLowerCeilingId) == 'A',
        "Ctrl+Alt+A lowers limiter ceiling");
    Expect(PeakGuard::LimiterHotkeyKey(PeakGuard::kHotkeyRaiseCeilingId) == 'D',
        "Ctrl+Alt+D raises limiter ceiling");
    Expect(PeakGuard::LimiterHotkeyKey(PeakGuard::kHotkeyToggleLimiterId) == 'S',
        "Ctrl+Alt+S toggles limiter");
    Expect(PeakGuard::LimiterHotkeyStepMilliDb() == 1000,
        "limiter hotkey step is 1 dB");
    Expect(PeakGuard::LimiterHotkeyKey(PeakGuard::kHotkeyLowerBoostId) == VK_F6,
        "Ctrl+Alt+F6 lowers soundbooster");
    Expect(PeakGuard::LimiterHotkeyKey(PeakGuard::kHotkeyRaiseBoostId) == VK_F7,
        "Ctrl+Alt+F7 raises soundbooster");
    Expect(PeakGuard::kHotkeyLowerBoostId != PeakGuard::kHotkeyLowerCeilingId,
        "boost hotkey ids do not overlap limiter ids");
    Expect(PeakGuard::LimiterHotkeyDisplay(PeakGuard::kHotkeyLowerCeilingId) == std::wstring(L"Ctrl+Alt+A"),
        "lower ceiling hotkey display is stable");
    Expect(PeakGuard::LimiterHotkeyDisplay(PeakGuard::kHotkeyRaiseBoostId) == std::wstring(L"Ctrl+Alt+F7"),
        "raise boost hotkey display is stable");
    PeakGuard::HotkeyBinding duplicateBindings[] = {
        { PeakGuard::kHotkeyLowerCeilingId, PeakGuard::LimiterHotkeyModifiers(), 'A' },
        { PeakGuard::kHotkeyRaiseCeilingId, PeakGuard::LimiterHotkeyModifiers(), 'A' },
    };
    Expect(PeakGuard::HasDuplicateLimiterHotkey(
            duplicateBindings, 2, PeakGuard::kHotkeyLowerCeilingId,
            PeakGuard::LimiterHotkeyModifiers(), 'A'),
        "duplicate hotkey bindings are rejected before saving");
    Expect(!PeakGuard::HasDuplicateLimiterHotkey(
            duplicateBindings, 2, PeakGuard::kHotkeyLowerCeilingId,
            PeakGuard::LimiterHotkeyModifiers(), 'D'),
        "unique hotkey bindings are accepted before saving");
    Expect(PeakGuard::IsSupportedLimiterHotkey(PeakGuard::LimiterHotkeyModifiers(), 'A'),
        "Ctrl+Alt letter hotkeys are accepted");
    Expect(!PeakGuard::IsSupportedLimiterHotkey(MOD_ALT, 'A'),
        "hotkey validation rejects missing Ctrl modifier");
    Expect(PeakGuard::IsHotkeyCaptureModifierKey(VK_CONTROL),
        "hotkey capture ignores Control until the main key is pressed");
    Expect(PeakGuard::IsHotkeyCaptureModifierKey(VK_LCONTROL),
        "hotkey capture ignores left Control");
    Expect(PeakGuard::IsHotkeyCaptureModifierKey(VK_MENU),
        "hotkey capture ignores Alt until the main key is pressed");
    Expect(PeakGuard::IsHotkeyCaptureModifierKey(VK_RMENU),
        "hotkey capture ignores right Alt");
    Expect(PeakGuard::IsHotkeyCaptureModifierKey(VK_SHIFT),
        "hotkey capture ignores Shift instead of cancelling capture");
    Expect(!PeakGuard::IsHotkeyCaptureModifierKey('A'),
        "hotkey capture treats letters as assignable keys");

    PeakGuard::LimiterSettings defaultSettings{};
    Expect(defaultSettings.overlayEnabled, "overlay defaults enabled");
    Expect(!defaultSettings.startWithWindows, "start with Windows defaults off");
    Expect(defaultSettings.selectedTab == PeakGuard::LimiterSettingsTab::Limiter,
        "settings default to limiter tab");
    Expect(PeakGuard::StartupRegistryValueName() == std::wstring(L"PeakGuard"),
        "startup registry value name is stable");
    Expect(PeakGuard::QuoteStartupCommand(L"C:\\Apps\\PeakGuardTray.exe") ==
            std::wstring(L"\"C:\\Apps\\PeakGuardTray.exe\""),
        "startup command quotes executable path");

    PeakGuard::LimiterSettings settings{};
    settings.enabled = false;
    settings.ceilingMilliDb = -120000;
    settings.boostEnabled = true;
    settings.boostMilliDb = 25000;
    PeakGuard::PeakGuardSharedState state{};
    PeakGuard::InitializeStateFields(&state);
    Expect(state.boostEnabled == 0, "shared state defaults booster off");
    Expect(state.boostMilliDb == PeakGuard::kDefaultBoostMilliDb,
        "shared state defaults boost to 0 dB");
    Expect(state.boostLinearScaled == PeakGuard::BoostMilliDbToLinearScaled(PeakGuard::kDefaultBoostMilliDb),
        "shared state initializes linear boost");
    Expect(PeakGuard::ClampBoostMilliDb(-1000) == PeakGuard::kMinBoostMilliDb,
        "boost clamp rejects negative values");
    Expect(PeakGuard::ClampBoostMilliDb(0) == 0,
        "boost clamp accepts 0 dB");
    Expect(PeakGuard::ClampBoostMilliDb(24000) == 24000,
        "boost clamp accepts 24 dB");
    Expect(PeakGuard::ClampBoostMilliDb(25000) == PeakGuard::kMaxBoostMilliDb,
        "boost clamp rejects above-range values");
    Expect(PeakGuard::BoostMilliDbToLinearScaled(0) == PeakGuard::kLinearScale,
        "0 dB boost maps to unity gain");
    Expect(PeakGuard::BoostMilliDbToLinearScaled(24000) == 15848931925LL,
        "24 dB boost maps to rounded 10^(24/20) scaled gain");
    PeakGuard::ApplySettings(&state, settings);
    Expect(state.enabled == 0, "settings restore disabled state");
    Expect(state.ceilingMilliDb == PeakGuard::kMinCeilingMilliDb,
        "settings restore clamps too-low ceiling");
    Expect(state.ceilingLinearScaled == PeakGuard::MilliDbToLinearScaled(PeakGuard::kMinCeilingMilliDb),
        "settings restore updates linear ceiling");
    Expect(state.boostEnabled == 1, "settings restore enabled booster state");
    Expect(state.boostMilliDb == PeakGuard::kMaxBoostMilliDb,
        "settings restore clamps too-high boost");
    Expect(state.boostLinearScaled == PeakGuard::BoostMilliDbToLinearScaled(PeakGuard::kMaxBoostMilliDb),
        "settings restore updates linear boost");

    PeakGuard::LimiterSettings readSettings = PeakGuard::ReadSettingsFromState(&state);
    Expect(readSettings.boostEnabled, "settings read returns booster enabled state");
    Expect(readSettings.boostMilliDb == PeakGuard::kMaxBoostMilliDb,
        "settings read clamps boost value");

    if (failures != 0) {
        return 1;
    }

    std::cout << "PeakGuardAudioTests passed\n";
    return 0;
}
