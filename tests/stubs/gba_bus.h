#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace gba {
struct TestIo {
    std::array<std::uint8_t, 0x400> bytes{};
    std::uint16_t read16(std::uint32_t offset) const {
        return bytes.at(offset) | (bytes.at(offset + 1) << 8);
    }
};
struct GbaBus {
    std::array<std::uint8_t, 0x40000> ewram{};
    std::array<std::uint8_t, 0x8000> iwram{};
    std::vector<std::uint8_t> rom = std::vector<std::uint8_t>(0x40000);
    TestIo registers;
    std::size_t ewram_reads = 0;
    std::uint8_t* ewram_ptr() { ++ewram_reads; return ewram.data(); }
    std::uint8_t* iwram_ptr() { return iwram.data(); }
    const std::uint8_t* rom_ptr() const { return rom.data(); }
    std::size_t rom_size() const { return rom.size(); }
    TestIo& io() { return registers; }
};
}
