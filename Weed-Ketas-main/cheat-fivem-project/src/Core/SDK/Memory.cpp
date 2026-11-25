
#include "pch.h"
#include "Memory.hpp"
#include <Globals.hpp>
#include <sstream>
#include <cctype>

namespace Core {

	uintptr_t MemoryClass::PatternScan(std::vector<uint8_t> Pattern, int InstructionLength)
	{
		uintptr_t Address = FindSignature(Pattern);

		if (InstructionLength != 0)
		{
			Address = ResolveRelativeAddress(Address, InstructionLength);
		}

		return Address;
	}

	uintptr_t MemoryClass::FindSignature(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize)
	{
		// Preserve the legacy vector API: zero means wildcard only here.
		std::vector<int16_t> Pattern;
		Pattern.reserve(Signature.size());
		for (const auto Byte : Signature) Pattern.push_back(Byte ? Byte : -1);
		return FindMaskedSignature(Pattern, ModuleBase, ModuleBaseSize, false);
	}

	uintptr_t MemoryClass::FindMaskedSignature(const std::vector<int16_t>& Pattern, uintptr_t ModuleBase, uintptr_t ModuleBaseSize, bool Unique)
	{
		const size_t BlockSize = 16384;
		if (Pattern.empty() || Pattern.size() > BlockSize) return 0;
		const uintptr_t Base = ModuleBase && ModuleBaseSize ? ModuleBase : ModBase;
		const uintptr_t Size = ModuleBase && ModuleBaseSize ? ModuleBaseSize : ModBaseSize;
		if (!ProcHandle || !Base || !Size || Size > UINTPTR_MAX - Base) return 0;
		static auto ReadMemory = (pNtReadVirtualMemory)SyscallHelper::CreateStub(xorstr("NtReadVirtualMemory"));
		if (!ReadMemory) return 0;
		std::vector<uint8_t> Buffer(BlockSize + Pattern.size() - 1);
		uintptr_t Match = 0;
		for (uintptr_t Offset = 0; Offset < Size; Offset += BlockSize) {
			SIZE_T Read = 0;
			const SIZE_T Count = static_cast<SIZE_T>(std::min<uintptr_t>(Buffer.size(), Size - Offset));
			ReadMemory(ProcHandle, reinterpret_cast<PVOID>(Base + Offset), Buffer.data(), Count, &Read);
			if (Read < Pattern.size()) continue;
			// Overlap is read, but starts belong only to the current block.
			for (size_t I = 0; I < BlockSize && I + Pattern.size() <= Read; ++I) {
				bool Matches = true;
				for (size_t J = 0; J < Pattern.size(); ++J)
					if (Pattern[J] >= 0 && Buffer[I + J] != Pattern[J]) { Matches = false; break; }
				if (!Matches) continue;
				if (!Unique) return Base + Offset + I;
				if (Match) return 0;
				Match = Base + Offset + I;
			}
		}
		return Match;
	}

	HMODULE MemoryClass::GetRemoteModuleHandle(const char* ModuleName) {
		if (!Mem.ProcHandle || Mem.ProcId == 0) {
			return NULL;
		}

		HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, Mem.ProcId);
		if (hSnapShot == INVALID_HANDLE_VALUE) {
			return NULL;
		}

		MODULEENTRY32 ModuleEntry;
		ModuleEntry.dwSize = sizeof(MODULEENTRY32);
		HMODULE hModule = NULL;

		if (Module32First(hSnapShot, &ModuleEntry)) {
			do {
				if (_stricmp(ModuleEntry.szModule, ModuleName) == 0) {
					hModule = ModuleEntry.hModule;
					break;
				}
			} while (Module32Next(hSnapShot, &ModuleEntry));
		}

