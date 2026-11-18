/* Utilidades comuns dos testes diferenciais (não dependem de support/). */
#ifndef FUZZ_COMMON_H
#define FUZZ_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t fz_state = 0x9E3779B97F4A7C15ULL;

/* xorshift64*: gerador próprio, independente de rand_beebs. */
static uint64_t
fz_next (void)
{
  fz_state ^= fz_state >> 12;
  fz_state ^= fz_state << 25;
  fz_state ^= fz_state >> 27;
  return fz_state * 0x2545F4914F6CDD1DULL;
}

static void
fz_seed (uint64_t s)
{
  int i;
  fz_state = s ? s : 0x9E3779B97F4A7C15ULL;
  for (i = 0; i < 16; i++)
    fz_next ();
}

#define FZ_ULL(x) ((unsigned long long) (x))
#endif