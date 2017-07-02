#include <stdio.h>

void func(const int *p) {
  printf("Argument is %d\n", *p);
}

int main() {
  
  int a = 34;
  int *ptr = &a; // NOTE THAT PTR DOES NOT NEED TO BE CONST!

  func(ptr);
  return 0;
}
