#include <stdio.h>
#include <stdint.h>

#ifndef LOOP_TYPE
    # define LOOP_TYPE 1
#endif

#if LOOP_TYPE == 1
    #define loop(init, test, inc, body) for(init; test; inc){\
                                              body\
                                          }
#elif LOOP_TYPE == 2
    #define loop(init, test, inc, body) init;\
                                          while(test){\
                                              body\
                                              inc;\
                                          }
#elif LOOP_TYPE == 3
    #define loop(init, test, inc, body) init;\
                                          do{\
                                              body\
                                              inc;\
                                          }while(test);
#else
    #error Invalid loop type given
#endif

int main(){
 int x = 0;
 loop(size_t i = 0, i < 10, i++,
         x++;
 )
 printf("%d\n",x);
}
