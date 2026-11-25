#pragma once
#include <Core/SDK/Structs/Structs.hpp>
#include <Security/xorstr.hpp>
#include <tlhelp32.h>
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <psapi.h>
#include <sstream>
#include <cstring>
#include <vector>
#include <regex>
#pragma comment(lib, "ntdll.lib")

typedef struct _PS_ATTRIBUTE
{
	ULONG_PTR Attribute;
	SIZE_T Size;
	union
	{
		ULONG_PTR Value;
		PVOID ValuePtr;
	};
	PSIZE_T ReturnLength;
} PS_ATTRIBUTE, *PPS_ATTRIBUTE;

typedef struct _PS_ATTRIBUTE_LIST
{
	SIZE_T TotalLength;
	PS_ATTRIBUTE Attributes[1];
} PS_ATTRIBUTE_LIST, *PPS_ATTRIBUTE_LIST;

typedef NTSTATUS(NTAPI* pNtCreateThreadEx)(
	PHANDLE ThreadHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PVOID StartRoutine,
	PVOID Argument,
	ULONG CreateFlags,
	SIZE_T ZeroBits,
	SIZE_T StackSize,
	SIZE_T MaximumStackSize,
	PPS_ATTRIBUTE_LIST AttributeList
	);

typedef NTSTATUS(NTAPI* pNtAllocateVirtualMemory)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtProtectVirtualMemory)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtFreeVirtualMemory)(HANDLE, PVOID*, PSIZE_T, ULONG);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

class SyscallHelper {
public:
	static void* CreateStub(const char* funcName) {
		HMODULE hNtdll = GetModuleHandleA(xorstr("ntdll.dll"));
		if (!hNtdll) return nullptr;
		BYTE* func = (BYTE*)GetProcAddress(hNtdll, funcName);
		if (!func) return nullptr;

		DWORD num = 0;
		if (func[0] == 0x4C && func[1] == 0x8B && func[2] == 0xD1 && func[3] == 0xB8)
			num = *(DWORD*)(func + 4);
		else {
			for (int i = 0; i < 28; i++) {
				if (func[i] == 0xB8) {
					DWORD n = *(DWORD*)(func + i + 1);
					if (n > 0 && n < 0x1000 && ((n >> 16) == 0)) { num = n; break; }
				}
			}
		}
		if (!num) return nullptr;

		uintptr_t sysAddr = 0;
		for (int i = 0; i < 32; i++) {
			if (func[i] == 0x0F && func[i + 1] == 0x05) { sysAddr = (uintptr_t)&func[i]; break; }
		}
		if (!sysAddr) return nullptr;

		BYTE* stub = (BYTE*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!stub) return nullptr;

		int o = 0;
		stub[o++] = 0x4C; stub[o++] = 0x8B; stub[o++] = 0xD1;
		stub[o++] = 0xB8; *(DWORD*)(stub + o) = num; o += 4;
		stub[o++] = 0xFF; stub[o++] = 0x25; *(DWORD*)(stub + o) = 0; o += 4;
		*(uintptr_t*)(stub + o) = sysAddr; o += 8;

		DWORD old;
		VirtualProtect(stub, 64, PAGE_EXECUTE_READ, &old);
		return stub;
	}
};

typedef struct _MEMORY_REGION {
	DWORD_PTR dwBaseAddr;
	DWORD_PTR dwMemorySize;
}MEMORY_REGION;

namespace Core {
	class MemoryClass {
	public:
		DWORD ProcId;
		HANDLE ProcHandle;
		std::wstring ProcName;
		uintptr_t ModBase, ModBaseSize;
		uintptr_t ModBasePlayerNames, ModBasePlayerNamesSize;

		DWORD GetPidByName(const char* ProcName);
		std::string GetNameByPid(DWORD Pid);
		uintptr_t GetModuleBaseAddr(DWORD ProcId, const char* ModuleName, uintptr_t* ModSize);

		uint64_t GetModuleBaseByName(DWORD Pid, std::wstring ModuleName);
		HMODULE GetRemoteModuleHandle(const char* ModuleName);
		uint64_t GetModuleBaseSizeByName(DWORD Pid, std::wstring ModuleName);

