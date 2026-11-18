#!/usr/bin/env python3
"""Teste diferencial por perfil: original x IA com entradas variadas.

Uso: python fuzz.py <benchmark> <modelo> [--seeds 1 2 3] [-n 2000] [--san]

O driver de cada benchmark fica em tests/<perfil>/<benchmark>.c. Ele inclui o
.c sob teste por texto (#include "arquivo.c"); aqui a versão é escolhida pelo -I.
As constantes abaixo devem ser iguais às do script.py.
"""
import argparse
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

BUILD_DIR = Path("bd")
IA_BUILD_DIR = Path("ia_bd") / "src"
SRC_DIR = Path("src")
TESTS_DIR = Path("tests")

CC = "gcc"
CFLAGS = ["-O2", "-fdata-sections", "-ffunction-sections"]
DEFINES = ["-DWARMUP_HEAT=1", "-DGLOBAL_SCALE_FACTOR=1"]
INCLUDES = ["-Isupport", "-Iexamples/native/speed"]
LDFLAGS = ["-O2", "-Wl,-gc-sections"]
LIBS = ["-lm"]
LINK_OBJS = [BUILD_DIR / "support" / "beebsc.o", BUILD_DIR / "config" / "boardsupport.o"]
SAN = ["-O1", "-g", "-fsanitize=undefined,address"]


def run(cmd):
    res = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(res.stderr.strip()[:600])
    return res


def find_test(bench):
    """Procura tests/*/<bench>_test.c; aceita também tests/*/<bench>.c.
    O driver NÃO pode ter o mesmo nome do .c que ele inclui (incluiria a si mesmo)."""
    hits = sorted(TESTS_DIR.glob(f"*/{bench}_test.c")) or sorted(TESTS_DIR.glob(f"*/{bench}.c"))
    if not hits:
        sys.exit(f"Sem driver de teste para '{bench}' em {TESTS_DIR}/*/{bench}_test.c")
    return hits[0]


def target_name(test):
    m = re.search(r'#\s*include\s*"([^"]+\.c)"', test.read_text())
    if not m:
        sys.exit(f"{test}: não achei o #include \"<arquivo>.c\" do código sob teste")
    if m.group(1) == test.name:
        sys.exit(f"{test}: o driver inclui um arquivo com o mesmo nome dele; renomeie o driver para <benchmark>_test.c")
    return m.group(1)


def build(test, src_dirs, out, extra, exclude):
    """src_dirs: o primeiro tem prioridade (IA antes do original)."""
    out.mkdir(parents=True, exist_ok=True)
    target = target_name(test)
    files = {}
    for d in reversed(src_dirs):
        for p in d.glob("*.c"):
            if not any(fnmatch.fnmatch(p.name, pat) for pat in exclude):
                files[p.name] = p
    if target not in files:
        raise RuntimeError(f"{target} não existe nas pastas {[str(d) for d in src_dirs]}")
    files.pop(target)                       # o alvo entra pelo #include
    objs = []
    for src in [test, *files.values()]:
        obj = out / (src.stem + ".o")
        run([CC, "-o", obj, "-c", *CFLAGS, *extra, *DEFINES, *INCLUDES,
             *[f"-I{d}" for d in src_dirs], src])
        objs.append(obj)
    exe = out / "fuzz"
    run([CC, "-o", exe, *LDFLAGS, *extra, *objs, *LINK_OBJS, *LIBS])
    return exe


def execute(exe, seed, n):
    r = subprocess.run([str(exe), str(seed), str(n)], capture_output=True,
                       text=True, timeout=300)
    return r.returncode, r.stdout, r.stderr


def first_diffs(a, b, limit=3):
    la, lb = a.splitlines(), b.splitlines()
    out = []
    for i in range(max(len(la), len(lb))):
        x = la[i] if i < len(la) else "(fim)"
        y = lb[i] if i < len(lb) else "(fim)"
        if x != y:
            out.append((i + 1, x, y))
            if len(out) >= limit:
                break
    return out


def sanitizer_hit(err):
    return bool(re.search(r"runtime error|AddressSanitizer", err))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("benchmark")
    ap.add_argument("ia")
    ap.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3])
    ap.add_argument("-n", type=int, default=2000)
    ap.add_argument("--san", action="store_true")
    ap.add_argument("--exclude", action="append", default=["*standalone*"])
    a = ap.parse_args()

    test = find_test(a.benchmark)
    orig_dir = SRC_DIR / a.benchmark
    ia_dir = IA_BUILD_DIR / a.ia / a.benchmark
    if not list(orig_dir.glob("*.c")):
        sys.exit(f"Nenhum .c original em {orig_dir}. Nome do benchmark correto?")
    if not ia_dir.is_dir():
        sys.exit(f"Pasta da IA não encontrada: {ia_dir}")
    work = ia_dir / "build" / "fuzz"

    variants = [("normal", [])] + ([("sanitizers", SAN)] if a.san else [])
    rows, details, all_ok = [], [], True

    for vname, extra in variants:
        exe_o = build(test, [orig_dir], work / f"orig_{vname}", extra, a.exclude)
        try:
            exe_i = build(test, [ia_dir, orig_dir], work / f"ia_{vname}", extra, a.exclude)
        except Exception as e:
            rows.append(f"| {vname} | - | - | N | não compilou com o driver: {str(e)[:150]!r} |")
            all_ok = False
            continue
        for seed in a.seeds:
            rc_o, out_o, err_o = execute(exe_o, seed, a.n)
            rc_i, out_i, err_i = execute(exe_i, seed, a.n)
            same = (rc_o == rc_i) and (out_o == out_i)
            note = ""
            if vname == "sanitizers" and sanitizer_hit(err_i) and not sanitizer_hit(err_o):
                same, note = False, "sanitizer reportou erro só na IA"
            if rc_i != 0 and rc_o == 0:
                note = (note + f" IA terminou com código {rc_i}").strip()
            all_ok &= same
            rows.append(f"| {vname} | {seed} | {len(out_i.splitlines())} | {'S' if same else 'N'} | {note} |")
            for ln, x, y in first_diffs(out_o, out_i):
                details.append(f"- ({vname}, semente {seed}) linha {ln}:\n  - original: `{x}`\n  - IA: `{y}`")

    report = [f"# Teste diferencial {a.benchmark}: {a.ia} vs original", "",
              f"Driver: `{test}`, n={a.n}, sementes {a.seeds}", "",
              "| build | semente | linhas | iguais | observação |", "|---|---|---|---|---|",
              *rows, "", f"**Resultado geral: {'IGUAIS' if all_ok else 'DIFERENTES'}**"]
    if details:
        report += ["", "## Primeiras diferenças", *details]
    (ia_dir / "fuzz.md").write_text("\n".join(report))
    print("\n".join(report))
    print(f"\n{a.benchmark},{a.ia},fuzz={'S' if all_ok else 'N'},relatorio={ia_dir / 'fuzz.md'}")


if __name__ == "__main__":
    main()