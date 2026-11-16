import argparse, fnmatch, subprocess, sys
from pathlib import Path
from compare import (run, DRIVER, CC, CFLAGS, INCLUDES, LDFLAGS, LIBS,
                     LINK_OBJS, BUILD_DIR, SRC_DIR, IA_BUILD_DIR)

WRAP = r'''
extern int __real_rand_beebs(void);
int __wrap_rand_beebs(void) { return __real_rand_beebs() ^ VARY_XOR; }
'''

def build(sources, inc_dirs, scale, workdir, driver=False, xor=None):
    workdir.mkdir(parents=True, exist_ok=True)
    srcs = list(sources)
    if driver:
        d = workdir / "driver.c"; d.write_text(DRIVER); srcs.append(d)
    if xor is not None:
        w = workdir / "wrap.c"; w.write_text(WRAP); srcs.append(w)
    objs = []
    for s in srcs:
        o = workdir / (s.stem + ".o")
        run([CC, "-o", o, "-c", *CFLAGS, "-DWARMUP_HEAT=1",
             f"-DGLOBAL_SCALE_FACTOR={scale}", f"-DVARY_XOR={xor or 0}",
             *INCLUDES, *[f"-I{x}" for x in inc_dirs], s])
        objs.append(o)
    link = list(LINK_OBJS) + ([] if driver else [BUILD_DIR / "support" / "main.o"])
    ld_extra = ["-Wl,--wrap=rand_beebs"] if xor is not None else []
    exe = workdir / "exe"
    run([CC, "-o", exe, *LDFLAGS, *ld_extra, *objs, *link, *LIBS])
    return exe

def instructions(exe):
    r = subprocess.run(["sudo", "env", "LC_ALL=C", "perf", "stat", "-x,",
                        "-e", "instructions:u", "-r", "3", str(exe)],
                       capture_output=True, text=True)
    for line in r.stderr.splitlines():
        p = line.split(",")
        if len(p) >= 3 and p[2].startswith("instructions"):
            return float(p[0])
    raise RuntimeError(f"perf sem instruções:\n{r.stderr}")

def output(exe):
    r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    return r.stdout.strip() or f"(sem saída, código {r.returncode})"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("benchmark"); ap.add_argument("ia")
    ap.add_argument("--exclude", action="append", default=["*standalone*"])
    a = ap.parse_args()

    ia_dir = IA_BUILD_DIR / a.ia / a.benchmark
    orig = {p.name: p for p in (SRC_DIR / a.benchmark).glob("*.c")}
    mine = {p.name: p for p in ia_dir.glob("*.c")
            if not any(fnmatch.fnmatch(p.name, pat) for pat in a.exclude)}
    if not mine:
        sys.exit(f"Nenhum .c da IA em {ia_dir}")
    ia_src = list({**orig, **mine}.values())
    work = ia_dir / "build" / "checks"
    lines = [f"# Checks {a.benchmark}: {a.ia} vs original", ""]

    # 1) escala: instruções em 1 e 4
    lines += ["## Escala (instruções do usuário)", "",
              "| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |",
              "|---|---|---|---|---|"]
    for nome, srcs, incs in (("original", list(orig.values()), [SRC_DIR / a.benchmark]),
                             (a.ia, ia_src, [ia_dir, SRC_DIR / a.benchmark])):
        i = {s: instructions(build(srcs, incs, s, work / f"{nome}_s{s}")) for s in (1, 4)}
        lines.append(f"| {nome} | {i[1]:,.0f} | {i[4]:,.0f} | {i[4]/i[1]:.2f} | {(i[4]-i[1])/3:,.0f} |")

    # 2) variação de entrada: saída do original vs IA
    lines += ["", "## Variação de entrada (wrap em rand_beebs)", "",
              "| xor | original | IA | iguais |", "|---|---|---|---|"]
    base_out = output(build(list(orig.values()), [SRC_DIR / a.benchmark], 1,
                            work / "orig_base", driver=True))
    for k in (0x155, 0x2A3, 0x7F1):
        o = output(build(list(orig.values()), [SRC_DIR / a.benchmark], 1,
                         work / f"orig_x{k}", driver=True, xor=k))
        i = output(build(ia_src, [ia_dir, SRC_DIR / a.benchmark], 1,
                         work / f"ia_x{k}", driver=True, xor=k))
        aviso = " (entrada não variou)" if o == base_out else ""
        lines.append(f"| {hex(k)} | {o}{aviso} | {i} | {'S' if o == i else 'N'} |")

    (ia_dir / "checks.md").write_text("\n".join(lines))
    print("\n".join(lines))

if __name__ == "__main__":
    main()