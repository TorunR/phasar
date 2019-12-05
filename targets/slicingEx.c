#include <stdio.h>
#include <stdlib.h>

#include "slicingEx2.h"
int global = 0;

int bar(int k);
struct point *getSomePoint(struct point* p, struct point* p2);
int foo(int i,int k,int j);

struct point {
    int x;
    int y;
}; 


struct point *getSomePoint(struct point* p, struct point* p2) {
    if (rand() < (RAND_MAX/2)) {
        return p;
    } else {
        return p2;
    }
}

static int int_add(int i, int j){
  return i + j;
}

static int int_mod(int i, int j){
  return i % j;
}


int foo3(struct point* p) {
    return p->x + p->y;
}

int (*getFunction ()) (int,int){
  if (rand() < (RAND_MAX/2)) {
    return &int_add;
  } else {
    return &int_mod;
  }
}

int bar(int k) {
    int i = 0;
    struct point p = { k,2};
    struct point p2 = { 1, k + 5};
    p.x = 5;
    p.y = k;
    global = 42;
    i += foo2(7,8);
    i += foo3(&p2);
    i += foo3(&p);
    i += foo3(getSomePoint(&p,&p2));
    i += fp(2,4,getFunction());
    return i;
}

int foo(int i,int k,int j){
    i += bar(k);
    if (i == 0){
        return k;
    } else {
        return j;
    }
}

int main() {
    int A = foo(0,1,2);
    int B = foo(5,3,4);
    printf("%d\n", A);
    printf("%d\n", B);
    printf("%d\n", global);
    printf("%d\n", global2);
}