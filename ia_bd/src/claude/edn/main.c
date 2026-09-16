/* Standalone harness for running a single Embench/BEEBS-style benchmark
 * (like edn.c) directly with gcc, without the rest of the Embench build
 * system. Mirrors, at a much smaller scale, what Embench's own main.c
 * does: initialise, warm the caches, run the benchmark, then verify it
 * produced the right answer.
 *
 * Build:
 *   gcc -O2 edn.c main.c -o edn
 *
 * Run (as your script2.py already expects):
 *   perf stat -x, -r 1000 -e duration_time,cycles,instructions ./edn
 */

#include <stdio.h>
#include "support.h"

int
main (void)
{
  int result;
  int ok;

  initialise_benchmark ();
  warm_caches (1);

  result = benchmark ();
  ok = verify_benchmark (result);

  if (!ok)
    {
      fprintf (stderr, "FAIL: verify_benchmark did not match expected output\n");
      return 1;
    }

  return 0;
}
