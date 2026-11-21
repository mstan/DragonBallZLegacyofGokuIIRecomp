#pragma once
#include <cstdint>
namespace gba {
struct GbaPpu {
    std::uint64_t frame = 1;
    std::uint64_t frame_count() const { return frame; }
};
enum { kWsTilemapUnavailable = -1, kWsTilemapReplace = 1 };
extern "C" {
inline int (*g_ws_tilemap_provider)(int, int, int, std::uint16_t*) = nullptr;
inline int (*g_ws_bg_x_provider)(int, int, int, int*) = nullptr;
inline unsigned g_ws_bg_x_provider_layers = 0;
inline unsigned g_ws_authored_margin_layers = 0;
inline int (*g_ws_obj_attr_x_provider)(int, std::uint16_t, std::uint16_t, std::uint16_t, int*) = nullptr;
}
#define GBARECOMP_WIDE_BG_SAMPLER_API 1
struct WsBgXSpan { int begin, end, source_x, step, action; };
using WsBgXRowProvider = int (*)(int, int, int, int, WsBgXSpan*, int);
inline WsBgXRowProvider published_row = nullptr;
inline bool published_tile_invariance = false;
inline void publish_read_only_wide_bg_providers(WsBgXRowProvider row, bool invariant) {
    published_row = row; published_tile_invariance = invariant;
}
}
