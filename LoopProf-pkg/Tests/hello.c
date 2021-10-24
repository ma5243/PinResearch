#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int n1 = 400;
int n2 = 200;
int n3 = 300;

void foo(int i);

void bar(int i)
{
  int x;
  //static int count = 1;

  for(x = 0; x < i; x++) {
    //printf("barloop %d\n", count++);
    foo(x);
  }

}

void foo(int i)
{
  int x;
  x = 5;
  for(x = 0; x < i; x++) {
    bar(x);
  }
  x = 6;
}

void baz(int i)
{
  if( i < 0 ) {return;}
  baz(i-1);
}

void b()
{
  int i = 0;
  float x = 3.3f;
  x *= x;
  x *= x;
  for( i = 0; i < n3; i++ ) {
    x *= x;
  }
}

void a()
{
  int i;
  float x = 3.3f;
  x *= x;
  x *= x;
  for( i = 0; i < n3; i++ ) {
    b();
  }
}

void c()
{
  float x = 0;
  x *= x;
}

int main(int argc, char *argv[], char *envp[])
{
  int i = 0;
  int j = 0;
  int k = 0;
  float x = 0;
  //int n1 = atoi(argv[1]);
  //int n2 = atoi(argv[2]);
  int do1 = 0, do2= 0;

  for(i = 0; i < 3; i++) {
    j = 0;
    while( j++ < 3 ) {
      c();
    }
  }

  for(i = 0; i < 3; i++) {
    foo(3);
  }

  for(i = 0; i < 3; i++) {
    baz(3);
  }

  for(i = 0; i < 3; i++) {
    a();
  }

  printf("hello\n");

  i = n1;
  while( --i > 0 ) {
    x *= 3;
    j = n2;
    while( --j > 0 ) {
      x *= 3;
    }
  }

  i = n1;
  do {
    do1++;
    x *= 3;
    j = n2;
    do {
      do2++;
      x *= 3;
    } while( j-- > 0 );
  } while( i-- > 0 );
  printf("do1 = %d, do2 = %d\n", do1, do2);
  for( i = 0; i < n1; i++ ) {
    for( j = 0; j < n2; j++ ) {
      for( k = 0; k < n3; k++ ) {
	x++;
      }
    }
  }

  for( i = 0; i < n1; i++ ) {
    x++;
    for( j = 0; j < n2; j++ ) {
      x++;
    }
    x++;
    for( k = 0; k < n3; k++ ) {
      x++;
    }
    x++;
  }

  for( i = 0; i < n1; i++ ) {
    for( j = 0; j < n2; j++ ) {
      printf("%c", '\0');
      if( j == 2 ) {
	break;
      }
    }
    printf("%d%d%d\n", 1,2,3);
    if( i == 2 ) {
      break;
    }
  }

  //foo(5);
  return EXIT_SUCCESS;
}
