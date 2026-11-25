# =============================================================================
# strip_pe.ps1 - Remove fingerprints do PE apos build
#
# Zera:
#   1. Rich Header       (fingerprint MSVC usado por PEiD/DetectItEasy)
#   2. TimeDateStamp     (data/hora de compilacao)
#   3. Linker version    (Major/MinorLinkerVersion no OptionalHeader)
#   4. Debug Directory   (caminho do .pdb, CV info, GUID)
#   5. DataDirectory[6]  (entrada de Debug Directory)
# =============================================================================
param(
    [Parameter(Mandatory=$true)][string]$Path
)

if (-not (Test-Path $Path)) {
    Write-Host "strip_pe.ps1: arquivo nao encontrado: $Path"
    exit 0  # nao falha o build
}

$bytes = [System.IO.File]::ReadAllBytes($Path)

# --- e_lfanew: offset do PE header ---
$e_lfanew = [BitConverter]::ToInt32($bytes, 0x3C)

# --- 1. Rich Header ---
# Procura signature 'Rich' (0x68636952) entre 0x80 e e_lfanew
$richOffset = -1
for ($i = 0x80; $i -lt ($e_lfanew - 4); $i++) {
    if ($bytes[$i]   -eq 0x52 -and $bytes[$i+1] -eq 0x69 -and
        $bytes[$i+2] -eq 0x63 -and $bytes[$i+3] -eq 0x68) {
        $richOffset = $i
        break
    }
}

if ($richOffset -ge 0) {
    # Chave XOR: 4 bytes apos 'Rich'
    $k0 = $bytes[$richOffset+4]
    $k1 = $bytes[$richOffset+5]
    $k2 = $bytes[$richOffset+6]
    $k3 = $bytes[$richOffset+7]

    # Procura 'DanS' (0x536E6144) XORado com a chave, retrocedendo de 4 em 4
    $dansOffset = -1
    for ($i = $richOffset - 4; $i -ge 0x40; $i -= 4) {
        if ((($bytes[$i]   -bxor $k0) -eq 0x44) -and
            (($bytes[$i+1] -bxor $k1) -eq 0x61) -and
            (($bytes[$i+2] -bxor $k2) -eq 0x6E) -and
            (($bytes[$i+3] -bxor $k3) -eq 0x53)) {
            $dansOffset = $i
            break
        }
    }

    if ($dansOffset -ge 0) {
        # Zera do DanS ate Rich+8 (inclui a chave)
        $end = $richOffset + 8
        for ($i = $dansOffset; $i -lt $end; $i++) { $bytes[$i] = 0 }
        Write-Host ("  Rich Header removido ({0} bytes)" -f ($end - $dansOffset))
    } else {
        Write-Host "  Rich Header: DanS nao encontrado (skipped)"
    }
} else {
    Write-Host "  Rich Header: nao encontrado (skipped)"
}

# --- 2. TimeDateStamp (IMAGE_FILE_HEADER at NT+4, TimeDateStamp offset 4) ---
$tsOffset = $e_lfanew + 8
for ($i = 0; $i -lt 4; $i++) { $bytes[$tsOffset + $i] = 0 }
Write-Host "  TimeDateStamp zerado"

# --- 3. Linker version (OptionalHeader offsets 2 e 3) ---
# OptionalHeader comeca em NT+24 (Signature 4 + FileHeader 20)
$optHeader = $e_lfanew + 24
$bytes[$optHeader + 2] = 0   # MajorLinkerVersion
$bytes[$optHeader + 3] = 0   # MinorLinkerVersion
Write-Host "  Linker version zerada"

# --- 4/5. Debug Directory ---
$magic = [BitConverter]::ToUInt16($bytes, $optHeader)
$isPE32Plus = ($magic -eq 0x20B)
# DataDirectory offset dentro do OptionalHeader
$dataDirOffset = $optHeader + $(if ($isPE32Plus) { 112 } else { 96 })
# DataDirectory[6] = Debug (8 bytes: VA + Size)
$debugEntryOffset = $dataDirOffset + (6 * 8)
$debugVA   = [BitConverter]::ToUInt32($bytes, $debugEntryOffset)
$debugSize = [BitConverter]::ToUInt32($bytes, $debugEntryOffset + 4)

