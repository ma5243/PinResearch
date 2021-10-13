#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./simple2 <count>\n");
        return 1;
    }

    int count = atoi(argv[1]);

    uint32_t x = 0;
    
    loop(size_t, i, 0, count, 
        x += i * (i % 3);
    )
    
    printf("%d\n",x);
}
