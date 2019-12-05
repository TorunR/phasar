#include "stdlib.h"
#include <stdio.h>

/*
WARNING: Comments are just examples of dependencies and are not exhaustive

Suggestion: Compile without compiler optimizations (-O0)
The expected output of your preparation should be a machine-readable list of dependencies, as produced or used by your tool.
*/


int main(int argc, char **argv) {
  // RAW dependence in declaration
  int n = 20;
  double A[n];

  A[0] = n;
  for (int i = 1; i < n; i++) {
    if(i % 2 == 0) {
      // Loop-carried data dependence on array elements
      A[i] = A[i-1] / n;
    } else {
      A[i] = ((double) i) / n;
    }
  }

  double sum = 0;
  // Loop-carried data dependence
  for (int i = 0; i < n; i++) {
    // WAW dependence with previous declaration of sum
    sum = sum + A[i];
  }

  // RAW dependence with previous statements
  // Control dependence on next two statements
  if(sum > 10) {
    printf("Sum is greater than 10");
  } else {
    printf("Sum is smaller or equal 10");
  }

  return 0;
}
