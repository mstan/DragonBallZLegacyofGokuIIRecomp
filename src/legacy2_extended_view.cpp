#include "legacy2_extended_view.h"

#include <cstdint>
#include <cstdio>

#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_arm.h"
#include "runtime_bus_bridge.h"

extern "C" unsigned g_ws_active;
extern "C" unsigned g_ws_extra_left;
extern "C" unsigned g_ws_extra_right;

namespace legacy2 {
namespace {

// The field renderer keeps authored 64x64 tile planes in EWRAM using the
// native GBA four-screen-block layout. It copies a 32x32 camera ring into
// VRAM screen blocks 30 and 31 for BG2/BG3. Reading the authored planes here
// reveals real neighboring tiles instead of repeating that 256-pixel ring.
constexpr std::uint32_t kBg2Field = 0x020306B8u;
constexpr std::uint32_t kBg3Field = 0x0202E688u;
constexpr int kFieldTiles = 64;
constexpr std::uint32_t kCameraLeft = 0x0300109Cu;
constexpr std::uint32_t kCameraRight = 0x030010A4u;

gba::GbaBus* active_bus() {
    return gbarecomp::active_bus();
}

std::uint16_t ewram16(gba::GbaBus* bus, std::uint32_t address) {
    if (!bus || address < 0x02000000u || address + 1u >= 0x02040000u)
        return 0;
    const std::uint8_t* p = bus->ewram_ptr() + address - 0x02000000u;
    return static_cast<std::uint16_t>(p[0]) |
        static_cast<std::uint16_t>(p[1] << 8);
}

bool is_overworld(gba::GbaBus* bus) {
    if (!bus || !g_ws_active) return false;
    const std::uint16_t dispcnt = bus->io().read16(0x000u);
    if ((dispcnt & 7u) != 0u || (dispcnt & 0x1F40u) != 0x1F40u)
        return false;

    for (int bg = 0; bg < 4; ++bg) {
        const std::uint16_t cnt =
            bus->io().read16(0x008u + static_cast<std::uint32_t>(bg * 2));
        if (((cnt >> 8) & 0x1Fu) != static_cast<unsigned>(28 + bg) ||
            ((cnt >> 14) & 3u) != 0u) {
            return false;
        }
    }
    return true;
}

std::uint16_t field_entry(
    gba::GbaBus* bus, std::uint32_t base, int tile_x, int tile_y) {
    if (!bus || tile_x < 0 || tile_x >= kFieldTiles ||
        tile_y < 0 || tile_y >= kFieldTiles) {
        return 0;
    }

    const int block = (tile_y >> 5) * 2 + (tile_x >> 5);
    const int local_x = tile_x & 31;
    const int local_y = tile_y & 31;
    const std::uint32_t offset =
        static_cast<std::uint32_t>(block) * 0x800u +
        static_cast<std::uint32_t>(
            (local_y * 32 + local_x) * 2);
    return ewram16(bus, base + offset);
}

extern "C" int legacy2_tilemap_provider(
    int bg, int hardware_x, int screen_y, std::uint16_t* out_entry) {
    gba::GbaBus* bus = active_bus();
    if (!out_entry || (bg != 2 && bg != 3) || !is_overworld(bus))
        return gba::kWsTilemapUnavailable;

    const int hofs = bus->io().read16(
        0x010u + static_cast<std::uint32_t>(bg * 4)) & 0x1FF;
    const int vofs = bus->io().read16(
        0x012u + static_cast<std::uint32_t>(bg * 4)) & 0x1FF;
    const int world_x = hofs + hardware_x;
    const int world_y = vofs + screen_y;
    if (world_x < 0 || world_y < 0 ||
        world_x >= kFieldTiles * 8 || world_y >= kFieldTiles * 8) {
        return gba::kWsTilemapUnavailable;
    }

    const std::uint32_t base = bg == 2 ? kBg2Field : kBg3Field;
    *out_entry = field_entry(bus, base, world_x >> 3, world_y >> 3);
    return gba::kWsTilemapReplace;
}

extern "C" int legacy2_obj_x_provider(
    int oam_index, std::uint16_t attr0, std::uint16_t attr1,
    std::uint16_t, int* out_x) {
    gba::GbaBus* bus = active_bus();
    if (!out_x || !is_overworld(bus)) return 0;

    const int x = attr1 & 0x1FFu;
    const int y = attr0 & 0xFFu;

    // Gameplay reserves slots 0..2 for the top-left health/energy group.
    // Slot 3 is already the player, so the slot and position guards are both
    // required to keep world actors camera-relative.
    if (oam_index >= 0 && oam_index <= 2 && x < 96 && y < 32) {
        *out_x = x - static_cast<int>(g_ws_extra_left);
        return 1;
    }

    // Preserve positive authored positions in the new right margin while
    // retaining the hardware's wrapped representation for off-left objects.
    const int widened_right = 240 + static_cast<int>(g_ws_extra_right);
    *out_x = x <= widened_right ? x : x - 512;
    return 1;
}

extern "C" int legacy2_bus_read_override(
    std::uint32_t pc, std::uint32_t address, std::uint32_t width,
    std::uint32_t original, std::uint32_t* out_value) {
    gba::GbaBus* bus = active_bus();
    if (!out_value || width != 4u || !is_overworld(bus))
        return 0;

    // The field actor renderer has two rectangle-intersection paths. Widen
    // only their reads of the camera's horizontal bounds, leaving the camera
    // object itself untouched so tile streaming, scrolling, and actor world
    // positions retain native behavior.
    const bool actor_visibility_left =
        pc == 0x08007212u || pc == 0x08007AEAu;
    const bool actor_visibility_right =
        pc == 0x0800720Cu || pc == 0x08007AE4u;
    if (actor_visibility_left && address == kCameraLeft) {
        *out_value =
            original - static_cast<std::uint32_t>(g_ws_extra_left);
        return 1;
    }
    if (actor_visibility_right && address == kCameraRight) {
        *out_value =
            original + static_cast<std::uint32_t>(g_ws_extra_right);
        return 1;
    }
    return 0;
}

}  // namespace

void install_extended_view(std::uint32_t, std::uint32_t) {
    gba::g_ws_tilemap_provider = &legacy2_tilemap_provider;
    gba::g_ws_authored_margin_layers = 1;
    gba::g_ws_bg_x_provider = nullptr;
    gba::g_ws_bg_x_provider_layers = 0;
    gba::g_ws_obj_attr_x_provider = &legacy2_obj_x_provider;
    g_runtime_bus_read_override = &legacy2_bus_read_override;
    std::fprintf(stderr,
        "[legacy2:view] authored BG2/BG3 64x64 field maps + edge HUD + "
        "expanded actor visibility enabled\n");
}

}  // namespace legacy2
