// Reference: http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html
//            (Brennan's Guide to Inline Assembly)
#include <stdio.h>

int
main(int argc, char **argv)
{
  int x = 1;
  printf("Hello x = %d\n", x);
  asm("inc %0\n\t"
      : "=r"(x)
      : "0"(x));
  printf("Hello x = %d after increment\n", x);

  if(x == 2){
    printf("OK\n");
  }
  else{
    printf("ERROR\n");
  }
}

