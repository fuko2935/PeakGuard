#pragma once

#include <windows.h>

#include <cmath>

namespace CodexLimiter {

constexpr LONG kStateMagic = 0x434C494D; // CLIM
constexpr LONG kStateVersion = 2;
constexpr LONG kDefaultCeilingMilliDb = -30000;
constexpr LONG kMinCeilingMilliDb = -75000;
constexpr LONG kMaxCeilingMilliDb = 0;
constexpr LONG kLinearScale = 1000000000;
constexpr LONG kMinLinearScaled = 1;
constexpr LONG kDefaultLinearScaled = 31622776; // -30 dBFS

enum class AudioMode : LONG {
    Loopback = 0
};

enum class AudioEngineState : LONG {
    Stopped = 0,
    Running = 1,
    Silence = 2,
    DeviceChanged = 3,
    Error = 4
};

struct LimiterSharedState {
    volatile LONG magic;
    volatile LONG version;
    volatile LONG enabled;
    volatile LONG ceilingMilliDb;
    volatile LONG ceilingLinearScaled;
    volatile LONG inputPeakMilliDb;
    volatile LONG outputPeakMilliDb;
    volatile LONG processCounter;
    volatile LONG audioMode;
    volatile LONG audioEngineState;
    volatile LONG sampleRate;
    volatile LONG _reserved[4];
};

inline LONG ClampMilliDb(LONG value) {
    if (value < kMinCeilingMilliDb) {
        return kMinCeilingMilliDb;
    }
    if (value > kMaxCeilingMilliDb) {
        return kMaxCeilingMilliDb;
    }
    return value;
}

inline LONG MilliDbToLinearScaled(LONG milliDb) {
    const float db = static_cast<float>(ClampMilliDb(milliDb)) / 1000.0f;
    const double linear = std::pow(10.0, static_cast<double>(db) / 20.0);
    const double scaled = linear * static_cast<double>(kLinearScale);
    if (scaled < static_cast<double>(kMinLinearScaled)) {
        return kMinLinearScaled;
    }
    if (scaled > static_cast<double>(kLinearScale)) {
        return kLinearScale;
    }
    return static_cast<LONG>(scaled + 0.5);
}

inline LONG LinearToMilliDb(float value) {
    if (!(value > 0.000001f) || !std::isfinite(value)) {
        return -120000;
    }

    const float db = 20.0f * std::log10(value);
    if (!std::isfinite(db)) {
        return -120000;
    }
    return static_cast<LONG>(db * 1000.0f);
}

inline float ScaledLinearToFloat(LONG value) {
    if (value < kMinLinearScaled) {
        value = kMinLinearScaled;
    }
    if (value > kLinearScale) {
        value = kLinearScale;
    }
    return static_cast<float>(value) / static_cast<float>(kLinearScale);
}

inline void InitializeStateFields(LimiterSharedState* state) {
    InterlockedExchange(const_cast<volatile LONG*>(&state->version), kStateVersion);
    InterlockedExchange(const_cast<volatile LONG*>(&state->enabled), 1);
    InterlockedExchange(const_cast<volatile LONG*>(&state->ceilingMilliDb), kDefaultCeilingMilliDb);
    InterlockedExchange(const_cast<volatile LONG*>(&state->ceilingLinearScaled), kDefaultLinearScaled);
    InterlockedExchange(const_cast<volatile LONG*>(&state->inputPeakMilliDb), -120000);
    InterlockedExchange(const_cast<volatile LONG*>(&state->outputPeakMilliDb), -120000);
    InterlockedExchange(const_cast<volatile LONG*>(&state->processCounter), 0);
    InterlockedExchange(const_cast<volatile LONG*>(&state->audioMode), static_cast<LONG>(AudioMode::Loopback));
    InterlockedExchange(const_cast<volatile LONG*>(&state->audioEngineState), static_cast<LONG>(AudioEngineState::Stopped));
    InterlockedExchange(const_cast<volatile LONG*>(&state->sampleRate), 0);
    for (int i = 0; i < 4; ++i) {
        InterlockedExchange(const_cast<volatile LONG*>(&state->_reserved[i]), 0);
    }
}

inline void InitializeStateIfNeeded(LimiterSharedState* state) {
    if (state == nullptr) {
        return;
    }

    if (InterlockedCompareExchange(const_cast<volatile LONG*>(&state->magic), kStateMagic, 0) == 0) {
        // Winner: we own initialization
        InitializeStateFields(state);
        return;
    }

    // Loser: someone else claimed initialization — spin-wait for them to finish
    for (int i = 0; i < 1000; ++i) {
        if (state->magic == kStateMagic && state->version == kStateVersion) {
            return; // initialization complete
        }
        // Also handle case where initializer crashed or never finished
        if (i == 999) {
            // Fallback: claim and reinitialize
            InterlockedExchange(const_cast<volatile LONG*>(&state->magic), kStateMagic);
            InitializeStateFields(state);
        }
        YieldProcessor();
    }
}

inline void SetCeilingMilliDb(LimiterSharedState* state, LONG milliDb) {
    if (state == nullptr) {
        return;
    }

    const LONG clamped = ClampMilliDb(milliDb);
    InterlockedExchange(const_cast<volatile LONG*>(&state->ceilingMilliDb), clamped);
    InterlockedExchange(const_cast<volatile LONG*>(&state->ceilingLinearScaled), MilliDbToLinearScaled(clamped));
}

class SharedLimiterMapping {
public:
    SharedLimiterMapping() = default;

    ~SharedLimiterMapping() {
        Close();
    }

    SharedLimiterMapping(const SharedLimiterMapping&) = delete;
    SharedLimiterMapping& operator=(const SharedLimiterMapping&) = delete;

    bool Open() {
        Close();
        return OpenNamed(L"Local\\CodexLimiterState");
    }

    LimiterSharedState* Get() const {
        return state_;
    }

private:
    bool OpenNamed(const wchar_t* name) {
        SECURITY_DESCRIPTOR securityDescriptor{};
        SECURITY_ATTRIBUTES securityAttributes{};
        if (InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION) &&
            SetSecurityDescriptorDacl(&securityDescriptor, TRUE, nullptr, FALSE)) {
            securityAttributes.nLength = sizeof(securityAttributes);
            securityAttributes.lpSecurityDescriptor = &securityDescriptor;
            securityAttributes.bInheritHandle = FALSE;
        }

        HANDLE mapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            securityAttributes.lpSecurityDescriptor != nullptr ? &securityAttributes : nullptr,
            PAGE_READWRITE,
            0,
            sizeof(LimiterSharedState),
            name);
        if (mapping == nullptr) {
            mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        }
        if (mapping == nullptr) {
            return false;
        }

        void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LimiterSharedState));
        if (view == nullptr) {
            CloseHandle(mapping);
            return false;
        }

        mapping_ = mapping;
        state_ = static_cast<LimiterSharedState*>(view);
        InitializeStateIfNeeded(state_);
        return true;
    }

    void Close() {
        if (state_ != nullptr) {
            UnmapViewOfFile(state_);
            state_ = nullptr;
        }
        if (mapping_ != nullptr) {
            CloseHandle(mapping_);
            mapping_ = nullptr;
        }
    }

    HANDLE mapping_ = nullptr;
    LimiterSharedState* state_ = nullptr;
};

} // namespace CodexLimiter
