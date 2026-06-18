#pragma once

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>

#include <cwctype>
#include <string>

namespace CodexLimiter {

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

} // namespace CodexLimiter
