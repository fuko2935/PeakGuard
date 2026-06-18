#include <windows.h>
#include <mmreg.h>

#include <iostream>

#include "tools/LimiterTray/AudioEndpointSelection.h"
#include "tools/LimiterTray/AudioPowerPolicy.h"
#include "tools/LimiterTray/LimiterSettings.h"

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
    Expect(CodexLimiter::HasRestorableRenderEndpoint(L"{physical-render-endpoint-id}"),
        "shutdown restores a remembered physical render endpoint");
    Expect(!CodexLimiter::HasRestorableRenderEndpoint(L""),
        "shutdown skips default restore without a remembered endpoint");
    Expect(CodexLimiter::ShouldRestoreRenderEndpoint(WAIT_OBJECT_0, L"{physical-render-endpoint-id}"),
        "shutdown restores only after the audio thread exits");
    Expect(!CodexLimiter::ShouldRestoreRenderEndpoint(WAIT_TIMEOUT, L"{physical-render-endpoint-id}"),
        "shutdown skips restore if the audio thread is still running");

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
