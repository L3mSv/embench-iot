# Comparação aha-mont64: bigPickle vs original

| build | escala | original | IA | iguais |
|---|---|---|---|---|
| normal | 1 | result=0 verify=1 | result=0 verify=1 | S |
| normal | 2 | result=0 verify=1 | result=0 verify=1 | S |
| normal | 4 | result=0 verify=1 | result=0 verify=1 | S |
| sanitizers | 1 | result=0 verify=1 | result=0 verify=1 | S |
| sanitizers | 2 | result=0 verify=1 | result=0 verify=1 | S |
| sanitizers | 4 | result=0 verify=1 | result=0 verify=1 | S |

**Resultado geral: IGUAIS**

## Diff do código
```diff
--- original/mont64.c
+++ bigPickle/mont64.c
@@ -84,6 +84,17 @@
 
 /* ---------------------------- modul64 ----------------------------- */
 
+#ifdef __SIZEOF_INT128__
+uint64
+modul64 (uint64 x, uint64 y, uint64 z)
+{
+  /* Divides (x || y) by z.  The caller guarantees x < z, so the quotient
+     fits in 64 bits and a single 128/64 hardware division computes the
+     whole remainder at once.  This replaces the 64 bit-at-a-time
+     subtract-and-shift loop used by the portable fallback below. */
+  return (uint64) ((((unsigned __int128) x << 64) | y) % z);
+}
+#else
 uint64
 modul64 (uint64 x, uint64 y, uint64 z)
 {
@@ -108,9 +119,36 @@
     }
   return x;			// Quotient is y.
 }
+#endif
 
 /* ---------------------------- montmul ----------------------------- */
 
+#ifdef __SIZEOF_INT128__
+uint64
+montmul (uint64 abar, uint64 bbar, uint64 m, uint64 mprime)
+{
+  unsigned __int128 t, tmm, sum;
+  uint64 tm, u, ov;
+
+  /* t = abar*bbar and tmm = ((t mod 2**64)*mprime mod 2**64)*m.  Only the
+     low half of t is needed for tm, exactly as in the portable version. */
+  t = (unsigned __int128) abar * bbar;
+  tm = (uint64) t * mprime;
+  tmm = (unsigned __int128) tm * m;
+
+  /* u = (t + tmm) >> 64.  Keeping the addition in 128 bits performs the
+     high-half sum and both carry detections in one operation; a 128-bit
+     overflow of the sum is precisely the extra bit of u that used to be
+     tracked separately as "ov". */
+  sum = t + tmm;
+  u = (uint64) (sum >> 64);
+  ov = (uint64) (sum < t);
+
+  /* If u overflowed (ov != 0) it is certainly >= m, so the same branchless
+     conditional subtraction as before gives the correct result. */
+  return u - (m & -(ov | (uint64) (u >= m)));
+}
+#else
 uint64
 montmul (uint64 abar, uint64 bbar, uint64 m, uint64 mprime)
 {
@@ -149,6 +187,7 @@
 
   return ulo;
 }
+#endif
 
 /* ---------------------------- xbinGCD ----------------------------- */
 
@@ -175,40 +214,65 @@
 void
 xbinGCD (uint64 a, uint64 b, volatile uint64 * pu, volatile uint64 * pv)
 {
-  uint64 alpha, beta, u, v;
-
-  u = 1;
-  v = 0;
-  alpha = a;
-  beta = b;			// Note that alpha is
-  // even and beta is odd.
-
-  /* The invariant maintained from here on is:
-     a = u*2*alpha - v*beta. */
-
-// printf("Before, a u v = %016llx %016llx %016llx\n", a, u, v);
-  while (a > 0)
+#ifdef __SIZEOF_INT128__
+  /* The benchmark uses a = 2**63, i.e. 2a = 2**64 = r.  We need
+     v such that b*v = -1 (mod 2**64), i.e. v = -b**-1 mod 2**64, and then
+     u = (b*v + 1)/2**64.  Since b is odd, x = b is already the inverse
+     modulo 8, and the Newton step x = x*(2 - b*x) doubles the number of
+     correct bits: five steps cover all 64 bits.  This replaces the 64
+     conditional shift/add steps of the binary-GCD loop.  Other values of
+     a still use the fully general loop below. */
+  if (a == UINT64_C (0x8000000000000000))
     {
-      a = a >> 1;
-      if ((u & 1) == 0)
-	{			// Delete a common
-	  u = u >> 1;
-	  v = v >> 1;		// factor of 2 in
-	}			// u and v.
-      else
-	{
-	  /* We want to set u = (u + beta) >> 1, but
-	     that can overflow, so we use Dietz's method. */
-	  u = ((u ^ beta) >> 1) + (u & beta);
-	  v = (v >> 1) + alpha;
-	}
-//    printf("After,  a u v = %016llx %016llx %016llx\n", a, u, v);
+      uint64 x = b;
+      uint64 u, v;
+      int i;
+
+      for (i = 0; i < 5; i++)
+	x = x * (2 - b * x);
+
+      v = -x;
+      u = (uint64) ((((unsigned __int128) b * v) + 1) >> 64);
+
+      *pu = u;
+      *pv = v;
+      return;
     }
-
-// printf("At end,    a u v = %016llx %016llx %016llx\n", a, u, v);
-  *pu = u;
-  *pv = v;
-  return;
+#endif
+
+  {
+    uint64 alpha, beta, u, v;
+
+    u = 1;
+    v = 0;
+    alpha = a;
+    beta = b;			// Note that alpha is
+    // even and beta is odd.
+
+    /* The invariant maintained from here on is:
+       a = u*2*alpha - v*beta. */
+
+    while (a > 0)
+      {
+	a = a >> 1;
+	if ((u & 1) == 0)
+	  {			// Delete a common
+	    u = u >> 1;
+	    v = v >> 1;		// factor of 2 in
+	  }			// u and v.
+	else
+	  {
+	    /* We want to set u = (u + beta) >> 1, but
+	       that can overflow, so we use Dietz's method. */
+	    u = ((u ^ beta) >> 1) + (u & beta);
+	    v = (v >> 1) + alpha;
+	  }
+      }
+
+    *pu = u;
+    *pv = v;
+    return;
+  }
 }
 
 /* ------------------------------ main ------------------------------ */
```