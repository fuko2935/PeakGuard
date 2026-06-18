#pragma once

#include <string>

namespace CodexLimiter {

inline std::wstring StartupRegistryValueName() {
    return L"CodexLimiter";
}

inline std::wstring QuoteStartupCommand(const std::wstring& executablePath) {
    return L"\"" + executablePath + L"\"";
}

} // namespace CodexLimiter
