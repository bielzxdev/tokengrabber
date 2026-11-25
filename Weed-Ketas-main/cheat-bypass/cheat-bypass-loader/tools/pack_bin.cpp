// pack_bin.cpp — keyed rolling transform for raw bytes (loader.bin -> loader.pak.bin)
// cl /O2 /EHsc /std:c++17 /nologo pack_bin.cpp /Fe:pack_bin.exe
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <vector>

namespace
{
    std::uint64_t InitializeState(std::uint32_t key, std::size_t length)
    {
        std::uint64_t state = 0xD6E8FEB86659FD93ull;
        state ^= (static_cast<std::uint64_t>(key) << 32) | key;
        state ^= static_cast<std::uint64_t>(key) * 0x9E3779B185EBCA87ull;
        state ^= static_cast<std::uint64_t>(length) * 0xC2B2AE3D27D4EB4Full;

        state ^= state >> 30;
        state *= 0xBF58476D1CE4E5B9ull;
        state ^= state >> 27;
        state *= 0x94D049BB133111EBull;
        state ^= state >> 31;
        return state;
    }

    std::uint64_t NextWord(std::uint64_t& state)
    {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 0x2545F4914F6CDD1Dull;
    }

    std::uint8_t RotateLeft8(std::uint8_t value, unsigned int count)
    {
        if (count == 0) return value;
        return static_cast<std::uint8_t>((value << count) | (value >> (8u - count)));
    }

    std::uint8_t MaskByte(std::uint64_t word, std::size_t index)
    {
        const unsigned int shift = static_cast<unsigned int>((index & 7u) * 8u);
        std::uint8_t mask = static_cast<std::uint8_t>((word >> shift) & 0xFFu);
        const std::uint64_t position = static_cast<std::uint64_t>(index);
        mask ^= static_cast<std::uint8_t>((position * 0x9Du + (position >> 8)) & 0xFFu);
        return mask;
    }
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "usage: pack_bin <in.bin> <key_u32> <out.bin>\n");
        return 1;
    }
    std::FILE* fi = std::fopen(argv[1], "rb");
    if (!fi) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::fseek(fi, 0, SEEK_END);
    long n = std::ftell(fi);
    std::fseek(fi, 0, SEEK_SET);
    if (n <= 0) { std::fclose(fi); return 3; }
    std::vector<uint8_t> buf(n);
    std::fread(buf.data(), 1, n, fi);
    std::fclose(fi);

    uint32_t key = (uint32_t)std::strtoul(argv[2], nullptr, 0);
    if (!key) key = 0xA3C5F17B;

    std::uint64_t state = InitializeState(key, buf.size());
    for (std::size_t i = 0; i < buf.size(); ++i) {
        const std::uint64_t word = NextWord(state);
        const unsigned int rotation = static_cast<unsigned int>((word >> 61) & 7u);
        const std::uint8_t value = static_cast<std::uint8_t>(buf[i] ^ MaskByte(word, i));
        buf[i] = RotateLeft8(value, rotation);
    }

    std::FILE* fo = std::fopen(argv[3], "wb");
    if (!fo) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 4; }
    std::fwrite(buf.data(), 1, n, fo);
    std::fclose(fo);

    std::fprintf(stderr, "pack_bin: %ld bytes -> %s (key=0x%08X)\n", n, argv[3], key);
    return 0;
}
