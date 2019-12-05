
#include <setjmp.h>

#include "slicingEx2.h"

int foo_impl(int i, int j) {
  return i * j;
}

int foo2(int i, int j) {
    return fp(i,j,&foo_impl);
}

int fp(int i, int j, int (*f)(int,int)) {
  return f(i,j);

}

int intra() {
  jmp_buf env;
  int i = setjmp(env);
  int k = 0;
  if (i == 0) {
    k = 2;
    longjmp(env,2);
  } else {
    k = 1;
  }
  return k;
}