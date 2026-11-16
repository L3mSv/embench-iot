import argparse, difflib, fnmatch, subprocess, sys
from pathlib import Path

BUILD_DIR = Path("bd")
IA_BUILD_DIR = Path("ia_bd") / "src"
SRC_DIR = Path("src")
CC = "gcc"
CFLAGS = ["-O2", "-fdata-sections", "-ffunction-sections"]
INCLUDES = ["-Isupport", "-Iexamples/native/speed"]
LDFLAGS = ["-O2", "-Wl,-gc-sections"]
LIBS = ["-lm"]
LINK_OBJS = [BUILD_DIR / "support" / "beebsc.o", BUILD_DIR / "config" / "boardsupport.o"]

DRIVER = r'''
#include <stdio.h>
#include "support.h"
int main(void) {
    initialise_benchmark();
    warm_caches(1);
    int r = benchmark();
    int v = verify_benchmark(r);
    printf("result=%d verify=%d\n", r, v);
    return 0;
}
'''

def run(cmd):
    res = subprocess.run([str(c) for c in cmd], capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"Falhou: {' '.join(map(str, cmd))}\n{res.stderr}")
    return res

def build_and_run(sources, inc_dirs, scale, workdir, extra):
    workdir.mkdir(parents=True, exist_ok=True)
    drv = workdir / "driver.c"
    drv.write_text(DRIVER)
    objs = []
    for src in [*sources, drv]:
        obj = workdir / (src.stem + ".o")
        run([CC, "-o", obj, "-c", *CFLAGS, *extra, "-DWARMUP_HEAT=1",
             f"-DGLOBAL_SCALE_FACTOR={scale}", *INCLUDES,
             *[f"-I{d}" for d in inc_dirs], src])
        objs.append(obj)
    exe = workdir / "cmp"
    run([CC, "-o", exe, *LDFLAGS, *extra, *objs, *LINK_OBJS, *LIBS])
    r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    return r.stdout.strip() or f"(sem saída, código {r.returncode})", r.stderr.strip()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("benchmark")
    ap.add_argument("ia")
    ap.add_argument("--scales", type=int, nargs="+", default=[1, 2, 4])
    ap.add_argument("--san", action="store_true", help="roda também com sanitizers")
    ap.add_argument("--exclude", action="append", default=["*standalone*"])
    a = ap.parse_args()

    ia_dir = IA_BUILD_DIR / a.ia / a.benchmark
    orig = {p.name: p for p in (SRC_DIR / a.benchmark).glob("*.c")}
    mine = {p.name: p for p in ia_dir.glob("*.c")
            if not any(fnmatch.fnmatch(p.name, pat) for pat in a.exclude)}
    if not mine:
        sys.exit(f"Nenhum .c da IA em {ia_dir}")
    ia_sources = list({**orig, **mine}.values())
    work = ia_dir / "build" / "compare"

    variants = [("normal", [])]
    if a.san:
        variants.append(("sanitizers", ["-O1", "-g", "-fsanitize=undefined,address"]))

    rows, all_equal = [], True
    for vname, extra in variants:
        for s in a.scales:
            try:
                o_out, _ = build_and_run(list(orig.values()), [SRC_DIR / a.benchmark], s,
                                         work / f"orig_{vname}_{s}", extra)
                i_out, i_err = build_and_run(ia_sources, [ia_dir, SRC_DIR / a.benchmark], s,
                                             work / f"ia_{vname}_{s}", extra)
            except Exception as e:
                o_out, i_out, i_err = "-", f"ERRO: {str(e)[:120]}", ""
            same = (o_out == i_out) and not (vname == "sanitizers" and i_err)
            all_equal &= same
            note = " (sanitizer reportou erro)" if vname == "sanitizers" and i_err else ""
            rows.append(f"| {vname} | {s} | {o_out} | {i_out}{note} | {'S' if same else 'N'} |")

    diffs = []
    for name, p in mine.items():
        a_lines = orig[name].read_text().splitlines() if name in orig else []
        b_lines = p.read_text().splitlines()
        diffs.append("\n".join(difflib.unified_diff(a_lines, b_lines, f"original/{name}",
                                                    f"{a.ia}/{name}", lineterm="")))

    report = [f"# Comparação {a.benchmark}: {a.ia} vs original", "",
              "| build | escala | original | IA | iguais |", "|---|---|---|---|---|", *rows, "",
              f"**Resultado geral: {'IGUAIS' if all_equal else 'DIFERENTES'}**", "",
              "## Diff do código", "```diff", *diffs, "```"]
    (ia_dir / "compare.md").write_text("\n".join(report))
    print(f"{a.benchmark},{a.ia},saida_identica={'S' if all_equal else 'N'},relatorio={ia_dir/'compare.md'}")

if __name__ == "__main__":
    main()