		CloseHandle(hSnapShot);
		return hModule;
	}

	uintptr_t MemoryClass::FindSignatureBypass(std::vector<uint8_t> Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize)
	{
		return FindSignature(std::move(Signature), ModuleBase, ModuleBaseSize);
	}

	uintptr_t MemoryClass::FindSignatureStr(std::string Pattern, uintptr_t ModuleBase, uintptr_t ModuleBaseSize, bool Unique)
	{
		std::istringstream Tokens(Pattern);
		std::vector<int16_t> Bytes;
		std::string Token;
		while (Tokens >> Token) {
			if (Token == "?" || Token == "??") { Bytes.push_back(-1); continue; }
			if (Token.size() != 2 || !std::isxdigit(static_cast<unsigned char>(Token[0]))
				|| !std::isxdigit(static_cast<unsigned char>(Token[1]))) return 0;
			Bytes.push_back(static_cast<int16_t>(std::stoi(Token, nullptr, 16)));
		}
		return FindMaskedSignature(Bytes, ModuleBase, ModuleBaseSize, Unique);
	}

	std::string MemoryClass::ReadStringUnsupported(uint64_t address) {
		uint64_t chunk0 = Mem.Read<uint64_t>(address + 0x00);
		uint64_t chunk1 = Mem.Read<uint64_t>(address + 0x08);
		uint64_t length = Mem.Read<uint64_t>(address + 0x10);

		if (length <= 15) {
			std::string result;
			result.reserve(length);

			for (size_t i = 0; i < length; ++i) {
				char c;
				if (i < 8) {
					c = reinterpret_cast<char*>(&chunk0)[i];
				}
				else {
					c = reinterpret_cast<char*>(&chunk1)[i - 8];
				}
				result.push_back(c);
			}
			return result;
		}
		else {
			uint64_t heap_ptr = chunk0;

			std::string result;
			result.reserve(length);

			for (size_t i = 0; i < length; ++i) {
				char c = Mem.Read<char>(heap_ptr + i);
				result.push_back(c);
			}

			return result;
		}
	}

	uintptr_t MemoryClass::resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize)
	{
		int32_t ripOffset = 0;
		if (!instr || instrSize <= 0 || offsetOffset < 0 || !ReadBuffer(instr + offsetOffset, &ripOffset, sizeof(ripOffset))) return 0;
		return instr + instrSize + ripOffset;
	}

	uintptr_t MemoryClass::ScanOffset(const char* sig, bool rip, bool deref)
	{
		uintptr_t found = FindSignatureStr(sig); 

		if (!found)
			return 0;

		uintptr_t resolved = found;

		if (rip)
			resolved = resolveRelativeAddress(found, 3, 7);

		if (deref)
			resolved = Read<uintptr_t>(resolved);

		return resolved - ModBase;
	}

	std::vector<uint8_t> MemoryClass::ReadBytes(uintptr_t Addr, size_t Size) {
		static auto pRead = (pNtReadVirtualMemory)SyscallHelper::CreateStub(xorstr("NtReadVirtualMemory"));
		std::vector<uint8_t> bytes(Size);
		if (!pRead) return {};
		SIZE_T bytesRead = 0;
		pRead(ProcHandle, (PVOID)Addr, bytes.data(), Size, &bytesRead);
		bytes.resize(bytesRead);
		return bytes;
	}

	BOOL MemoryClass::WriteBytes(uintptr_t Addr, std::vector<uint8_t> Bytes) {
		static auto pWrite = (pNtWriteVirtualMemory)SyscallHelper::CreateStub(xorstr("NtWriteVirtualMemory"));
		if (!pWrite) return FALSE;
		return NT_SUCCESS(pWrite(ProcHandle, (PVOID)Addr, Bytes.data(), Bytes.size(), NULL));
	}

	BOOL MemoryClass::WriteBytesImpl(uintptr_t Addr, const void* buffer, size_t size)
	{
		static auto pWrite = (pNtWriteVirtualMemory)SyscallHelper::CreateStub(xorstr("NtWriteVirtualMemory"));
		if (!pWrite) return FALSE;
		return NT_SUCCESS(pWrite(ProcHandle, (PVOID)Addr, (PVOID)buffer, size, nullptr));
	}

	BOOL MemoryClass::PatchFunc(uintptr_t Addr, const std::vector<uint8_t>& Bytes)
	{
		if (!Addr || Bytes.empty()) return FALSE;
		const auto Original = ReadBytes(Addr, Bytes.size());
		if (Original.size() != Bytes.size()) return FALSE;
		DWORD OldProtection = 0;
		if (!VirtualProtectEx(ProcHandle, reinterpret_cast<void*>(Addr), Bytes.size(),
			PAGE_EXECUTE_READWRITE, &OldProtection))
			return FALSE;
		const bool Written = WriteBytes(Addr, Bytes) && ReadBytes(Addr, Bytes.size()) == Bytes;
		if (!Written) WriteBytes(Addr, Original);
		const bool Flushed = FlushInstructionCache(ProcHandle,
			reinterpret_cast<const void*>(Addr), Bytes.size()) != FALSE;
		DWORD IgnoredProtection = 0;
		const bool Restored = VirtualProtectEx(ProcHandle, reinterpret_cast<void*>(Addr),
			Bytes.size(), OldProtection, &IgnoredProtection) != FALSE;
		return Written && Flushed && Restored;
	}

	BOOL MemoryClass::PatchFunc(uintptr_t Addr, int NopCount)
	{
		if (!NopCount) return false;

		std::vector<uint8_t> PatchTable = { };
		for (int i = 0; i < NopCount; i++)
		{
			PatchTable.push_back({ 0x90 });
		}

		return WriteBytes(Addr, PatchTable);
	}


	BOOL MemoryClass::GetMaxPrivileges(HANDLE hProc) {
		HANDLE h_Token;
		DWORD dw_TokenLength;
		if (OpenProcessToken(hProc, TOKEN_READ | TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &h_Token))
		{
			TOKEN_PRIVILEGES* privilages = new TOKEN_PRIVILEGES[100];
			if (GetTokenInformation(h_Token, TokenPrivileges, privilages, sizeof(TOKEN_PRIVILEGES) * 100, &dw_TokenLength))
			{
				for (int i = 0; i < privilages->PrivilegeCount; i++)
				{
					privilages->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
				}
				if (AdjustTokenPrivileges(h_Token, false, privilages, sizeof(TOKEN_PRIVILEGES) * 100, NULL, NULL))
				{
					delete[] privilages;
					return true;
				}
			}
			delete[] privilages;
		}

		return false;
	}

	std::string MemoryClass::ReadString(uintptr_t Addr)
	{
		if (!Addr)
			return "";

		std::string Result;
		char Chunk[64];

		// Le em blocos: uma leitura unica de 255 bytes falha por inteiro se
		// esbarrar numa pagina nao mapeada (o que acontece quando a string
		// esta perto do fim de uma alocacao).
		for (int Block = 0; Block < 4; Block++)
		{
			SIZE_T bytes = 0;
			memset(Chunk, 0, sizeof(Chunk));

			static auto pRead = (pNtReadVirtualMemory)SyscallHelper::CreateStub(xorstr("NtReadVirtualMemory"));
			if (!pRead) return Result;
			NTSTATUS status = pRead(
				ProcHandle,
				reinterpret_cast<PVOID>(Addr + Block * sizeof(Chunk)),
				Chunk,
				sizeof(Chunk),
				&bytes);

			// STATUS_SUCCESS == 0, entao o "if (!Zw...) return """ antigo
			// devolvia string vazia exatamente quando a leitura dava certo.
			if (!NT_SUCCESS(status) || bytes == 0)
				break;

			for (SIZE_T i = 0; i < bytes; i++)
			{
				if (Chunk[i] == '\0')
					return Result;
				Result.push_back(Chunk[i]);
			}
		}

		return Result;
	}

	DWORD MemoryClass::GetPidByName(const char* ProcName) {
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return 0;
		}

		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		if (!Process32First(hSnapshot, &pe32)) {
			CloseHandle(hSnapshot);
			return 0;
		}

		DWORD pid = 0;
		do {
			if (_stricmp(pe32.szExeFile, ProcName) == 0) {
				pid = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));

		CloseHandle(hSnapshot);
		return pid;
	}

	std::string MemoryClass::GetNameByPid(DWORD Pid)
	{
		HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
		if (!process) return {};
		char path[32768]{};
		DWORD size = sizeof(path);
		const bool found = QueryFullProcessImageNameA(process, 0, path, &size) != FALSE;
		CloseHandle(process);
		if (!found) return {};
		std::string name(path, size);
		const size_t slash = name.find_last_of("\\/");
		return slash == std::string::npos ? name : name.substr(slash + 1);
	}

	uintptr_t MemoryClass::GetModuleBaseAddr(DWORD ProcId, const char* ModuleName, uintptr_t* ModSize) {
		if (ModSize) *ModSize = 0;
		if (!ProcId || !ModuleName) return 0;
		wchar_t WideModuleName[MAX_PATH]{};
		if (!MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, WideModuleName, MAX_PATH)) return 0;
		HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, ProcId);

		if (hSnapShot == INVALID_HANDLE_VALUE) { return 0; }

		MODULEENTRY32W ModuleEntry{};
		ModuleEntry.dwSize = sizeof(ModuleEntry);
		uintptr_t ModuleBase = 0;
		if (Module32FirstW(hSnapShot, &ModuleEntry)) {
			do {
				if (_wcsicmp(ModuleEntry.szModule, WideModuleName) == 0) {
					ModuleBase = reinterpret_cast<uintptr_t>(ModuleEntry.modBaseAddr);
					if (ModSize) *ModSize = ModuleEntry.modBaseSize;
					break;
				}
			} while (Module32NextW(hSnapShot, &ModuleEntry));
		}
		CloseHandle(hSnapShot);

		return ModuleBase;
	}

	uint64_t MemoryClass::GetModuleBaseByName(DWORD Pid, std::wstring ModuleName)
	{
		HANDLE hSnapshot = (CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, Pid);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
			return 0;
		}

		uint64_t ModuleBase = 0;
		MODULEENTRY32W ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);

		if ((Module32FirstW)(hSnapshot, &ModuleEntry))
		{
			do {
				if (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()) == 0)
				{
					ModuleBase = reinterpret_cast<uint64_t>(ModuleEntry.modBaseAddr);
					break;
				}
			} while ((Module32NextW)(hSnapshot, &ModuleEntry));
		}

		(CloseHandle)(hSnapshot);
		return ModuleBase;
	}

	uint64_t MemoryClass::GetModuleBaseSizeByName(DWORD Pid, std::wstring ModuleName)
	{
		HANDLE hSnapshot = (CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, Pid);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
		{
			return 0;
		}

		uint64_t ModuleBase = 0;
		MODULEENTRY32W ModuleEntry;
		ModuleEntry.dwSize = sizeof(ModuleEntry);

		if ((Module32FirstW)(hSnapshot, &ModuleEntry))
		{
			do {
				if (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()) == 0)
				{
					ModuleBase = (uint64_t)ModuleEntry.modBaseSize;
					break;
				}
			} while ((Module32NextW)(hSnapshot, &ModuleEntry));
		}

		(CloseHandle)(hSnapshot);
		return ModuleBase;
	}

	BOOL MemoryClass::OpenProcByPid() {
		const DWORD pid = g_Variables.ProcIdFiveM;
		if (!pid || ProcName.empty()) return FALSE;
		HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if (!process) return FALSE;

		const std::string name(ProcName.begin(), ProcName.end());
		uintptr_t size = 0;
		const uintptr_t base = GetModuleBaseAddr(pid, name.c_str(), &size);
		DWORD exitCode = 0;
		if (!base || !size || !GetExitCodeProcess(process, &exitCode) || exitCode != STILL_ACTIVE)
		{
			CloseHandle(process);
			return FALSE;
		}
		if (ProcHandle) CloseHandle(ProcHandle);
		ProcHandle = process;
		ProcId = pid;
		ModBase = base;
		ModBaseSize = size;
		ModBasePlayerNames = ModBasePlayerNamesSize = 0;
		return TRUE;
	}
}
