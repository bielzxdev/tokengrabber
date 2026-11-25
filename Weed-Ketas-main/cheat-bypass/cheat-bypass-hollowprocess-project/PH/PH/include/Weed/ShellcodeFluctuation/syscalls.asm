.code

EXTERN g_SyscallTable : QWORD
EXTERN SpoofedSyscall : PROC

PUBLIC SysNtAllocateVirtualMemory
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

; cada wrapper salva os 4 primeiros args (rcx/rdx/r8/r9), monta a
; chamada SpoofedSyscall(ssn, addr, nargs, arg1..argN) e retorna

SysNtAllocateVirtualMemory PROC
    ; 6 args: rcx=ProcessHandle, rdx=BaseAddress*, r8=ZeroBits, r9=RegionSize*
    ; [rsp+28h]=AllocationType, [rsp+30h]=Protect
    
    mov     r10, rcx                    ; r10 = arg1 (Windows syscall convention)
    mov     eax, DWORD PTR g_SyscallTable[0]  ; SSN
    syscall
    ret
SysNtAllocateVirtualMemory ENDP

SysNtWriteVirtualMemory PROC
    ; 5 args: rcx=ProcessHandle, rdx=BaseAddress, r8=Buffer, r9=Size, [rsp+28h]=Written*
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[16]
    syscall
    ret
SysNtWriteVirtualMemory ENDP

SysNtProtectVirtualMemory PROC
    ; 5 args: rcx=ProcessHandle, rdx=BaseAddress*, r8=RegionSize*, r9=NewProtect, [rsp+28h]=OldProtect*
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[32]
    syscall
    ret
SysNtProtectVirtualMemory ENDP

SysNtCreateThreadEx PROC
    ; 11 args - syscall direto
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[48]
    syscall
    ret
SysNtCreateThreadEx ENDP

SysNtOpenProcess PROC
    ; 4 args: rcx=ProcessHandle*, rdx=DesiredAccess, r8=ObjectAttributes*, r9=ClientId*
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[64]
    syscall
    ret
SysNtOpenProcess ENDP

SysNtOpenThread PROC
    ; 4 args: rcx=ThreadHandle*, rdx=DesiredAccess, r8=ObjectAttributes*, r9=ClientId*
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[176]
    syscall
    ret
SysNtOpenThread ENDP

SysNtClose PROC
    ; 1 arg: rcx=Handle
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[80]
    syscall
    ret
SysNtClose ENDP

SysNtQuerySystemInformation PROC
    ; 4 args: rcx=Class, rdx=Buffer, r8=Length, r9=ReturnLength*
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[96]
    syscall
    ret
SysNtQuerySystemInformation ENDP

; NtCreateSection - 7 args
SysNtCreateSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[112]
    syscall
    ret
SysNtCreateSection ENDP

; NtMapViewOfSection - 10 args
SysNtMapViewOfSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[128]
    syscall
    ret
SysNtMapViewOfSection ENDP

; NtUnmapViewOfSection - 2 args
SysNtUnmapViewOfSection PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[144]
    syscall
    ret
SysNtUnmapViewOfSection ENDP

; NtDelayExecution - 2 args
SysNtDelayExecution PROC
    mov     r10, rcx
    mov     eax, DWORD PTR g_SyscallTable[160]
    syscall
    ret
SysNtDelayExecution ENDP

END
