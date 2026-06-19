import csv
import re
import subprocess
import sys
from pathlib import Path

BUILD_DIR = Path("bd")
REPETITIONS = 1000


def find_benchmark(benchname):
    bench_executable = BUILD_DIR / "src" / benchname / benchname
    if bench_executable.is_file():
        return bench_executable
    raise FileNotFoundError(f"Benchmark not found at {bench_executable}")


def measure_size(executable):
    res = subprocess.run(["size", executable], capture_output=True, text=True)
    lines = res.stdout.strip().splitlines()
    sizes = lines[1].split()
    return int(sizes[0]), int(sizes[1]), int(sizes[2])


def measure_perf(executable):
    cmd = [
        "perf",
        "stat",
        "-x,",
        "-r",
        str(REPETITIONS),
        "-e",
        "duration_time,cycles,instructions",
        str(executable),
    ]

    res = subprocess.run(
        cmd, capture_output=True, text=True, env={"LC_ALL": "C"}
    )

    metrics = {}
    for line in res.stderr.splitlines():
        parts = line.split(",")
        if len(parts) >= 3:
            val, metric = parts[0], parts[2]
            if val.isdigit() or "." in val:
                metrics[metric] = float(val)

    # Convert duration_time (nanoseconds) to milliseconds
    time_ms = (metrics.get("duration_time", 0)) / 1_000_000
    return time_ms, int(metrics.get("cycles", 0)), int(metrics.get("instructions", 0))


def measure_dynamic_memory(executable):
    res = subprocess.run(
        ["valgrind", str(executable)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        env={"LC_ALL": "C"},
    )

    match = re.search(
        r"total heap usage:.*, ([\d,]+) bytes allocated", res.stderr
    )
    if match:
        return int(match.group(1).replace(",", ""))
    return 0


def main():
    if len(sys.argv) < 2:
        print("Usage: script2.py <benchmark1> ...")
        sys.exit(1)

    fieldnames = [
        "benchmark",
        "time_ms",
        "cycles",
        "instructions",
        "text",
        "data",
        "bss",
        "ram_data_bss",
        "dynamic_mem_bytes",
    ]

    writer = csv.DictWriter(sys.stdout, fieldnames=fieldnames)
    writer.writeheader()

    for bench in sys.argv[1:]:
        try:
            exe = find_benchmark(bench)

            time_ms, cycles, insts = measure_perf(exe)
            text, data, bss = measure_size(exe)
            dyn_mem = measure_dynamic_memory(exe)

            writer.writerow(
                {
                    "benchmark": bench,
                    "time_ms": time_ms,
                    "cycles": cycles,
                    "instructions": insts,
                    "text": text,
                    "data": data,
                    "bss": bss,
                    "ram_data_bss": data + bss,
                    "dynamic_mem_bytes": dyn_mem,
                }
            )
        except Exception as e:
            print(f"Error processing {bench}: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()
