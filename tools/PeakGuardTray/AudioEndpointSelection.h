#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

#include <cwctype>
#include <string>

namespace PeakGuard {

inline std::wstring LowerEndpointName(std::wstring value) {
    for (wchar_t& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

inline bool ContainsEndpointToken(const std::wstring& name, const wchar_t* token) {
    return name.find(token) != std::wstring::npos;
}

inline bool IsVirtualAudioEndpointName(const std::wstring& endpointName) {
    const std::wstring name = LowerEndpointName(endpointName);
    return ContainsEndpointToken(name, L"vb-audio") ||
        ContainsEndpointToken(name, L"vb cable") ||
        ContainsEndpointToken(name, L"vb-cable") ||
        ContainsEndpointToken(name, L"virtual cable") ||
        ContainsEndpointToken(name, L"virtual audio") ||
        ContainsEndpointToken(name, L"cable input") ||
        ContainsEndpointToken(name, L"cable output") ||
        ContainsEndpointToken(name, L"voicemeeter") ||
        ContainsEndpointToken(name, L"blackhole") ||
        ContainsEndpointToken(name, L"codex limiter");
}

inline bool HasRestorableRenderEndpoint(const std::wstring& endpointId) {
    return !endpointId.empty();
}

inline bool ShouldRestoreRenderEndpoint(DWORD waitResult, const std::wstring& endpointId) {
    return waitResult == WAIT_OBJECT_0 && HasRestorableRenderEndpoint(endpointId);
}

inline bool IsActiveEndpointState(DWORD state) {
    return state == DEVICE_STATE_ACTIVE;
}

inline bool EndpointNotificationRequiresRetarget(LPCWSTR deviceId, DWORD state) {
    return deviceId == nullptr || state != DEVICE_STATE_ACTIVE;
}

inline int PhysicalRenderEndpointScore(const std::wstring& endpointName, int formFactor) {
    if (IsVirtualAudioEndpointName(endpointName)) {
        return -1000;
    }
    if (formFactor == Headphones || formFactor == Headset) {
        return 300;
    }
    if (formFactor == Speakers) {
        return 200;
    }
    if (formFactor == DigitalAudioDisplayDevice || formFactor == SPDIF ||
        formFactor == UnknownDigitalPassthrough) {
        return -100;
    }

    const std::wstring name = LowerEndpointName(endpointName);
    if (ContainsEndpointToken(name, L"headphone") ||
        ContainsEndpointToken(name, L"headphones") ||
        ContainsEndpointToken(name, L"kulaklik") ||
        ContainsEndpointToken(name, L"kulaklık") ||
        ContainsEndpointToken(name, L"headset") ||
        ContainsEndpointToken(name, L"havit")) {
        return 300;
    }
    if (ContainsEndpointToken(name, L"speaker") ||
        ContainsEndpointToken(name, L"hoparlör") ||
        ContainsEndpointToken(name, L"hoparlor")) {
        return 200;
    }
    if (ContainsEndpointToken(name, L"nvidia") ||
        ContainsEndpointToken(name, L"amd high definition") ||
        ContainsEndpointToken(name, L"digital") ||
        ContainsEndpointToken(name, L"dijital") ||
        ContainsEndpointToken(name, L"display") ||
        ContainsEndpointToken(name, L"monitor")) {
        return -100;
    }
    return 50;
}

inline bool IsFloatPcmFormat(const WAVEFORMATEX* format) {
    if (format == nullptr) {
        return false;
    }

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return format->wBitsPerSample == 32;
    }

    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format->cbSize >= (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))) {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT &&
            format->wBitsPerSample == 32;
    }

    return false;
}

} // namespace PeakGuard
