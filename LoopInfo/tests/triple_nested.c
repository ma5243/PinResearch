#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./triple_nested <count1> <count2> <count3> \n");
        return 1;
    }

    int count = atoi(argv[1]);
    int count2 = atoi(argv[2]);
    int count3 = atoi(argv[3]);

    uint32_t x = 0;

    loop(size_t, i, 0, count,
        loop(size_t,j,0,count2,
            loop(size_t,k,0,count3,
              x += i * j * k;
            )
        )
    )

    printf("%d\n",x);
}

