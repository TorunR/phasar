#ifndef PHASAR_BACK_MAPPER_H
#define PHASAR_BACK_MAPPER_H

#include <memory>
#include <set>
#include <string>

std::shared_ptr<std::set<unsigned int>>
add_block(std::string file, std::set<unsigned int> *target_lines);

#endif // PHASAR_BACK_MAPPER_H
