#pragma once

#include <string>

namespace PeakGuard {

inline std::wstring StartupRegistryValueName() {
    return L"PeakGuard";
}

inline std::wstring QuoteStartupCommand(const std::wstring& executablePath) {
    return L"\"" + executablePath + L"\"";
}

} // namespace PeakGuard
