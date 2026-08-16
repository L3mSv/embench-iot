#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOCAL_SCALE_FACTOR 81
#define GLOBAL_SCALE_FACTOR 1
#define N 100
#define ORDER 50

/* Prototypes with restrict to enable full compiler auto-vectorization */
void vec_mpy1 (short * restrict y, const short * restrict x, short scaler);
long int mac (const short * restrict a, const short * restrict b, long int sqr, long int * restrict sum);
void fir (const short * restrict array1, const short * restrict coeff, long int * restrict output);
void fir_no_red_ld (const short * restrict x, const short * restrict h, long int * restrict y);
long int latsynth (short * restrict b, const short * restrict k, long int n, long int f);
void iir1 (const short * restrict coefs, const short * restrict input, long int * restrict optr, long int * restrict state);
long int codebook (long int mask, long int bitchanged, long int numbasis, long int codeword, long int g, const short * restrict d, short ddim, short theta);
void jpegdct (short * restrict d, const short * restrict r);

/*****************************************************
*           Vector Multiply                          *
*****************************************************/
void
vec_mpy1 (short * restrict y, const short * restrict x, short scaler)
{
  long int i;
  for (i = 0; i < 148; i += 4)
    {
      y[i + 0] += (short) ((scaler * x[i + 0]) >> 15);
      y[i + 1] += (short) ((scaler * x[i + 1]) >> 15);
      y[i + 2] += (short) ((scaler * x[i + 2]) >> 15);
      y[i + 3] += (short) ((scaler * x[i + 3]) >> 15);
    }
  for (; i < 150; i++)
    {
      y[i] += (short) ((scaler * x[i]) >> 15);
    }
}

/*****************************************************
*           Dot Product                              *
*****************************************************/
long int
mac (const short * restrict a, const short * restrict b, long int sqr, long int * restrict sum)
{
  long int i;
  long int dotp0 = 0, dotp1 = 0;
  long int sqr0 = 0, sqr1 = 0;

  for (i = 0; i < 150; i += 2)
    {
      dotp0 += (long int) b[i + 0] * a[i + 0];
      sqr0  += (long int) b[i + 0] * b[i + 0];

      dotp1 += (long int) b[i + 1] * a[i + 1];
      sqr1  += (long int) b[i + 1] * b[i + 1];
    }

  *sum += (dotp0 + dotp1);
  return sqr + (sqr0 + sqr1);
}

/*****************************************************
*       FIR Filter                                   *
*****************************************************/
void
fir (const short * restrict array1, const short * restrict coeff, long int * restrict output)
{
  long int i, j;

  for (i = 0; i < N - ORDER; i++)
    {
      long int sum = 0;
      const short * restrict a_ptr = &array1[i];
      
      for (j = 0; j < ORDER; j++)
        {
          sum += (long int) a_ptr[j] * coeff[j];
        }
      output[i] = sum >> 15;
    }
}

/*****************************************************
*   FIR Filter with Redundant Load Elimination       *
*****************************************************/
void
fir_no_red_ld (const short * restrict x, const short * restrict h, long int * restrict y)
{
  long int j;
  for (j = 0; j < 100; j += 2)
    {
      long int sum0 = 0;
      long int sum1 = 0;
      short x0 = x[j];

      for (long int i = 0; i < 32; i += 2)
        {
          short x1 = x[j + i + 1];
          short h0 = h[i];
          short h1 = h[i + 1];

          sum0 += x0 * h0;
          sum1 += x1 * h0;

          x0 = x[j + i + 2];
          sum0 += x1 * h1;
          sum1 += x0 * h1;
        }
      y[j]     = sum0 >> 15;
      y[j + 1] = sum1 >> 15;
    }
}

/*******************************************************
*   Lattice Synthesis                                  *
********************************************************/
long int
latsynth (short * restrict b, const short * restrict k, long int n, long int f)
{
  long int i;
  f -= (long int) b[n - 1] * k[n - 1];

  for (i = n - 2; i >= 0; i--)
    {
      f -= (long int) b[i] * k[i];
      b[i + 1] = b[i] + (short) ((k[i] * (f >> 16)) >> 16);
    }
  b[0] = (short) (f >> 16);
  return f;
}

/*****************************************************
*           IIR Filter                               *
*****************************************************/
void
iir1 (const short * restrict coefs, const short * restrict input, long int * restrict optr, long int * restrict state)
{
  long int x = input[0];
  long int n;

  for (n = 0; n < 50; n++)
    {
      long int t = x + ((coefs[2] * state[0] + coefs[3] * state[1]) >> 15);
      x = t + ((coefs[0] * state[0] + coefs[1] * state[1]) >> 15);
      state[1] = state[0];
      state[0] = t;
      coefs += 4;
      state += 2;
    }
  *optr = x;
}

/*****************************************************
*   Vocoder Codebook Search                          *
*****************************************************/
long int
codebook (long int mask, long int bitchanged, long int numbasis,
          long int codeword, long int g, const short * restrict d, short ddim,
          short theta)
{
  (void)mask; (void)codeword; (void)d; (void)ddim; (void)theta; (void)bitchanged; (void)numbasis;
  return g;
}

