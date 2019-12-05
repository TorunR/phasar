#include <stdbool.h>
#include <stdlib.h>

void oneC(){
    bool condA = rand() < (RAND_MAX/2);
    bool condC,condD,condF;
    if (condA) {
        labelB:
            condC = rand() < (RAND_MAX/2);
            if (condC) {
              goto labelB;
            }
        labelD:
            condD = rand() < (RAND_MAX/2);
            if (condD) {
               goto labelJ;
            }
        labelJ:
            goto labelD;
    } else {
        condF = rand() < (RAND_MAX/2);
        if (condF) {

        }
    }
}


int main(void) {
  oneC();
}