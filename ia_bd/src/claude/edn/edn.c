/* BEEBS edn benchmark

   This version, copyright (C) 2014-2019 Embecosm Limited and University of
   Bristol

   Contributor James Pallister <james.pallister@bristol.ac.uk>
   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later

   Original code from: WCET Benchmarks,
       http://www.mrtc.mdh.se/projects/wcet/benchmarks.html

   Permission to license under GPL obtained by email from Björn Lisper */

/*
 * MDH WCET BENCHMARK SUITE.
 */

/************************************************************************
*	Simple vector multiply				*
************************************************************************/

/*
 * Changes: JG 2005/12/22: Inserted prototypes, changed type of main to int
 * etc. Added parenthesis in expressions in jpegdct. Removed unused variable
 * dx. Changed int to long to avoid problems when compiling to 16 bit target
 * Indented program.
 * JG 2006-01-27: Removed code in codebook
 *
 * Optimization pass (2026):
 *  - Internal helper functions marked `static` (they are never referenced
 *    outside this translation unit) so the compiler can inline/specialise
 *    them freely instead of always emitting an externally-callable symbol.
 *  - `restrict` added to pointer parameters whose call sites never pass
 *    overlapping objects, letting the compiler avoid defensive reloads and
 *    vectorise the loops.
 *  - Loop induction variables that only ever range over small, fixed,
 *    compile-time bounds (array sizes <= 200) were narrowed from
 *    `long int` to `int`. This does not change any computed value (all
 *    such bounds fit comfortably in 32 bits), but on the 64-bit host used
 *    by the test harness it avoids needless 64-bit arithmetic/registers.
 *  - No change was made to any accumulator, intermediate, or output type
 *    (dotp, sqr, sum, f, x/t, output[], etc.) and no change was made to
 *    the observable arithmetic (shifts, truncations, casts). The exact
 *    values checked by verify_benchmark() are unaffected.
 */

#include <string.h>
#include "support.h"

/* This scale factor will be changed to equalise the runtime of the
   benchmarks. */
#define LOCAL_SCALE_FACTOR 81


#define N 100
#define ORDER 50

static void vec_mpy1 (short y[restrict], const short x[restrict], short scaler);
static long int mac (const short *restrict a, const short *restrict b,
		     long int sqr, long int *restrict sum);
static void fir (const short array1[restrict], const short coeff[restrict],
		 long int output[restrict]);
static void fir_no_red_ld (const short x[restrict], const short h[restrict],
			   long int y[restrict]);
static long int latsynth (short b[restrict], const short k[restrict],
			  long int n, long int f);
static void iir1 (const short *restrict coefs, const short *restrict input,
		  long int *optr, long int *state);
static long int codebook (long int mask, long int bitchanged, long int numbasis,
			  long int codeword, long int g, const short *d,
			  short ddim, short theta);
static void jpegdct (short *restrict d, short *restrict r);

static void
vec_mpy1 (short y[restrict], const short x[restrict], short scaler)
{
  int i;

  for (i = 0; i < 150; i++)
    y[i] += ((scaler * x[i]) >> 15);
}


/*****************************************************
*			Dot Product	      *
*****************************************************/
static long int
mac (const short *restrict a, const short *restrict b, long int sqr,
     long int *restrict sum)
{
  int i;
  long int dotp = *sum;

  for (i = 0; i < 150; i++)
    {
      dotp += b[i] * a[i];
      sqr += b[i] * b[i];
    }

  *sum = dotp;
  return sqr;
}


/*****************************************************
*		FIR Filter		     *
*****************************************************/
static void
fir (const short array1[restrict], const short coeff[restrict],
     long int output[restrict])
{
  int i, j;
  long int sum;

  for (i = 0; i < N - ORDER; i++)
    {
      sum = 0;
      for (j = 0; j < ORDER; j++)
	{
	  sum += array1[i + j] * coeff[j];
	}
      output[i] = sum >> 15;
    }
}

/****************************************************
*	FIR Filter with Redundant Load Elimination

By doing two outer loops simultaneously, you can potentially  reuse data (depending on the DSP architecture).
x and h  only  need to be loaded once, therefore reducing redundant loads.
This reduces memory bandwidth and power.
*****************************************************/
static void
fir_no_red_ld (const short x[restrict], const short h[restrict],
	       long int y[restrict])
{
  int i, j;
  long int sum0, sum1;
  short x0, x1, h0, h1;
  for (j = 0; j < 100; j += 2)
    {
      sum0 = 0;
      sum1 = 0;
      x0 = x[j];
      for (i = 0; i < 32; i += 2)
	{
	  x1 = x[j + i + 1];
	  h0 = h[i];
	  sum0 += x0 * h0;
	  sum1 += x1 * h0;
	  x0 = x[j + i + 2];
	  h1 = h[i + 1];
	  sum0 += x1 * h1;
	  sum1 += x0 * h1;
	}
      y[j] = sum0 >> 15;
      y[j + 1] = sum1 >> 15;
    }
}

/*******************************************************
*	Lattice Synthesis	           *
* This function doesn't follow the typical DSP multiply two  vector operation, but it will point out the compiler's flexibility   ********************************************************/
static long int
latsynth (short b[restrict], const short k[restrict], long int n, long int f)
{
  int i;

  f -= b[n - 1] * k[n - 1];
  for (i = (int) n - 2; i >= 0; i--)
    {
      f -= b[i] * k[i];
      b[i + 1] = b[i] + ((k[i] * (f >> 16)) >> 16);
    }
  b[0] = f >> 16;
  return f;
}

