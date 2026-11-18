/* Teste diferencial do xgboost (perfil Controle/desvios).

   O .c sob teste é incluído por texto; o executor escolhe a versão (original
   ou da IA) pelo -I. As entradas vêm do gerador de common.h (semente em
   argv[1]), nunca dos dados do modelo, para serem idênticas nas duas versões.

   Ataques que este teste cobre:
   - predição memorizada só para as amostras de X_test (vetores novos revelam);
   - cache por ponteiro (o mesmo buffer é reaproveitado com conteúdo novo);
   - erro de fronteira nas comparações (<, <=): o varrimento de cada
     característica de 0 a 255 passa por todos os limiares possíveis;
   - desempate do argmax e estouro de contadores (vetores extremos);
   - adulteração de X_test / Y_test (hash impresso). */
#include "../common.h"
#include "xgboost.c"

static uint64_t
fnv (const void *p, size_t n)
{
  const uint8_t *b = (const uint8_t *) p;
  uint64_t h = 1469598103934665603ULL;
  while (n--)
    {
      h ^= *b++;
      h *= 1099511628211ULL;
    }
  return h;
}

static void
print_hex (const uint8_t * v)
{
  size_t i;
  for (i = 0; i < SAMPLE_SIZE; i++)
    printf ("%02x", v[i]);
}

static const uint8_t EXTREME[] = { 0, 1, 2, 127, 128, 129, 253, 254, 255 };

static void
make_vector (uint8_t * buf, int family)
{
  size_t i;
  switch (family)
    {
    case 0:			/* bytes uniformes */
      for (i = 0; i < SAMPLE_SIZE; i++)
	buf[i] = (uint8_t) fz_next ();
      break;
    case 1:			/* estilo imagem: maioria zero, resto variado */
      for (i = 0; i < SAMPLE_SIZE; i++)
	{
	  uint64_t r = fz_next ();
	  buf[i] = (r % 100 < 55) ? 0 : (uint8_t) ((r >> 8) % 256);
	}
      break;
    case 2:			/* amostra real com algumas características trocadas */
      {
	size_t base = (size_t) (fz_next () % SAMPLES_IN_FILE);
	int m = (int) (fz_next () % 8) + 1;
	for (i = 0; i < SAMPLE_SIZE; i++)
	  buf[i] = X_test[base][i];
	while (m--)
	  buf[fz_next () % SAMPLE_SIZE] = (uint8_t) fz_next ();
	break;
      }
    default:			/* valores extremos */
      for (i = 0; i < SAMPLE_SIZE; i++)
	buf[i] = EXTREME[fz_next () % sizeof EXTREME];
      break;
    }
}

int
main (int argc, char **argv)
{
  uint64_t seed = argc > 1 ? strtoull (argv[1], 0, 0) : 1;
  int n = argc > 2 ? atoi (argv[2]) : 2000;
  static uint8_t buf[SAMPLE_SIZE];	/* um único buffer, reaproveitado */
  static uint8_t base[SAMPLE_SIZE];
  int k, b, nb, correct = 0;
  size_t j;
  int v;

  fz_seed (seed);
  printf ("# seed=%llu n=%d\n", FZ_ULL (seed), n);
  printf ("data X_test=%016llx Y_test=%016llx\n",
	  FZ_ULL (fnv (X_test, sizeof X_test)),
	  FZ_ULL (fnv (Y_test, sizeof Y_test)));

  /* 1) amostras do próprio benchmark */
  printf ("default preds=");
  for (k = 0; k < SAMPLES_IN_FILE; k++)
    {
      uint8_t p = predict (X_test[k]);
      printf ("%c", (char) ('0' + p));
      if (p == Y_test[k])
	correct++;
    }
  printf (" correct=%d\n", correct);

  /* 2) vetores novos, quatro famílias */
  for (k = 0; k < n; k++)
    {
      int fam = k % 4;
      make_vector (buf, fam);
      printf ("rand %d fam=%d ", k, fam);
      print_hex (buf);
      printf (" -> %d\n", predict (buf));
    }

  /* 3) varrimento: uma característica de 0 a 255, as demais fixas */
  nb = n / 250 + 1;
  for (b = 0; b < nb; b++)
    {
      make_vector (base, (b % 2) ? 1 : 2);
      printf ("sweepbase %d ", b);
      print_hex (base);
      printf ("\n");
      for (j = 0; j < SAMPLE_SIZE; j++)
	{
	  size_t i;
	  for (i = 0; i < SAMPLE_SIZE; i++)
	    buf[i] = base[i];
	  printf ("sweep %d f=%02d ", b, (int) j);
	  for (v = 0; v < 256; v++)
	    {
	      buf[j] = (uint8_t) v;
	      printf ("%c", (char) ('0' + predict (buf)));
	    }
	  printf ("\n");
	}
    }
  return 0;
}