#ifndef MACROS_H
#define MACROS_H

#ifndef LOOP_TYPE
    # define LOOP_TYPE 1
#endif

#if LOOP_TYPE == 1
    #define loop(type,var,start,end,body) for(type var = start; var < end; var ++){\
                                              body\
                                          }
#elif LOOP_TYPE == 2
    #define loop(type,var,start,end,body) type var = start;\
                                          while(var < end){\
                                              body\
                                              var ++;\
                                          }
#elif LOOP_TYPE == 3
    #define loop(type,var,start,end,body) type var = start;\
                                          do{\
                                              body\
                                              var ++;\
                                          }while(var < end);
#else
    #error Invalid loop type given
#endif

#endif /* MARCOS_H */
