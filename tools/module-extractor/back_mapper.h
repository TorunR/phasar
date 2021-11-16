#ifndef PHASAR_BACK_MAPPER_H
#define PHASAR_BACK_MAPPER_H

#include <set>
#include <string>

namespace clang {
class SourceLocation;
};

std::string add_block(std::string file,
                      const std::set<unsigned int> *target_lines);

#endif // PHASAR_BACK_MAPPER_H
