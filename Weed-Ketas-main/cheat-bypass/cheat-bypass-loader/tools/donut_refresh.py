#!/usr/bin/env python3
"""Converte a saida C do Donut em binario e atualiza o payload embarcado.

Uso:
  python tools/donut_refresh.py <cheat_donut.c> <cheat_donut.bin> <cheat_embedded.hpp>
  python tools/donut_refresh.py <loader_donut.c> <loader.bin>

Os arquivos C devem ter sido gerados com:
  donut -a 2 -f 3 -z 3 -b 3 -e 3 -k 1 -x 1 -i <exe> -o <arquivo.c>
"""

import base64
import re
import sys
from pathlib import Path

ESCAPE = re.compile(rb"\\x([0-9A-Fa-f]{2})")


def parse_donut_c(path: Path) -> bytes:
    text = path.read_bytes()
    # O Donut emite `unsigned char buf[] = ` seguido de linhas `"\x.."`.
    _, _, body = text.partition(b"=")
    if not body:
        raise SystemExit(f"formato C inesperado em {path}")
    out = bytearray()
    for match in ESCAPE.finditer(body):
        out.append(int(match.group(1), 16))
    if not out:
        raise SystemExit(f"nenhum byte encontrado em {path}")
    return bytes(out)


def make_embedded_header(payload: bytes, flags: str) -> str:
    b64 = base64.b64encode(payload).decode("ascii")
    lines = [b64[i:i + 120] for i in range(0, len(b64), 120)]
    quoted = "\n".join(f'    "{line}"' for line in lines)
    return (
        "#pragma once\n"
        "// Shellcode Donut do cheat, embarcado como string base64 em .rdata.\n"
        f"// Gerado a partir de cheat_donut.bin ({flags}).\n"
        "// String literal e referenciada por simbolo: funciona mesmo quando o PE e\n"
        "// mapeado manualmente pelo Donut (sem entrada na lista de modulos do PEB).\n"
        "// Uma linha por literal de 120 chars evita um unico token gigante no cl.exe.\n"
        "static const char kCheatDonutBase64[] =\n"
        f"{quoted}\n"
        "    ;\n"
        f"static const unsigned int kCheatDonutBase64Len = {len(b64)}u;\n"
    )


def main() -> int:
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)

    src = Path(sys.argv[1])
    out_bin = Path(sys.argv[2])
    payload = parse_donut_c(src)
    out_bin.write_bytes(payload)
    print(f"{out_bin}: {len(payload)} bytes")

    if len(sys.argv) == 4:
        header = Path(sys.argv[3])
        flags = "donut -a 2 -f 3 -z 3 -b 3 -e 3 -k 1 -x 1 -i Weed.exe"
        header.write_text(make_embedded_header(payload, flags), encoding="utf-8")
        print(f"{header}: {header.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
