#pragma once

#include <windows.h>
#include <audioclient.h>

namespace CodexLimiter {

constexpr DWORD CaptureStreamFlags() {
    return AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK;
}

constexpr DWORD RenderStreamFlags() {
    return AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
}

constexpr DWORD AudioThreadWaitTimeoutMs() {
    return INFINITE;
}

constexpr UINT UiUpdateIntervalMs() {
    return 100;
}

constexpr int UiWindowHeightPx() {
    return 320;
}

} // namespace CodexLimiter
