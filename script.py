#!/usr/bin/env python3
"""Esboço para medir benchmarks individualmente e gerar uma tabela com:
- tempo absoluto médio
- text/data/bss via `size`
- RAM estática = data + bss

Ajuste os caminhos e a lista de benchmarks conforme o seu build do Embench.
"""

from __future__ import annotations

import csv
import re
import subprocess
import sys
import time
from pathlib import Path
from statistics import mean


# -----------------------------
# Configuração
# -----------------------------

BUILD_DIR = Path("bd")
# Tente ajustar para o local real dos binários.
# Ex.: bd/src/crc32/crc32
BENCHMARKS = [
    "aha-mont64",
    "crc32",
    "depthconv",
    "edn",
    "huffbench",
    "matmult-int",
    "md5sum",
    "nettle-aes",
    "nettle-sha256",
    "nsichneu",
    "picojpeg",
    "qrduino",
    "sglib-combined",
    "slre",
    "statemate",
    "tarfind",
    "ud",
    "wikisort",
    "xgboost",
]

# Quantas execuções para cada benchmark ao medir o tempo.
# Aumente isso se os benchmarks forem muito rápidos.
REPETICOES_TEMPO = 1000

# Saída CSV
CSV_OUT = Path("embench_metrics.csv")


# -----------------------------
# Utilitários
# -----------------------------


def find_executable(bench_name: str) -> Path:
    """Localiza o executável do benchmark dentro do build do Embench.

    Esta função tenta alguns padrões comuns. Ajuste se necessário.
    """
    candidates = [
        BUILD_DIR / "src" / bench_name / bench_name,
        BUILD_DIR / bench_name / bench_name,
        BUILD_DIR / bench_name,
    ]

    for path in candidates:
        if path.exists() and path.is_file() and os_access_x(path):
            return path

    # Busca recursiva como fallback.
    for path in BUILD_DIR.rglob(bench_name):
        if path.is_file() and os_access_x(path):
            return path

    raise FileNotFoundError(f"Não encontrei executável para '{bench_name}'")


def os_access_x(path: Path) -> bool:
    try:
        return path.stat().st_mode & 0o111 != 0
    except OSError:
        return False


def measure_time_absolute(executable: Path, repetitions: int) -> float:
    """Mede tempo absoluto médio em milissegundos por execução."""
    times = []

    for _ in range(repetitions):
        start = time.perf_counter()
        proc = subprocess.run(
            [str(executable)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        end = time.perf_counter()

        # Se falhar, ainda assim marca o tempo medido até o erro.
        # Você pode trocar por `raise` se preferir parar.
        times.append((end - start) * 1000.0)

        if proc.returncode != 0:
            # Para este esboço, interrompe apenas aquela execução.
            # Se quiser, pode tratar de outra forma.
            pass

    return mean(times)


_SIZE_RE = re.compile(
    r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\S+)\s*$"
)


def measure_size(executable: Path) -> tuple[int, int, int, int]:

    proc = subprocess.run(
        ["size", str(executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=True,
    )

    lines = proc.stdout.splitlines()

    # Normalmente:
    #
    # text data bss dec hex filename
    # 5210 112 344 5666 1622 executable
    #
    # Pegamos a segunda linha.

    if len(lines) < 2:
        raise RuntimeError(
            f"Saída inesperada do size para {executable}"
        )

    parts = lines[1].split()

    text = int(parts[0])
    data = int(parts[1])
    bss = int(parts[2])

    ram = data + bss

    return text, data, bss, ram


# -----------------------------
# Execução principal
# -----------------------------


def main() -> int:
    rows = []

    for bench in BENCHMARKS:
        try:
            exe = find_executable(bench)
            avg_time_ms = measure_time_absolute(exe, REPETICOES_TEMPO)
            text, data, bss, ram = measure_size(exe)

            rows.append(
                {
                    "benchmark": bench,
                    "time_ms": f"{avg_time_ms:.3f}",
                    "text": text,
                    "data": data,
                    "bss": bss,
                    "ram_data_bss": ram,
                    "executable": str(exe),
                    "status": "OK",
                }
            )

        except Exception as e:
            rows.append(
                {
                    "benchmark": bench,
                    "time_ms": "",
                    "text": "",
                    "data": "",
                    "bss": "",
                    "ram_data_bss": "",
                    "executable": "",
                    "status": f"FAIL: {e}",
                }
            )

    # Imprime tabela simples no terminal
    headers = ["Benchmark", "Time (ms)", "text", "data", "bss", "RAM(data+bss)", "status"]
    print("\t".join(headers))
    for r in rows:
        print(
            f"{r['benchmark']}\t{r['time_ms']}\t{r['text']}\t{r['data']}\t{r['bss']}\t{r['ram_data_bss']}\t{r['status']}"
        )

    # Salva CSV
    with CSV_OUT.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=["benchmark", "time_ms", "text", "data", "bss", "ram_data_bss", "executable", "status"],
        )
        writer.writeheader()
        writer.writerows(rows)

    print(f"\nCSV salvo em: {CSV_OUT.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
