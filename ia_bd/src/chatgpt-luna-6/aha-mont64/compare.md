# Comparação aha-mont64: chatgpt-luna-6 vs original

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
+++ chatgpt-luna-6/mont64.c
@@ -84,6 +84,29 @@
 
 /* ---------------------------- modul64 ----------------------------- */
 
+#ifdef __SIZEOF_INT128__
+uint64
+modul64 (uint64 x, uint64 y, uint64 z)
+{
+  /* The caller supplies x < z, so the 128-bit dividend's quotient fits in
+     64 bits. Use a wide remainder instead of 64 serial shift/subtract steps. */
+  return (uint64) ((((unsigned __int128) x << 64) | y) % z);
+}
+#else
+#define MODUL64_STEP()                                                \
+  do                                                                  \
+    {                                                                 \
+      t = 0 - (x >> 63);                                              \
+      x = (x << 1) | (y >> 63);                                      \
+      y <<= 1;                                                        \
+      if ((x | t) >= z)                                               \
+	{                                                               \
+	  x -= z;                                                      \
+	  ++y;                                                         \
+	}                                                               \
+    }                                                                 \
+  while (0)
+
 uint64
 modul64 (uint64 x, uint64 y, uint64 z)
 {
@@ -93,24 +116,48 @@
      Must have x < z (to get a 64-bit result). This is
      checked for. */
 
-  int64 i, t;
-
-  for (i = 1; i <= 64; i++)
-    {				// Do 64 times.
-      t = (int64) x >> 63;	// All 1's if x(63) = 1.
-      x = (x << 1) | (y >> 63);	// Shift x || y left
-      y = y << 1;		// one bit.
-      if ((x | t) >= z)
-	{
-	  x = x - z;
-	  y = y + 1;
-	}
+  uint64 t;
+  int i;
+
+  /* Keep all 64 division steps, but run eight per loop iteration. */
+  for (i = 0; i < 8; ++i)
+    {
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
+      MODUL64_STEP ();
     }
   return x;			// Quotient is y.
 }
 
+#undef MODUL64_STEP
+#endif
+
 /* ---------------------------- montmul ----------------------------- */
 
+#ifdef __SIZEOF_INT128__
+uint64
+montmul (uint64 abar, uint64 bbar, uint64 m, uint64 mprime)
+{
+  unsigned __int128 t, tmm, sum;
+  uint64 tm, u, ov;
+
+  t = (unsigned __int128) abar * bbar;
+  tm = (uint64) t * mprime;
+  tmm = (unsigned __int128) tm * m;
+
+  /* The wide sum combines the high-word addition and its carry detection. */
+  sum = t + tmm;
+  u = (uint64) (sum >> 64);
+  ov = (uint64) (sum < t);
+
+  return u - (m & -(ov | (uint64) (u >= m)));
+}
+#else
 uint64
 montmul (uint64 abar, uint64 bbar, uint64 m, uint64 mprime)
 {
@@ -149,6 +196,7 @@
 
   return ulo;
 }
+#endif
 
 /* ---------------------------- xbinGCD ----------------------------- */
 
@@ -175,6 +223,31 @@
 void
 xbinGCD (uint64 a, uint64 b, volatile uint64 * pu, volatile uint64 * pv)
 {
+  /* For a = 2**63 and odd b, Newton iteration computes b's inverse modulo
+     2**64 in five steps. Derive both outputs from it, retaining the general
+     binary-GCD path below for every other valid input. */
+  if (a == UINT64_C (0x8000000000000000) && (b & 1))
+    {
+      uint64 x = b;
+      uint64 u, v, lo, oldlo;
+
+      x = x * (2 - b * x);
+      x = x * (2 - b * x);
+      x = x * (2 - b * x);
+      x = x * (2 - b * x);
+      x = x * (2 - b * x);
+
+      v = 0 - x;
+      mulul64 (b, v, &u, &lo);
+      oldlo = lo;
+      ++lo;
+      u += (lo < oldlo);
+
+      *pu = u;
+      *pv = v;
+      return;
+    }
+
   uint64 alpha, beta, u, v;
 
   u = 1;
```