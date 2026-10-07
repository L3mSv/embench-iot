import csv
import fnmatch
import os
import re
import subprocess
import sys
import argparse
from pathlib import Path

BUILD_DIR = Path("bd")
IA_BUILD_DIR = Path("ia_bd")
SRC_DIR = Path("src")
REPETITIONS = 1000
CPU_CORE = None  # ex.: 3 para fixar num núcleo com taskset; None = não fixa

# --- Devem ser idênticos ao comando do scons usado para gerar o baseline (bd/) ---
CC = "gcc"
CFLAGS = ["-O2", "-fdata-sections", "-ffunction-sections"]
DEFINES = ["-DWARMUP_HEAT=1", "-DGLOBAL_SCALE_FACTOR=1"]
INCLUDES = ["-Isupport", "-Iexamples/native/speed"]
LDFLAGS = ["-O2", "-Wl,-gc-sections"]
LIBS = ["-lm"]
SUPPORT_OBJS = [
    BUILD_DIR / "support" / "main.o",
    BUILD_DIR / "support" / "beebsc.o",
    BUILD_DIR / "config" / "boardsupport.o",
]


def run(cmd):
    res = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"Comando falhou: {' '.join(map(str, cmd))}\n{res.stderr}")
    return res


def build_ia(bench, ia, exclude):
    ia_dir = IA_BUILD_DIR / "src" / ia / bench
    if not ia_dir.is_dir():
        raise FileNotFoundError(f"Diretório não encontrado: {ia_dir}")

    sources = sorted(
        p for p in ia_dir.glob("*.c")
        if not any(fnmatch.fnmatch(p.name, pat) for pat in exclude)
    )
    if not sources:
        raise FileNotFoundError(f"Nenhum .c para compilar em {ia_dir}")

    missing = [str(o) for o in SUPPORT_OBJS if not o.is_file()]
    if missing:
        raise FileNotFoundError(f"Objetos de suporte ausentes (rode o scons do baseline): {missing}")

    print(f"[build] fontes: {[s.name for s in sources]}", file=sys.stderr)

    objs = []
    for src in sources:
        obj = src.with_suffix(".o")
        run([CC, "-o", obj, "-c", *CFLAGS, *DEFINES, *INCLUDES,
             f"-I{ia_dir}", f"-I{SRC_DIR / bench}", src])
        objs.append(obj)

    exe = ia_dir / bench
    run([CC, "-o", exe, *LDFLAGS, *objs, *SUPPORT_OBJS, *LIBS])
    return exe


def find_benchmark(bench, ia=""):
    exe = (IA_BUILD_DIR / "src" / ia / bench / bench) if ia else (BUILD_DIR / "src" / bench / bench)
    if not exe.is_file():
        raise FileNotFoundError(f"Executável não encontrado: {exe}")
    return exe


def verify_ok(exe):
    # No Embench, exit code 0 = verify_benchmark passou (confira em support/main.c)
    return subprocess.run([str(exe)], capture_output=True).returncode == 0


def measure_size(exe):
    res = run(["size", exe])
    sizes = res.stdout.strip().splitlines()[1].split()
    return int(sizes[0]), int(sizes[1]), int(sizes[2])


def measure_perf(exe):
    cmd = ["sudo", "env", "LC_ALL=C", "perf", "stat", "-x,", "-r", str(REPETITIONS),
           "-e", "task-clock,cycles,instructions"]
    if CPU_CORE is not None:
        cmd += ["taskset", "-c", str(CPU_CORE)]
    cmd.append(str(exe))

    res = subprocess.run(cmd, capture_output=True, text=True)

    metrics = {}
    for line in res.stderr.splitlines():
        parts = line.split(",")
        if len(parts) >= 3:
            try:
                metrics[parts[2]] = float(parts[0])
            except ValueError:
                pass  # <not counted> / <not supported>

    for k in ("task-clock", "cycles", "instructions"):
        if k not in metrics:
            raise RuntimeError(f"perf não retornou '{k}':\n{res.stderr}")

    # task-clock já vem em milissegundos (tempo de CPU do processo)
    return metrics["task-clock"], int(metrics["cycles"]), int(metrics["instructions"])


def measure_dynamic_memory(exe):
    res = subprocess.run(["valgrind", str(exe)], stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, text=True,
                         env={**os.environ, "LC_ALL": "C"})
    m = re.search(r"total heap usage:.*, ([\d,]+) bytes allocated", res.stderr)
    return int(m.group(1).replace(",", "")) if m else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("benchmark")
    ap.add_argument("ia", nargs="?", default="", help="nome do modelo (pasta em ia_bd/src/)")
    ap.add_argument("--no-build", action="store_true", help="não recompilar a versão da IA")
    ap.add_argument("--exclude", action="append", default=None,
                    help="padrão de .c a ignorar (padrão: *standalone*)")
    args = ap.parse_args()
    exclude = args.exclude or ["*standalone*"]

    fieldnames = ["benchmark", "variant", "verify_ok", "time_ms", "cycles", "instructions",
                  "text", "data", "bss", "ram_data_bss", "dynamic_mem_bytes"]
    writer = csv.DictWriter(sys.stdout, fieldnames=fieldnames)
    writer.writeheader()

    try:
        if args.ia and not args.no_build:
            exe = build_ia(args.benchmark, args.ia, exclude)
        else:
            exe = find_benchmark(args.benchmark, args.ia)

        ok = verify_ok(exe)
        if not ok:
            print(f"AVISO: {exe} NÃO passou na verificação do benchmark", file=sys.stderr)

        time_ms, cycles, insts = measure_perf(exe)
        text, data, bss = measure_size(exe)
        dyn = measure_dynamic_memory(exe)

        writer.writerow({
            "benchmark": args.benchmark, "variant": args.ia or "baseline",
            "verify_ok": "S" if ok else "N",
            "time_ms": time_ms, "cycles": cycles, "instructions": insts,
            "text": text, "data": data, "bss": bss, "ram_data_bss": data + bss,
            "dynamic_mem_bytes": dyn,
        })
    except Exception as e:
        print(f"Erro em {args.benchmark}: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()