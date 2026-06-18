#include <windows.h>

#include <cwchar>
#include <iostream>
#include <string>

#include "common/PeakGuardSharedState.h"

namespace {

const wchar_t* AudioEngineStateName(LONG state) {
    switch (state) {
    case 0: return L"Stopped";
    case 1: return L"Running";
    case 2: return L"Silence";
    case 3: return L"DeviceChanged";
    case 4: return L"Error";
    default: return L"Unknown";
    }
}

void PrintState(PeakGuard::PeakGuardSharedState* state) {
    if (state == nullptr) {
        std::wcout << L"State: unavailable\n";
        return;
    }

    std::wcout << L"magic=" << state->magic << L"\n";
    std::wcout << L"version=" << state->version << L"\n";
    std::wcout << L"enabled=" << state->enabled << L"\n";
    std::wcout << L"ceilingMilliDb=" << state->ceilingMilliDb << L"\n";
    std::wcout << L"ceilingLinearScaled=" << state->ceilingLinearScaled << L"\n";
    std::wcout << L"inputPeakMilliDb=" << state->inputPeakMilliDb << L"\n";
    std::wcout << L"outputPeakMilliDb=" << state->outputPeakMilliDb << L"\n";
    std::wcout << L"processCounter=" << state->processCounter << L"\n";
    if (state->version >= 2) {
        std::wcout << L"audioMode=" << state->audioMode << L"\n";
        std::wcout << L"audioEngineState=" << AudioEngineStateName(state->audioEngineState) << L"\n";
        std::wcout << L"sampleRate=" << state->sampleRate << L"\n";
    }
    if (state->version >= 3) {
        std::wcout << L"boostEnabled=" << state->boostEnabled << L"\n";
        std::wcout << L"boostMilliDb=" << state->boostMilliDb << L"\n";
        std::wcout << L"boostLinearScaled=" << state->boostLinearScaled << L"\n";
    }
}

bool TryParseLong(const wchar_t* value, LONG* result) {
    wchar_t* end = nullptr;
    long parsed = std::wcstol(value, &end, 10);
    if (value == end || *end != L'\0') {
        return false;
    }
    *result = static_cast<LONG>(parsed);
    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    PeakGuard::SharedLimiterMapping mapping;
    if (!mapping.Open()) {
        std::wcerr << L"Failed to open limiter state mapping.\n";
        return 1;
    }

    auto* state = mapping.Get();
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--enable") {
            InterlockedExchange(const_cast<volatile LONG*>(&state->enabled), 1);
        } else if (arg == L"--disable") {
            InterlockedExchange(const_cast<volatile LONG*>(&state->enabled), 0);
        } else if (arg == L"--boost-enable") {
            InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), 1);
        } else if (arg == L"--boost-disable") {
            InterlockedExchange(const_cast<volatile LONG*>(&state->boostEnabled), 0);
        } else if (arg == L"--boost-millidb") {
            if (i + 1 >= argc) {
                std::wcerr << L"--boost-millidb requires a value.\n";
                return 2;
            }
            LONG value = 0;
            if (!TryParseLong(argv[++i], &value)) {
                std::wcerr << L"Invalid boost value.\n";
                return 2;
            }
            PeakGuard::SetBoostMilliDb(state, value);
        } else if (arg == L"--ceiling-millidb") {
            if (i + 1 >= argc) {
                std::wcerr << L"--ceiling-millidb requires a value.\n";
                return 2;
            }
            LONG value = 0;
            if (!TryParseLong(argv[++i], &value)) {
                std::wcerr << L"Invalid ceiling value.\n";
                return 2;
            }
            PeakGuard::SetCeilingMilliDb(state, value);
        } else {
            std::wcerr << L"Unknown argument: " << arg << L"\n";
            return 2;
        }
    }

    PrintState(state);
    return 0;
}
