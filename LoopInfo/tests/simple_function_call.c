#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"
#include <stdbool.h>
#include<string.h>

int foo(int i) {
  return (i+1)*2;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./simple_function_call <count>\n");
        return 1;
    }

    int count = atoi(argv[1]);

    uint32_t x = 0;

    loop(size_t, i, 0, count,
    	x+=foo(i);    
    )

    printf("%d\n",x);
}

