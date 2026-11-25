#include "../include/Security.h"

namespace Hollowing {

    bool Security::CheckAnalysis() {

        return false;
    }

    void Security::XORCipher(std::vector<BYTE>& data, const std::string& key) {
        if (key.empty()) return;
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= key[i % key.length()];
        }
    }
}
