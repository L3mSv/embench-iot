
#include <stdio.h>
#include "support.h"
int main(void) {
    initialise_benchmark();
    warm_caches(1);
    int r = benchmark();
    int v = verify_benchmark(r);
    printf("result=%d verify=%d\n", r, v);
    return 0;
}
