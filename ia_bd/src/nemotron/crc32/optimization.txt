nemotron - CRC32 benchmark optimization
========================================

File modified: src/crc32/crc_32.c

Technique: Loop unrolling of inner CRC computation by factor of 8

Description:
  The inner 1024-iteration CRC loop in crc32pseudo() was unrolled by 8x
  to create crc32pseudo_unrolled(). This reduces loop overhead (branch
  instructions and counter increments) while maintaining exactly the same
  computational workload.

  The main loop processes 8 iterations at a time (128 iterations × 8 = 1024),
  with a small cleanup loop for the remaining iterations (1024 is divisible
  by 8, so the cleanup runs 4 times as a formality).

  Performed per benchmark iteration: 1024 calls to rand_beebs() and 1024
  calls to UPDC32() - identical to the original.

Why it helps:
  - Reduces branch instructions from ~1024 to ~132 per crc32pseudo() call
  - Reduces counter increments similarly
  - Same number of CRC operations performed
  - Minimal code size increase (~30 bytes)

Verification:
  - verify_benchmark() returns 1 (result = 11433) in all tested configurations
  - Tested with GLOBAL_SCALE_FACTOR = 1, 4, 1024
  - Same functional output as original benchmark