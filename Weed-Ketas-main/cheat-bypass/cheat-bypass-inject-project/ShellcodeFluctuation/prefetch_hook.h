#pragma once

#include <windows.h>

namespace NtCreateFileHook
{
    bool Install();
    void Remove(bool force = false);
    bool Pending();
    DWORD LastError();
    DWORD TargetPid();
}
