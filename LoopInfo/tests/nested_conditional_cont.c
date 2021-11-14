#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "macros.h"
#include <stdbool.h>
#include<string.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: ./nested_conditional_cont <count1> <count2> <bool: check1> <bool: check2> \n");
        return 1;
    }

    int count = atoi(argv[1]);
    int count2 = atoi(argv[2]);
    bool check = strcmp(argv[3],"0");
    bool check2 = strcmp(argv[4],"0");

    uint32_t x = 0;

    loop(size_t, i, 0, count,
        if(check) {
          loop(size_t, j, 0, count2, 
            if(check2 && i == j) {
	      x*=2;
            }
            x += i * j;
          )					
        }
    )
    printf("%d\n",x);
}