if ($debugVA -ne 0 -and $debugSize -ne 0) {
    # Traduz RVA para file offset percorrendo section headers
    $numSections           = [BitConverter]::ToUInt16($bytes, $e_lfanew + 6)
    $sizeOfOptionalHeader  = [BitConverter]::ToUInt16($bytes, $e_lfanew + 20)
    $sectionsOffset        = $optHeader + $sizeOfOptionalHeader

    $debugFileOffset = 0
    for ($s = 0; $s -lt $numSections; $s++) {
        $secBase    = $sectionsOffset + ($s * 40)
        $secVSize   = [BitConverter]::ToUInt32($bytes, $secBase + 8)
        $secVA      = [BitConverter]::ToUInt32($bytes, $secBase + 12)
        $secRawOff  = [BitConverter]::ToUInt32($bytes, $secBase + 20)

        if ($debugVA -ge $secVA -and $debugVA -lt ($secVA + $secVSize)) {
            $debugFileOffset = $secRawOff + ($debugVA - $secVA)
            break
        }
    }

    if ($debugFileOffset -gt 0) {
        # Zera o conteudo apontado pelo Debug Directory (inclui path do .pdb e GUID)
        # Cada entry de debug tem 28 bytes; alem disso, dentro dela ha ponteiro para
        # o CodeView info (que contem o path do PDB). Vamos zerar tambem esse CV info.
        $numDebugEntries = [int]($debugSize / 28)
        for ($d = 0; $d -lt $numDebugEntries; $d++) {
            $entryBase = $debugFileOffset + ($d * 28)
            $cvSize    = [BitConverter]::ToUInt32($bytes, $entryBase + 16)
            $cvFileOff = [BitConverter]::ToUInt32($bytes, $entryBase + 24)
            # Zera o CodeView info (path do PDB fica aqui)
            if ($cvFileOff -gt 0 -and $cvSize -gt 0 -and ($cvFileOff + $cvSize) -le $bytes.Length) {
                for ($i = 0; $i -lt $cvSize; $i++) { $bytes[$cvFileOff + $i] = 0 }
            }
        }
        # Zera o Debug Directory em si
        for ($i = 0; $i -lt $debugSize; $i++) { $bytes[$debugFileOffset + $i] = 0 }
        Write-Host ("  Debug Directory zerado ({0} bytes)" -f $debugSize)
    }

    # Zera a entry do Debug no DataDirectory
    for ($i = 0; $i -lt 8; $i++) { $bytes[$debugEntryOffset + $i] = 0 }
    Write-Host "  DataDirectory[Debug] zerado"
} else {
    Write-Host "  Debug Directory: ausente (nada a fazer)"
}

# --- 6. Renomear sections suspeitas ---
# PEiD / detect-it-easy flagam .tls (TLS) e .00cfg (CFG) como indicadores.
# Renomeamos para nomes padrao. A DataDirectory continua apontando por RVA,
# entao o loader continua achando essas sections normalmente.
$numSections          = [BitConverter]::ToUInt16($bytes, $e_lfanew + 6)
$sizeOfOptionalHeader = [BitConverter]::ToUInt16($bytes, $e_lfanew + 20)
$sectionsOffset       = $optHeader + $sizeOfOptionalHeader

$renameMap = @{
    ".tls"    = ".data"
    ".00cfg"  = ".rdata"
}

for ($s = 0; $s -lt $numSections; $s++) {
    $secBase = $sectionsOffset + ($s * 40)

    # Le nome atual (8 bytes, null-padded)
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt 8; $i++) {
        $b = $bytes[$secBase + $i]
        if ($b -eq 0) { break }
        [void]$sb.Append([char]$b)
    }
    $currentName = $sb.ToString()

    if ($renameMap.ContainsKey($currentName)) {
        $newName = $renameMap[$currentName]
        # Zera os 8 bytes e escreve o novo nome
        for ($i = 0; $i -lt 8; $i++) { $bytes[$secBase + $i] = 0 }
        for ($i = 0; $i -lt $newName.Length -and $i -lt 8; $i++) {
            $bytes[$secBase + $i] = [byte][char]$newName[$i]
        }
        Write-Host ("  Section '{0}' renomeada para '{1}'" -f $currentName, $newName)
    }
}

[System.IO.File]::WriteAllBytes($Path, $bytes)
Write-Host "strip_pe.ps1: OK -> $Path"
