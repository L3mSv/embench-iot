/* Minimal standalone replacement for Embench's support.h.
 *
 * Only what edn.c actually needs: the GLOBAL_SCALE_FACTOR macro and the
 * prototypes for the four functions every Embench/BEEBS benchmark exposes.
 * Real Embench computes GLOBAL_SCALE_FACTOR automatically so every
 * benchmark runs for a similar amount of time; here it's just a constant
 * you can tune by hand to make the run longer/shorter for measurement.
 */

#ifndef SUPPORT_H
#define SUPPORT_H

#ifndef GLOBAL_SCALE_FACTOR
#define GLOBAL_SCALE_FACTOR 1
#endif

void initialise_benchmark (void);
void warm_caches (int heat);
int  benchmark (void);
int  verify_benchmark (int result);

#endif /* SUPPORT_H */
