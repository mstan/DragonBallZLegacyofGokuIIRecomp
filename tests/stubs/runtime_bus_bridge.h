#pragma once
#include "gba_bus.h"
#include "gba_ppu.h"
extern "C" { inline unsigned long long g_runtime_vblank_starts = 0; }
namespace gbarecomp {
inline gba::GbaBus* test_bus = nullptr;
inline gba::GbaPpu* test_ppu = nullptr;
inline gba::GbaBus* active_bus() { return test_bus; }
inline gba::GbaPpu* active_ppu() { return test_ppu; }
}
