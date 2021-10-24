#include <stdio.h>

void foo()
{
  printf("hi\n");
}

void bar()
{
  foo();
}

void baz()
{
  bar();
}

int main(int argc, char **argv)
{
  baz();
  baz();
  baz();
  return 0;
}
