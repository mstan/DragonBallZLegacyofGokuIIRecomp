#pragma once
#include <cstdint>
inline int (*g_runtime_bus_read_override)(std::uint32_t, std::uint32_t,
    std::uint32_t, std::uint32_t, std::uint32_t*) = nullptr;
extern "C" { inline unsigned long long g_runtime_state_epoch = 0; }
