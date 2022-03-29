#ifndef PHASAR_INCLUDE_HANDLING_H
#define PHASAR_INCLUDE_HANDLING_H

#include <boost/filesystem.hpp>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * Returns all includes found in a file, which are not blacklisted
 * Based on a regex search
 * @param filename The file to search for includes
 * @param blacklist set containing includes not to extract
 * @return Found includes
 */
std::vector<std::string>
get_includes_to_extract(const std::string &filename,
                        const std::unordered_set<std::string> &blacklist);

/**
 *
 * @return Returns the path to a file suitable for temporary files
 */
boost::filesystem::path get_tmp_filename();

/**
 *
 * @return  rue if iwyu_tool.py and fix_includes.py can be successfully called
 */
bool is_iwyu_available();

void cleanup_includes(const std::string &original_file,
                      const std::string &header,
                      const std::string &compile_commands_path);

#endif // PHASAR_INCLUDE_HANDLING_H
