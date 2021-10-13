#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"
#include <stdbool.h>
#include<string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./conditional_oscillate <count> <bool: check> \n");
        return 1;
    }

    int count = atoi(argv[1]);
    bool check = strcmp(argv[2],"0");

    uint32_t x = 0;

    loop(size_t, i, 0, count,
        if(check) {
          x += i;
        } else {
          x -= i;
        }
    )

    printf("%d\n",x);
}

