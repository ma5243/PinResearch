#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./simple_nested <count1> <count2> \n");
        return 1;
    }

    int count = atoi(argv[1]);
    int count2 = atoi(argv[2]);

    uint32_t x = 0;

    loop(size_t, i, 0, count,
        if (i == 3) {
            count2 = count2 * 2;
        }
        loop(size_t,j,0,count2,
             x += i * j;
        )
    )

    printf("%d\n",x);
}     
