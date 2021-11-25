#ifndef PHASAR_BACK_MAPPER_H
#define PHASAR_BACK_MAPPER_H

#include "printer.h"
#include <set>
#include <string>
#include <vector>

namespace clang {
class SourceLocation;
};

std::vector<printer::FileSlice>
add_block(std::string file, const std::set<unsigned int> *target_lines);

#endif // PHASAR_BACK_MAPPER_H
