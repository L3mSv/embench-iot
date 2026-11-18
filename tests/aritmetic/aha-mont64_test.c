/* Teste diferencial do aha-mont64 (perfil Aritmético/loops).

   O .c sob teste é incluído por texto, então o executor escolhe a versão
   (original ou da IA) pelo -I, e este arquivo enxerga as variáveis static.
   Todas as entradas vêm do gerador de common.h, com a semente em argv[1]. */
#include "../common.h"
#include "mont64.c"

static const uint64 EDGE[] = {
  0ULL, 1ULL, 2ULL, 3ULL, 0x7fffffffffffffffULL, 0x8000000000000000ULL,
  0x8000000000000001ULL, 0xfffffffffffffffeULL, 0xffffffffffffffffULL,
  0xffffffffULL, 0x100000000ULL
};
#define NEDGE (sizeof EDGE / sizeof EDGE[0])

static uint64
pick (void)
{
  uint64 r = fz_next ();
  if ((r & 3) == 0)
    return EDGE[(r >> 8) % NEDGE];
  return fz_next ();
}

int
main (int argc, char **argv)
{
  uint64_t seed = argc > 1 ? strtoull (argv[1], 0, 0) : 1;
  int n = argc > 2 ? atoi (argv[2]) : 2000;
  int k, r;

  fz_seed (seed);
  printf ("# seed=%llu n=%d\n", FZ_ULL (seed), n);

  /* 1) benchmark completo, entrada padrão */
  initialise_benchmark ();
  r = benchmark_body (1, 1);
  printf ("default errors=%d verify=%d\n", r, verify_benchmark (r));

  /* 2) benchmark completo, entradas variadas (m ímpar, a e b < m) */
  for (k = 0; k < n / 10 + 1; k++)
    {
      uint64 m = pick () | 1;
      uint64 a = pick () % m;
      uint64 b = pick () % m;
      initialise_benchmark ();
      in_m = m;
      in_a = a;
      in_b = b;
      r = benchmark_body (1, 1);
      printf ("bench m=%016llx b=%016llx a=%016llx -> errors=%d\n",
	      FZ_ULL (m), FZ_ULL (b), FZ_ULL (a), r);
    }

  /* 3) funções internas, uma a uma */
  for (k = 0; k < n; k++)
    {
      uint64 u = pick (), v = pick (), hi, lo;
      mulul64 (u, v, &hi, &lo);
      printf ("mulul64 %016llx %016llx -> %016llx %016llx\n",
	      FZ_ULL (u), FZ_ULL (v), FZ_ULL (hi), FZ_ULL (lo));
    }
  for (k = 0; k < n; k++)
    {
      uint64 z = pick (), x, y, res;
      if (z == 0)
	z = 1;
      x = pick () % z;		/* precondição: x < z */
      y = pick ();
      res = modul64 (x, y, z);
      printf ("modul64 %016llx %016llx %016llx -> %016llx\n",
	      FZ_ULL (x), FZ_ULL (y), FZ_ULL (z), FZ_ULL (res));
    }
  for (k = 0; k < n; k++)
    {
      int sh = (int) (fz_next () % 65);
      uint64 a = (sh == 64) ? 0 : (1ULL << sh);	/* 0 ou potência de 2 */
      uint64 b = pick () | 1;	/* b ímpar */
      volatile uint64 u, v;
      xbinGCD (a, b, &u, &v);
      printf ("xbinGCD %016llx %016llx -> %016llx %016llx\n",
	      FZ_ULL (a), FZ_ULL (b), FZ_ULL (u), FZ_ULL (v));
    }
  for (k = 0; k < n; k++)
    {
      uint64 m = pick () | 1;
      uint64 abar = pick () % m, bbar = pick () % m, mprime = pick ();
      uint64 res = montmul (abar, bbar, m, mprime);
      printf ("montmul %016llx %016llx %016llx %016llx -> %016llx\n",
	      FZ_ULL (abar), FZ_ULL (bbar), FZ_ULL (m), FZ_ULL (mprime),
	      FZ_ULL (res));
    }
  return 0;
}