		BOOL OpenProc(const char* ProcName);
		BOOL OpenProcByPid();
		BOOL GetMaxPrivileges( HANDLE hProc );

		std::string ReadString( uintptr_t Addr );
		std::string ReadStringUnsupported(uint64_t address);
		std::vector<uint8_t> ReadBytes( uintptr_t Addr, size_t Size );
		BOOL WriteBytes( uintptr_t Addr, std::vector<uint8_t> Bytes );
		BOOL WriteBytesImpl(uintptr_t Addr, const void* buffer, size_t size);
		BOOL PatchFunc( uintptr_t Addr, int NopCount );
		BOOL PatchFunc( uintptr_t Addr, const std::vector<uint8_t>& Bytes );

		uintptr_t FindSignature( std::vector<uint8_t> Signature, uintptr_t ModuleBase = 0, uintptr_t ModuleBaseSize = 0 );
		// Most call sites historically expect the first valid match. Requiring
		// uniqueness globally makes every repeated instruction look "not found".
		// Callers that really need a unique match can opt in explicitly.
		uintptr_t FindSignatureStr( std::string Pattern, uintptr_t ModuleBase = 0, uintptr_t ModuleBaseSize = 0, bool Unique = false );
		uintptr_t FindMaskedSignature(const std::vector<int16_t>& Pattern, uintptr_t ModuleBase, uintptr_t ModuleBaseSize, bool Unique);
		uintptr_t PatternScan( std::vector<uint8_t> Pattern, int InstructionLength );
		uintptr_t FindSignatureBypass(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize);
		uintptr_t ScanOffset(const char* sig, bool rip, bool deref);
		uintptr_t resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize);

		bool ReadBuffer(uintptr_t Address, void* Buffer, size_t Size) {
			static auto pRead = (pNtReadVirtualMemory)SyscallHelper::CreateStub(xorstr("NtReadVirtualMemory"));
			if (!pRead) return false;
			SIZE_T bytesRead = 0;
			NTSTATUS status = pRead(ProcHandle, (PVOID)Address, Buffer, Size, &bytesRead);
			return NT_SUCCESS(status) && bytesRead == Size;
		}

		template <typename T>
		T Read( uintptr_t Addr ) {
			static auto pRead = (pNtReadVirtualMemory)SyscallHelper::CreateStub(xorstr("NtReadVirtualMemory"));
			T value = { };
			if (pRead)
				pRead( ProcHandle, (PVOID)Addr, &value, sizeof( value ), NULL );
			return value;
		}

		template <typename T>
		BOOL Write( uintptr_t Addr, T Value ) {
			static auto pWrite = (pNtWriteVirtualMemory)SyscallHelper::CreateStub(xorstr("NtWriteVirtualMemory"));
			if (!pWrite) return FALSE;
			return NT_SUCCESS(pWrite( ProcHandle, (PVOID)Addr, &Value, sizeof( Value ), NULL ));
		}

