ling - CRC32 benchmark optimization
===================================

File modified: src/crc32/crc_32.c
Optimized copy: ia_bd/src/ling/crc32/crc_32.c
(the original benchmark file was left untouched)

Techniques used
---------------

1. Inlining of the BEEBS random-number generator (rand_beebs) into
   crc32pseudo().

   The generator is a simple linear congruential generator:

     seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
     byte = seed >> 16;

   benchmark_body() always calls srand_beebs(0) immediately before
   crc32pseudo(), so crc32pseudo() can keep the generator state in a
   local variable initialized to 0 and reproduce the exact same
   1024-byte sequence.  This is bit-exact on both 32-bit and 64-bit
   targets: the intermediate product always fits the wider type and the
   final mask to 31 bits makes the results identical.

   Why it helps: removes a function call (call/ret, stack traffic and
   scheduling barriers) for each of the 1024 bytes processed per
   crc32pseudo() call (174,080 calls per benchmark() run with the
   default scale factors).  It also lets the compiler interleave the
   LCG arithmetic with the CRC table lookups: the random byte for byte
   k+n is computed while the CPU waits for the table load of byte k,
   hiding the load-to-use latency.  The generated code contains no
   calls to rand_beebs() anymore (verified with objdump).

2. Loop unrolling by 4 of the 1024-iteration CRC loop.

   1024 is a multiple of 4, so the unrolled loop covers every byte with
   no cleanup code.  Each unrolled body keeps the same sequence of
   operations: advance the generator, then one UPDC32 step.

   Why it helps: reduces loop overhead (branch, compare and increment)
   from 1024 to 256 executions per crc32pseudo() call and gives the
   compiler more independent instructions to schedule around the
   serial CRC dependency chain.

Correctness
-----------

The CRC computation itself is untouched: same table, same UPDC32
macro, same initial value 0xFFFFFFFF and same final complement.  The
only change is how the input bytes are produced (inline LCG instead of
out-of-line call), which provably yields the identical byte sequence,
hence the identical CRC.

Verification (exit code 0 means verify_benchmark passed, result 11433):
  - default build  (WARMUP_HEAT=1,  GLOBAL_SCALE_FACTOR=1):    pass
  - WARMUP_HEAT=4,   GLOBAL_SCALE_FACTOR=4:                    pass
  - WARMUP_HEAT=16,  GLOBAL_SCALE_FACTOR=1024:                 pass

Work performed is unchanged: every run still executes 1024 generator
steps and 1024 CRC table updates per crc32pseudo() call; nothing is
precomputed or skipped.  No dynamic allocation, no external libraries,
no inline assembly, no architecture-specific code.

Size impact (gcc -O2, "size" on the linked binary):
  baseline:  text=4013  data=520  bss=16
  this:      text=4181  data=520  bss=16   (+168 bytes of text,
             ~4.2%; data and RAM usage unchanged; smaller than the
             8x-unrolled reference variant at text=4197)
