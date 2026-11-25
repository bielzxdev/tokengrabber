#pragma once

#include <windows.h>
#include <vector>

extern std::vector<unsigned char> buf;

// Select the module that owns the appended payload.
void SetPayloadModule(HMODULE module);

// Load and decode the payload appended to the module image.
void DecodePackedPayload(std::vector<unsigned char>& decoded);