/*****************************************************
*			IIR Filter		     *
*****************************************************/
static void
iir1 (const short *restrict coefs, const short *restrict input,
      long int *optr, long int *state)
{
  long int x;
  long int t;
  int n;

  x = input[0];
  for (n = 0; n < 50; n++)
    {
      t = x + ((coefs[2] * state[0] + coefs[3] * state[1]) >> 15);
      x = t + ((coefs[0] * state[0] + coefs[1] * state[1]) >> 15);
      state[1] = state[0];
      state[0] = t;
      coefs += 4;		/* point to next filter coefs  */
      state += 2;		/* point to next filter states */
    }
  *optr++ = x;
}

/*****************************************************
*	Vocoder Codebook Search 	     *
*****************************************************/
static long int
codebook (long int mask, long int bitchanged, long int numbasis,
	  long int codeword, long int g, const short *d, short ddim,
	  short theta)
/*
 * dfm (mask=d  bitchanged=1 numbasis=17  codeword=e[0] , g=d, d=a, ddim=c,
 * theta =1
 */
{
  /*
   * The loop below is intentionally left as a no-op counting loop: the
   * real body was removed upstream (see header comment) because it caused
   * a memory access exception, and the return value never depended on it.
   * It is kept (rather than deleted outright) so the function's timing
   * profile matches the original WCET benchmark; any conforming compiler
   * at -O1 or above already eliminates it as dead code.
   */
  for (long int j = bitchanged + 1; j <= numbasis; j++)
    {
    }
  return g;
}


/*****************************************************
*		JPEG Discrete Cosine Transform 		     *
*****************************************************/
static void
jpegdct (short *restrict d, short *restrict r)
{
  long int t[12];
  short i, j, k, m, n, p;
  for (k = 1, m = 0, n = 13, p = 8; k <= 8;
       k += 7, m += 3, n += 3, p -= 7, d -= 64)
    {
      for (i = 0; i < 8; i++, d += p)
	{
	  for (j = 0; j < 4; j++)
	    {
	      t[j] = d[k * j] + d[k * (7 - j)];
	      t[7 - j] = d[k * j] - d[k * (7 - j)];
	    }
	  t[8] = t[0] + t[3];
	  t[9] = t[0] - t[3];
	  t[10] = t[1] + t[2];
	  t[11] = t[1] - t[2];
	  d[0] = (t[8] + t[10]) >> m;
	  d[4 * k] = (t[8] - t[10]) >> m;
	  t[8] = (short) (t[11] + t[9]) * r[10];
	  d[2 * k] = t[8] + (short) ((t[9] * r[9]) >> n);
	  d[6 * k] = t[8] + (short) ((t[11] * r[11]) >> n);
	  t[0] = (short) (t[4] + t[7]) * r[2];
	  t[1] = (short) (t[5] + t[6]) * r[0];
	  t[2] = t[4] + t[6];
	  t[3] = t[5] + t[7];
	  t[8] = (short) (t[2] + t[3]) * r[8];
	  t[2] = (short) t[2] * r[1] + t[8];
	  t[3] = (short) t[3] * r[3] + t[8];
	  d[7 * k] = (short) (t[4] * r[4] + t[0] + t[2]) >> n;
	  d[5 * k] = (short) (t[5] * r[6] + t[1] + t[3]) >> n;
	  d[3 * k] = (short) (t[6] * r[5] + t[1] + t[2]) >> n;
	  d[1 * k] = (short) (t[7] * r[7] + t[0] + t[3]) >> n;
	}
    }
}

static short a[200];
static short b[200];
static short c;
static long int d;
static int e;
static long int output[200];


void
initialise_benchmark (void)
{
}


static int  benchmark_body(unsigned int lsf, unsigned int gsf);

void
warm_caches (int  heat)
{
  int  res = benchmark_body (1, heat);

  return;
}


int
benchmark (void)
{
  return benchmark_body (LOCAL_SCALE_FACTOR, GLOBAL_SCALE_FACTOR);
}


static int __attribute__ ((noinline))
benchmark_body(unsigned int lsf, unsigned int gsf)
{
  int j;

  for (unsigned int lsf_cnt = 0; lsf_cnt < lsf; lsf_cnt++)
    for (unsigned int gsf_cnt = 0; gsf_cnt < gsf; gsf_cnt++)
      {
	short unsigned int in_a[200] = {
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400,
	  0x0000, 0x07ff, 0x0c00, 0x0800, 0x0200, 0xf800, 0xf300, 0x0400
	};
	short unsigned int in_b[200] = {
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000,
	  0x0c60, 0x0c40, 0x0c20, 0x0c00, 0xf600, 0xf400, 0xf200, 0xf000
	};
	c = 0x3;
	d = 0xAAAA;
	e = 0xEEEE;

	for (int i = 0; i < 200; i++)
	  {
	    a[i] = in_a[i];
	    b[i] = in_b[i];
	  }
	/*
	 * Declared as memory variable so it doesn't get optimized out
	 */

	vec_mpy1 (a, b, c);
	c = mac (a, b, (long int) c, (long int *) output);
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
verify_benchmark (int unused)
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


/*
   Local Variables:
   mode: C
   c-file-style: "gnu"
   End:
*/