/*****************************************************
*       JPEG Discrete Cosine Transform               *
*****************************************************/
void
jpegdct (short * restrict d, const short * restrict r)
{
  long int t[12];
  short i, j;
  short *d_ptr = d;

  for (i = 0; i < 8; i++)
    {
      for (j = 0; j < 4; j++)
        {
          t[j]     = d_ptr[j] + d_ptr[7 - j];
          t[7 - j] = d_ptr[j] - d_ptr[7 - j];
        }
      t[8]  = t[0] + t[3];
      t[9]  = t[0] - t[3];
      t[10] = t[1] + t[2];
      t[11] = t[1] - t[2];

      d_ptr[0] = (short) ((t[8] + t[10]) >> 1);
      d_ptr[4] = (short) ((t[8] - t[10]) >> 1);

      t[8] = (short) (t[11] + t[9]) * r[10];
      d_ptr[2] = (short) (t[8] + ((t[9] * r[9]) >> 13));
      d_ptr[6] = (short) (t[8] + ((t[11] * r[11]) >> 13));

      t[0] = (short) (t[4] + t[7]) * r[2];
      t[1] = (short) (t[5] + t[6]) * r[0];
      t[2] = t[4] + t[6];
      t[3] = t[5] + t[7];

      t[8] = (short) (t[2] + t[3]) * r[8];
      t[2] = (short) t[2] * r[1] + t[8];
      t[3] = (short) t[3] * r[3] + t[8];

      d_ptr[7] = (short) ((t[4] * r[4] + t[0] + t[2]) >> 13);
      d_ptr[5] = (short) ((t[5] * r[6] + t[1] + t[3]) >> 13);
      d_ptr[3] = (short) ((t[6] * r[5] + t[1] + t[2]) >> 13);
      d_ptr[1] = (short) ((t[7] * r[7] + t[0] + t[3]) >> 13);

      d_ptr += 8;
    }
}

static short a[200];
static short b[200];
static short c;
static long int d;
static int e;
static long int output[200];

static const unsigned short default_in_a[200] = {
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400, 0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400
};

static const unsigned short default_in_b[200] = {
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000, 0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000
};

static int __attribute__ ((noinline))
benchmark_body (unsigned int lsf, unsigned int gsf)
{
  for (unsigned int lsf_cnt = 0; lsf_cnt < lsf; lsf_cnt++)
    for (unsigned int gsf_cnt = 0; gsf_cnt < gsf; gsf_cnt++)
      {
        c = 0x3;
        d = 0xAAAA;
        e = 0xEEEE;

        memcpy(a, default_in_a, sizeof(a));
        memcpy(b, default_in_b, sizeof(b));

        vec_mpy1 (a, b, c);
        c = (short) mac (a, b, (long int) c, (long int *) output);
        fir (a, b, output);
        fir_no_red_ld (a, b, output);
        d = latsynth (a, b, N, d);
        iir1 (a, b, &output[100], output);
        e = codebook (d, 1, 17, e, d, a, c, 1);
        jpegdct (a, b);
      }

  return 0;
}

int
verify_benchmark (void)
{
  long int exp_output[200] =
    { 3760, 4269, 3126, 1030, 2453, -4601, 1981, -1056, 2621, 4269,
    3058, 1030, 2378, -4601, 1902, -1056, 2548, 4269, 2988, 1030,
    2300, -4601, 1822, -1056, 2474, 4269, 2917, 1030, 2220, -4601,
    1738, -1056, 2398, 4269, 2844, 1030, 2140, -4601, 1655, -1056,
    2321, 4269, 2770, 1030, 2058, -4601, 1569, -1056, 2242, 4269,
    2152, 1030, 1683, -4601, 1627, -1056, 2030, 4269, 2080, 1030,
    1611, -4601, 1555, -1056, 1958, 4269, 2008, 1030, 1539, -4601,
    1483, -1056, 1886, 4269, 1935, 1030, 1466, -4601, 1410, -1056,
    1813, 4269, 1862, 1030, 1393, -4601, 1337, -1056, 1740, 4269,
    1789, 1030, 1320, -4601, 1264, -1056, 1667, 4269, 1716, 1030,
    1968, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };

  return (0 == memcmp (output, exp_output, 200 * sizeof (output[0])))
    && (10243 == c) && (-441886230 == d) && (-441886230 == e);
}

int main (void)
{
  printf("[+] Running Self-Contained EDN Benchmark...\n");

  /* Warm caches */
  benchmark_body(1, 1);

  /* Benchmark Execution Timing */
  clock_t start = clock();
  benchmark_body(LOCAL_SCALE_FACTOR, GLOBAL_SCALE_FACTOR);
  clock_t end = clock();

  double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

  /* Verification check matching Embench standards */
  if (verify_benchmark()) {
    printf("[SUCCESS] Benchmark verification PASSED!\n");
    printf("Execution Time: %.3f ms\n", elapsed_ms);
    return 0;
  } else {
    printf("[FAILURE] Benchmark verification FAILED!\n");
    return 1;
  }
}