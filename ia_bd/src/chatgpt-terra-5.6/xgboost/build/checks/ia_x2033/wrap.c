
extern int __real_rand_beebs(void);
int __wrap_rand_beebs(void) { return __real_rand_beebs() ^ VARY_XOR; }
