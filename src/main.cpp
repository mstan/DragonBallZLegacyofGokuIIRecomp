#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "legacy2_extended_view.h"
#include "runtime.h"

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

namespace {

void print_usage() {
    std::printf(
        "DragonBallZLegacyofGokuIIRecomp [--bios <path>] [--rom <path>] "
        "[game.toml]\n");
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    gbarecomp::RunOptions opts;
    opts.builtin_game_name = "Dragon Ball Z: The Legacy of Goku II";
    opts.builtin_rom_sha1 = "18e0715dec419f3501c301511530d2edcd590f8b";
    opts.builtin_rom_crc32 = 0x204142E1u;
    opts.mod_game_id = "dragon-ball-z-legacy-of-goku-ii-us";
    opts.mod_owns_adaptive_view = true;
    opts.max_view_width = 480;
    opts.max_resize_view_width = 480;
    opts.resize_driven_view = true;
    opts.extended_view_init = &legacy2::install_extended_view;
    opts.launcher_expose_widescreen = false;
    opts.launcher_expose_adaptive_view = false;
    opts.launcher_region = "USA";
    opts.launcher_game_config = "game.toml";

#if defined(GBAGAME_RECOMP_UI)
    std::vector<std::string> args(argv, argv + argc);
    if (game_launcher_preboot(args, opts)) return 0;
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
#else
    return gbarecomp::run_game(argc, argv, opts);
#endif
}
