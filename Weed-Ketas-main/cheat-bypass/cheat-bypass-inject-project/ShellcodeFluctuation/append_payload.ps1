param(
    [Parameter(Mandatory = $true)]
    [string]$Target,
    [Parameter(Mandatory = $true)]
    [string]$Payload
)

$ErrorActionPreference = 'Stop'
$targetPath = Resolve-Path -LiteralPath $Target

# --- Strip Rich PE header and zero compilation timestamp ---
$pe = [System.IO.File]::ReadAllBytes($targetPath)

# PE header offset at 0x3C
$peOff = [BitConverter]::ToInt32($pe, 0x3C)

# Zero TimeDateStamp at PE+8 (4 bytes)
$pe[$peOff + 8] = 0; $pe[$peOff + 9] = 0
$pe[$peOff + 10] = 0; $pe[$peOff + 11] = 0

# Find and zero Rich header: scan backwards from PE offset for "Rich" marker
for ($i = $peOff - 4; $i -ge 0x80; $i--) {
    if ($pe[$i] -eq 0x52 -and $pe[$i+1] -eq 0x69 -and
        $pe[$i+2] -eq 0x63 -and $pe[$i+3] -eq 0x68) {
        # XOR key is the 4 bytes after "Rich"
        # Zero from end of DOS stub (0x80) to Rich+8
        $richEnd = $i + 8
        for ($j = 0x80; $j -lt $richEnd; $j++) { $pe[$j] = 0 }
        Write-Host ("stripped Rich header: 0x80..0x{0:X}" -f ($richEnd - 1))
        break
    }
}

[System.IO.File]::WriteAllBytes($targetPath, $pe)

# --- Append payload with magic footer ---
$payloadBytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Payload))
$magic = [byte[]](0xF5, 0xE1, 0x93, 0xFF, 0xF7, 0xE6, 0xFE, 0xEB)
$length = [BitConverter]::GetBytes([uint32]$payloadBytes.Length)

$stream = [System.IO.File]::Open(
    $targetPath,
    [System.IO.FileMode]::Append,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::Read)
try {
    $stream.Write($payloadBytes, 0, $payloadBytes.Length)
    $stream.Write($magic, 0, $magic.Length)
    $stream.Write($length, 0, $length.Length)
}
finally {
    $stream.Dispose()
}

Write-Host ("appended {0} bytes to {1}" -f $payloadBytes.Length, $Target)
