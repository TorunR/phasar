#ifndef PHASAR_INCLUDE_HANDLING_H
#define PHASAR_INCLUDE_HANDLING_H

#include <boost/filesystem.hpp>
#include <string>
#include <unordered_set>
#include <vector>

std::vector<std::string>
get_includes_to_extract(const std::string &filename,
                        const std::unordered_set<std::string> &blacklist);

boost::filesystem::path get_tmp_filename();

bool is_iwyu_available();

void cleanup_includes(const std::string &original_file,
                      const std::string &header,
                      const std::string &compile_commands_path);

#endif // PHASAR_INCLUDE_HANDLING_H
