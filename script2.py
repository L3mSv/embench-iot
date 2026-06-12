import csv
import re
import subprocess
import sys
import time
from pathlib import Path

BUILD_DIR = Path("bd")

def find_benchmark(benchname):
    bench_dir = BUILD_DIR / "src" / benchname
    bench_executable = bench_dir / benchname
    if bench_executable.is_file():
        return bench_executable
    raise FileNotFoundError(f"Benchmark {benchname} not found at {bench_executable}")

def measure_size(executable):
    result = subprocess.run(['size', executable], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Failed to run size on {executable}: {result.stderr}")
    
    # Parse the output of size
    lines = result.stdout.strip().splitlines()
    if len(lines) < 2:
        raise ValueError(f"Unexpected output from size: {result.stdout}")
    
    # The second line contains the sizes
    size_info = lines[1].split()
    if len(size_info) < 4:
        raise ValueError(f"Unexpected output from size: {result.stdout}")
    
    text_size = int(size_info[0])
    data_size = int(size_info[1])
    bss_size = int(size_info[2])
    
    return text_size, data_size, bss_size

def measure_time(executable, repetitions):
    total_time = 0.0
    for _ in range(repetitions):
        start_time = time.perf_counter()
        subprocess.run([executable], check=True)
        end_time = time.perf_counter()
        total_time += (end_time - start_time)
    
    avg_time_ms = (total_time / repetitions) * 1000
    return avg_time_ms

def main():
    if len(sys.argv) < 2:
        print("Usage: script2.py <benchmark1> [benchmark2 ...]")
        sys.exit(1)

    benchmarks = sys.argv[1:]
    repetitions = 5

    results = []
    for bench in benchmarks:
        try:
            executable = find_benchmark(bench)
            avg_time_ms = measure_time(executable, repetitions)
            text_size, data_size, bss_size = measure_size(executable)
            results.append({
                "benchmark": bench,
                "time_ms": avg_time_ms,
                "text": text_size,
                "data": data_size,
                "bss": bss_size,
                "ram_data_bss": data_size + bss_size
            })
        except Exception as e:
            print(f"Error processing benchmark {bench}: {e}", file=sys.stderr)

    # Print results as CSV
    writer = csv.DictWriter(sys.stdout, fieldnames=["benchmark", "time_ms", "text", "data", "bss", "ram_data_bss"])
    writer.writeheader()
    for result in results:
        writer.writerow(result)

if __name__ == "__main__":
    sys.exit(main())
