#include <iostream>

#include "slicer.h"

int main(int argc, const char **argv) {
  if (argc != 3) {
    std::cout << "Usage:" << std::endl;
    std::cout << "result_comparator path_to_ground_truth path_to_extracted_version"
              << std::endl;
    return -1;
  } else {
    string original = argv[1];
    string module = argv[2];
    compare_slice(original, module);
    return 0;
  }
}