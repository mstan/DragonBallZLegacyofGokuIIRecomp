#include <cstdio>
#include <stdexcept>
#ifndef LEGACY2_VIEW_SOURCE
#define LEGACY2_VIEW_SOURCE "../src/legacy2_extended_view.cpp"
#endif
#include LEGACY2_VIEW_SOURCE

extern "C" {
unsigned g_ws_active = 1, g_ws_extra_left = 40, g_ws_extra_right = 40;
}

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template <class Buffer> void put16(Buffer& data, unsigned offset, unsigned value) {
    data.at(offset) = value; data.at(offset + 1) = value >> 8;
}
template <class Buffer> void put32(Buffer& data, unsigned offset, unsigned value) {
    put16(data, offset, value); put16(data, offset + 2, value >> 16);
}
unsigned object(int layer) { return 0x1000u + layer * 0x3000u; }
void add_layer(gba::GbaBus& bus, int layer) {
    const unsigned obj = object(layer), resource = 0x28000u + layer * 0x100u;
    const unsigned descriptor = 0x30000u + layer * 0x1000u;
    put32(bus.ewram, obj + 8, 0x08023868u);
    put32(bus.ewram, obj + 0x10, 0x08000000u + resource);
    for (unsigned slot = 0; slot < 4; ++slot) bus.ewram[obj + 0x24 + slot] = 0xff;
    put32(bus.rom, resource, 0x080056A1u);
    put32(bus.rom, resource + 0x10, static_cast<unsigned>(layer) << 16);
    bus.rom[resource + 0x14] = bus.rom[resource + 0x15] = 1;
    put32(bus.rom, resource + 0x18, 0x08000000u + descriptor);
    put32(bus.rom, descriptor, 0);
    put32(bus.rom, descriptor + 4, 0x800);
    for (unsigned tile = 0; tile < 1024; ++tile) put16(bus.rom, descriptor + 8 + tile * 2, layer + 1);
}
}

int main() try {
    gba::GbaBus bus;
    gba::GbaPpu ppu;
    gbarecomp::test_bus = &bus; gbarecomp::test_ppu = &ppu;
    put16(bus.registers.bytes, 0, 0x1f40);
    for (int layer = 0; layer < 4; ++layer) {
        put16(bus.registers.bytes, 8 + layer * 2, (28 + layer) << 8);
        if (layer) add_layer(bus, layer);
    }
    legacy2::install_extended_view(40, 40);
    legacy2::resolve_field_objects(&bus);
    require(legacy2::g_field_objects[0] == 0, "missing field layer remains absent");
    const auto reads = bus.ewram_reads;
    legacy2::resolve_field_objects(&bus);
    require(bus.ewram_reads - reads < 16, "missing layer repeated a whole-EWRAM scan in the same frame");
    add_layer(bus, 0); ++ppu.frame;
    legacy2::resolve_field_objects(&bus);
    require(legacy2::g_field_objects[0] != 0, "next frame did not discover a newly loaded layer");
    put32(bus.ewram, object(0) + 8, 0); ++g_runtime_state_epoch;
    legacy2::resolve_field_objects(&bus);
    require(legacy2::g_field_objects[0] == 0, "same-frame state restore retained an old layer");
    add_layer(bus, 0); ++g_runtime_state_epoch;
    legacy2::resolve_field_objects(&bus);
    require(legacy2::g_field_objects[0] != 0, "restore did not discover the replacement field");
    require(gba::published_row && gba::published_tile_invariance, "existing shared tile-span provider was not published");
    const int shift = legacy2::calculate_view_shift(&bus);
    require(shift != 0, "framing control must require a real shift");
    int x = 0;
    require(gba::g_ws_bg_x_provider(0, 12, 7, &x) == 1 && x == 12 - 40 + shift,
            "framing depends on a preceding BG3 first pixel");
    gba::WsBgXSpan spans[8];
    require(gba::published_row(0, 7, 320, 40, spans, 8) == 1 &&
            spans[0].begin == 0 && spans[0].end == 320 && spans[0].source_x == -40 + shift && spans[0].step == 1,
            "row mapping differs from the scalar sampler");
    std::uint32_t value = 0;
    require(g_runtime_bus_read_override(0x08007212u, 0x0300109cu, 4, 1000, &value) == 1 &&
            value == 1000 - 40 + static_cast<unsigned>(shift), "actor framing differs from background framing");
    bus.ewram[object(2) + 0x24] = 0;
    put16(bus.ewram, object(2) + 0x30 + 16, 0x123);
    std::uint16_t entry = 0;
    for (int pixel = 64; pixel < 72; ++pixel)
        require(gba::g_ws_tilemap_provider(2, pixel, 0, &entry) == gba::kWsTilemapReplace && entry == 0x123,
                "entry varies inside an authored 8x8 cell");
    put16(bus.ewram, object(2) + 0x30 + 16, 0x456);
    require(gba::g_ws_tilemap_provider(2, 64, 0, &entry) == gba::kWsTilemapReplace && entry == 0x456,
            "live native chunk writes were frozen by the cache");
    const auto before = bus.ewram;
    (void)gba::published_row(1, 1, 320, 40, spans, 8);
    (void)gba::g_ws_bg_x_provider(1, 3, 1, &x);
    require(bus.ewram == before, "presentation wrote guest memory");
    put32(bus.ewram, object(3) + 8, 0); ++g_runtime_state_epoch;
    require(gba::published_row(0, 0, 320, 40, spans, 8) == 1 && legacy2::g_field_objects[3] == 0,
            "row preparation requires BG3 to exist");
    std::puts("PASS missing-layer discovery, frame/restore invalidation, pure framing, row/tile contract and live chunks");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1;
}
