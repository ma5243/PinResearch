#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


int x = 0;
int NUM_THREADS = 8;
float f = 3.14f;

int fact(int n)
{
  int i = 0, j = 0;
  for( i = 0; i < 3; i++ ) {
    for( j = 0; j < 3; j++ ) {
      f *= f;
    }
  }
  if( n <= 0 ) {
    return 1;
  } else {
    return n * fact(n - 1);
  }
}

void *
thread(void *v)
{
  int incs = 0;
  int id = (int)v;
  printf("%d started\n", id);
  while( incs < 4 ) {
    if( x % NUM_THREADS == id ) {
      /*printf("ID: %d\n", id);*/
      x++;
      fact(5);
      incs++;
    }
  }
  
  printf(__FILE__ ": thread %d finished\n", id);
  return 0;
}

int
main(int argc, char **argv)
{
  pthread_t *threads;
  int i;
  
  if( argc > 1 ) {
    NUM_THREADS = atoi(argv[1]);
  }
  threads = malloc(sizeof(pthread_t) * NUM_THREADS);
  for( i = 0; i < NUM_THREADS; i++ ) {
    printf("creating %d\n", i);
    if( pthread_create(&threads[i], NULL, thread, (void *)i) != 0 ) {
      perror("pthread_create");
    }
  }
  
  for( i = 0; i < NUM_THREADS; i++ ) {
    pthread_join(threads[i], NULL);
  }
  return 0;
}
