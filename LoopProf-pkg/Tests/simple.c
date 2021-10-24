#include <stdio.h>

void bar();
void foo()
{
  int n1 = 3;
  int j, k;
  float x = 0.0f;

  for( j = 0; j < n1; j++ ) {
    for( k = 0; k < n1; k++ ) {
      x++;
    }
  }
  
  bar();
}

void bar()
{
  float x = 0.3f;
  x *= 3;
}

int main()
{
  int n1 = 3;
  int i, j, k;
  float x = 0.0f;

  for( i = 0; i < n1; i++ ) {
    for( j = 0; j < n1; j++ ) {
      for( k = 0; k < n1; k++ ) {
	x++;
      }
    }
  }

  for( i = 0; i < n1; i++ ) {
    foo();
  }

  for( i = 0; i < n1; i++ ) {
    printf("%c", '\0');
  }

  return 0;
}
