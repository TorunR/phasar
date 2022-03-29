#ifndef PHASAR_BACK_MAPPER_H
#define PHASAR_BACK_MAPPER_H

#include "printer.h"
#include <set>
#include <string>
#include <vector>

/**
 * Get the slices for the file 'file' and the given target_lines
 * @param file
 * @param target_lines
 * @return  The first vector are the file slices, the second the slices for the
 * creation of a header
 */
std::pair<std::vector<printer::FileSlice>, std::vector<printer::FileSlice>>
add_block(std::string file, const std::set<unsigned int> *target_lines);

#endif // PHASAR_BACK_MAPPER_H
