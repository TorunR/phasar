


#include <stdlib.h>

int throws_something(){
    int r = rand();
    if (r < (RAND_MAX/2)) {
        throw r;
    } else {
        throw r + 5;
    }
}

int main(int, char*[])  {
    try {
    throws_something();
  } catch (int x) {

    }
}