		uintptr_t CreateCodeCave( size_t Size ) {
			static auto pAlloc = (pNtAllocateVirtualMemory)SyscallHelper::CreateStub(xorstr("NtAllocateVirtualMemory"));
			if (pAlloc) {
				PVOID base = nullptr;
				SIZE_T sz = Size;
				if (NT_SUCCESS(pAlloc(ProcHandle, &base, 0, &sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
					return (uintptr_t)base;
				return 0;
			}
			return 0;
		}

		BOOL UnsealCave( uintptr_t Addr, size_t Size ) {
			static auto pProtect = (pNtProtectVirtualMemory)SyscallHelper::CreateStub(xorstr("NtProtectVirtualMemory"));
			if (!pProtect) return FALSE;
			DWORD old = 0;
			PVOID base = (PVOID)Addr;
			SIZE_T sz = Size;
			return NT_SUCCESS(pProtect(ProcHandle, &base, &sz, PAGE_READWRITE, &old));
		}

		BOOL SealCave( uintptr_t Addr, size_t Size ) {
			static auto pProtect = (pNtProtectVirtualMemory)SyscallHelper::CreateStub(xorstr("NtProtectVirtualMemory"));
			if (!pProtect) return FALSE;
			DWORD old = 0;
			PVOID base = (PVOID)Addr;
			SIZE_T sz = Size;
			return NT_SUCCESS(pProtect(ProcHandle, &base, &sz, PAGE_EXECUTE_READ, &old));
		}

		uintptr_t CreateDataCave( size_t Size ) {
			static auto pAlloc = (pNtAllocateVirtualMemory)SyscallHelper::CreateStub(xorstr("NtAllocateVirtualMemory"));
			if (pAlloc) {
				PVOID base = nullptr;
				SIZE_T sz = Size;
				if (NT_SUCCESS(pAlloc(ProcHandle, &base, 0, &sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
					return (uintptr_t)base;
				return 0;
			}
			return 0;
		}

		BOOL FreeCave( uintptr_t Addr, size_t Size = 0 ) {
			static auto pWrite = (pNtWriteVirtualMemory)SyscallHelper::CreateStub(xorstr("NtWriteVirtualMemory"));
			static auto pFree = (pNtFreeVirtualMemory)SyscallHelper::CreateStub(xorstr("NtFreeVirtualMemory"));
			if (Size > 0 && pWrite) {
				std::vector<uint8_t> zeros(Size, 0);
				pWrite(ProcHandle, (PVOID)Addr, zeros.data(), Size, nullptr);
			}
			if (pFree) {
				PVOID base = (PVOID)Addr;
				SIZE_T sz = 0;
				return NT_SUCCESS(pFree(ProcHandle, &base, &sz, MEM_RELEASE));
			}
			return FALSE;
		}

		BOOL HookJMP( uintptr_t HookAddress, uintptr_t JmpToAddress ) {
			BYTE jmpPatch[ ] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
			for ( int x = 0; x < sizeof( jmpPatch ); x++ ) {
				Write<BYTE>( HookAddress + x, jmpPatch[ x ] );
			}

			Write<uintptr_t>( HookAddress + 6, JmpToAddress );

			return true;
		}

		uintptr_t GetRipRelativeAddress(uintptr_t InstructionAddress)
		{
			if (InstructionAddress == 0)
				return 0;

			int32_t RelativeOffset = 0;
			if (!ReadBuffer(InstructionAddress + 3, &RelativeOffset, sizeof(RelativeOffset)))
				return 0;

			return InstructionAddress + 7 + RelativeOffset;
		}

		uintptr_t ResolveRelativeAddress(uintptr_t Addr, int InstructionLength)
		{
			int32_t Offset = 0;
			if (!Addr || InstructionLength < 4 || !ReadBuffer(Addr + InstructionLength - 4, &Offset, sizeof(Offset))) return 0;
			return Addr + InstructionLength + Offset;
		}

		uintptr_t CreateCodeCaveNearby(uintptr_t TargetAddr, size_t Size) {
			static auto pAlloc = (pNtAllocateVirtualMemory)SyscallHelper::CreateStub(xorstr("NtAllocateVirtualMemory"));
			if (!pAlloc) return 0;
			const uintptr_t AllocationGranularity = 64 * 1024;
			const uintptr_t MaxDist = 0x7FFFFFFF;

			for (uintptr_t offset = 0; offset < MaxDist; offset += AllocationGranularity) {
				uintptr_t tryAddr = (TargetAddr + offset) & ~(AllocationGranularity - 1);
				PVOID base = (PVOID)tryAddr;
				SIZE_T sz = Size;
				if (NT_SUCCESS(pAlloc(ProcHandle, &base, 0, &sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
					return (uintptr_t)base;
			}

			for (uintptr_t offset = AllocationGranularity; offset < MaxDist; offset += AllocationGranularity) {
				uintptr_t tryAddr = (TargetAddr - offset) & ~(AllocationGranularity - 1);
				if (tryAddr < 0x10000) break;
				PVOID base = (PVOID)tryAddr;
				SIZE_T sz = Size;
				if (NT_SUCCESS(pAlloc(ProcHandle, &base, 0, &sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
					return (uintptr_t)base;
			}

			return 0;
		}

		std::vector<uint8_t> Pattern2Vector( std::string Pattern ) {
			std::vector<uint8_t> result;
			std::stringstream ss( Pattern );
			std::string token;

			while ( ss >> token ) {
				if ( token == xorstr( "??" ) ) {
					result.push_back( 0x00 );
				}
				else if ( token == xorstr( "?" ) ) {
					result.push_back( 0x00 );
				}
				else {
					uint8_t value = std::stoul( token, nullptr, 16 );
					result.push_back( value );
				}
			}

			return result;
		}

		DWORD WaitForThreadAndReturn(HANDLE hThread)
		{
			if (!hThread)
				return 0;

			WaitForSingleObject(hThread, INFINITE);

			DWORD exitCode = 0;
			GetExitCodeThread(hThread, &exitCode);

			CloseHandle(hThread);

			return exitCode;
		}
		

		inline bool ExecuteShellcode(uintptr_t cave, DWORD waitMs = 3000)
		{
			if (!cave) return false;
			static auto pThread = (pNtCreateThreadEx)SyscallHelper::CreateStub(xorstr("NtCreateThreadEx"));
			HANDLE hThread = nullptr;
			if (pThread) {
				pThread(&hThread, THREAD_ALL_ACCESS, nullptr, ProcHandle, (PVOID)cave, nullptr, FALSE, 0, 0, 0, nullptr);
			} else {
				return false;
			}
			if (!hThread) return false;
			WaitForSingleObject(hThread, waitMs);
			CloseHandle(hThread);
			return true;
		}

		DWORD execShellcode(uintptr_t codeCaveAddress, const unsigned char* shellcode, size_t shellcodeSize) {
			if (!codeCaveAddress || !shellcode || shellcodeSize == 0)
				return false;

			static auto pWrite = (pNtWriteVirtualMemory)SyscallHelper::CreateStub(xorstr("NtWriteVirtualMemory"));
			static auto pProtect = (pNtProtectVirtualMemory)SyscallHelper::CreateStub(xorstr("NtProtectVirtualMemory"));
			static auto pThread = (pNtCreateThreadEx)SyscallHelper::CreateStub(xorstr("NtCreateThreadEx"));

			if (!pWrite || !pProtect || !pThread) return false;

			DWORD oldProtect = 0;
			PVOID base = (PVOID)codeCaveAddress;
			SIZE_T sz = shellcodeSize;
			if (!NT_SUCCESS(pProtect(ProcHandle, &base, &sz, PAGE_EXECUTE_READWRITE, &oldProtect)))
				return false;

			SIZE_T bw = 0;
			if (!NT_SUCCESS(pWrite(ProcHandle, (PVOID)codeCaveAddress,
				(PVOID)shellcode, shellcodeSize, &bw)) || bw != shellcodeSize)
			{
				DWORD ignored = 0;
				base = (PVOID)codeCaveAddress;
				sz = shellcodeSize;
				pProtect(ProcHandle, &base, &sz, oldProtect, &ignored);
				return false;
			}

			FlushInstructionCache(ProcHandle, (LPCVOID)codeCaveAddress, shellcodeSize);

			DWORD ignored = 0;
			base = (PVOID)codeCaveAddress;
			sz = shellcodeSize;
			if (!NT_SUCCESS(pProtect(ProcHandle, &base, &sz, PAGE_EXECUTE_READ, &ignored)))
				return false;

			HANDLE hRemoteThread = nullptr;
			pThread(&hRemoteThread, THREAD_ALL_ACCESS, nullptr, ProcHandle, (PVOID)codeCaveAddress, nullptr, FALSE, 0, 0, 0, nullptr);

			if (!hRemoteThread)
				return false;

			return WaitForThreadAndReturn(hRemoteThread);
		}


		inline bool ApplyBypass() {
			uintptr_t address = FindSignatureStr(
				xorstr("48 89 5C 24 ? 57 48 83 EC ? 33 DB 48 8B FA 48 89 5A")
			);

			if (!address)
			{
				address = ModBase + 0x0D8A404;
			}

			if (!address)
				return false;

			return PatchFunc(address, 64);
		}

	};

	inline MemoryClass Mem;
}
