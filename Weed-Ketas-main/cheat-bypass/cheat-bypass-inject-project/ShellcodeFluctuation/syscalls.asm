.code

EXTERN g_SyscallTable : QWORD
EXTERN SpoofedSyscall : PROC

PUBLIC SysNtAllocateVirtualMemory
PUBLIC SysNtFreeVirtualMemory
PUBLIC SysNtWriteVirtualMemory
PUBLIC SysNtProtectVirtualMemory
PUBLIC SysNtCreateThreadEx
PUBLIC SysNtOpenProcess
PUBLIC SysNtOpenThread
PUBLIC SysNtClose
PUBLIC SysNtQuerySystemInformation
PUBLIC SysNtCreateSection
PUBLIC SysNtMapViewOfSection
PUBLIC SysNtUnmapViewOfSection
PUBLIC SysNtDelayExecution

; SYSCALL_ENTRY = { DWORD ssn; PVOID syscallAddress; } -> 16 bytes em x64
; idx*16 pega o DWORD ssn da entrada

SysNtAllocateVirtualMemory PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[0]        ; idx 0
    syscall
    ret
SysNtAllocateVirtualMemory ENDP

SysNtFreeVirtualMemory PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[16]       ; idx 1
    syscall
    ret
SysNtFreeVirtualMemory ENDP

SysNtWriteVirtualMemory PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[32]       ; idx 2
    syscall
    ret
SysNtWriteVirtualMemory ENDP

SysNtProtectVirtualMemory PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[48]       ; idx 3
    syscall
    ret
SysNtProtectVirtualMemory ENDP

SysNtCreateThreadEx PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[64]       ; idx 4
    syscall
    ret
SysNtCreateThreadEx ENDP

SysNtOpenProcess PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[80]       ; idx 5
    syscall
    ret
SysNtOpenProcess ENDP

SysNtClose PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[96]       ; idx 6
    syscall
    ret
SysNtClose ENDP

SysNtQuerySystemInformation PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[112]      ; idx 7
    syscall
    ret
SysNtQuerySystemInformation ENDP

SysNtCreateSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[128]      ; idx 8
    syscall
    ret
SysNtCreateSection ENDP

SysNtMapViewOfSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[144]      ; idx 9
    syscall
    ret
SysNtMapViewOfSection ENDP

SysNtUnmapViewOfSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[160]      ; idx 10
    syscall
    ret
SysNtUnmapViewOfSection ENDP

SysNtDelayExecution PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[176]      ; idx 11
    syscall
    ret
SysNtDelayExecution ENDP

SysNtOpenThread PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[192]      ; idx 12
    syscall
    ret
SysNtOpenThread ENDP

END
