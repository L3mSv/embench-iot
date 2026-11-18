# Comparação aha-mont64: nemotron-lightning-3.5 vs original

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
+++ nemotron-lightning-3.5/mont64.c
@@ -56,7 +56,7 @@
 void
 mulul64 (uint64 u, uint64 v, uint64 * whi, uint64 * wlo)
 {
-  uint64 u0, u1, v0, v1, k, t;
+  uint64 u0, u1, v0, v1, t;
   uint64 w0, w1, w2;
 
   u1 = u >> 32;
@@ -66,17 +66,14 @@
 
   t = u0 * v0;
   w0 = t & 0xFFFFFFFF;
-  k = t >> 32;
-
-  t = u1 * v0 + k;
+
+  t = u1 * v0 + (t >> 32);
   w1 = t & 0xFFFFFFFF;
   w2 = t >> 32;
 
   t = u0 * v1 + w1;
-  k = t >> 32;
-
   *wlo = (t << 32) + w0;
-  *whi = u1 * v1 + w2 + k;
+  *whi = u1 * v1 + w2 + (t >> 32);
 
   return;
 }
```