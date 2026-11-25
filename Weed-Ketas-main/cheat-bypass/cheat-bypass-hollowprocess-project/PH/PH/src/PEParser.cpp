#include "../include/PEParser.h"

namespace Hollowing {

    bool PEParser::Parse(const std::vector<BYTE>& data, PE_INFO& peInfo) {
        if (data.size() < sizeof(IMAGE_DOS_HEADER)) return false;

        peInfo.data = data;
        peInfo.size = (DWORD)data.size();

        peInfo.dosHeader = (PIMAGE_DOS_HEADER)peInfo.data.data();
        if (peInfo.dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

        peInfo.ntHeaders32 = (PIMAGE_NT_HEADERS32)(peInfo.data.data() + peInfo.dosHeader->e_lfanew);
        if (peInfo.ntHeaders32->Signature != IMAGE_NT_SIGNATURE) return false;

        if (peInfo.ntHeaders32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            peInfo.is64Bit = true;
            peInfo.ntHeaders64 = (PIMAGE_NT_HEADERS64)peInfo.ntHeaders32;
        } else if (peInfo.ntHeaders32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            peInfo.is64Bit = false;
        } else {
            return false;
        }

        return true;
    }
}
