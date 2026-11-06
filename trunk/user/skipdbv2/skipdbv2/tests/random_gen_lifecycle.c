#include "RandomGen.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    RandomGen *first;
    RandomGen *second;
    int i;

    if (RandomGen_randomDouble(NULL) != 0.0 ||
        RandomGen_randomInt(NULL) != 0 ||
        RandomGen_gaussian(NULL, 12.0, 3.0) != 0.0) {
        fprintf(stderr, "NULL RandomGen calls did not return their safe defaults\n");
        return EXIT_FAILURE;
    }
    RandomGen_setSeed(NULL, 5489UL);
    RandomGen_chooseRandomSeed(NULL);

    first = RandomGen_new();
    second = RandomGen_new();
    if (NULL == first || NULL == second) {
        fprintf(stderr, "RandomGen construction failed\n");
        RandomGen_free(first);
        RandomGen_free(second);
        return EXIT_FAILURE;
    }

    RandomGen_setSeed(first, 5489UL);
    RandomGen_setSeed(second, 5489UL);
    for (i = 0; i < 128; ++i) {
        int firstValue = RandomGen_randomInt(first);
        int secondValue = RandomGen_randomInt(second);
        if (firstValue < 0 || secondValue < 0) {
            fprintf(stderr, "RandomGen_randomInt produced a negative value\n");
            RandomGen_free(first);
            RandomGen_free(second);
            return EXIT_FAILURE;
        }
        if (firstValue != secondValue) {
            fprintf(stderr, "equal seeds produced different random sequences\n");
            RandomGen_free(first);
            RandomGen_free(second);
            return EXIT_FAILURE;
        }
    }

    RandomGen_setSeed(first, 123456UL);
    for (i = 0; i < 2048; ++i) {
        double value = RandomGen_randomDouble(first);
        double gaussian = RandomGen_gaussian(second, 5.0, 2.0);
        if (!(value >= 0.0 && value < 1.0) || !isfinite(gaussian)) {
            fprintf(stderr, "random output left its documented numeric range\n");
            RandomGen_free(first);
            RandomGen_free(second);
            return EXIT_FAILURE;
        }
    }

    RandomGen_free(first);
    RandomGen_free(second);
    puts("RandomGen lifecycle test passed");
    return EXIT_SUCCESS;
}
