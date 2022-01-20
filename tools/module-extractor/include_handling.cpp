#include "include_handling.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <string>

std::vector<std::string>
get_includes_to_extract(const std::string &filename, const std::string &outname,
                        const std::unordered_set<std::string> &blacklist) {
  const std::string iwyu_output_file = get_tmp_filename().native();
  const std::string iwyu_toolname = "iwyu";
  // inspired by
  // https://stackoverflow.com/questions/26492513/write-c-regular-expression-to-match-a-include-preprocessing-directive
  // This does not match weird includes containing macros and may get some
  // things in comments
  const std::string include_regex_str =
      R"reg(\s*#\s*include\s*[<"]([^>"]+)[>"])reg";
  std::regex include_regex(include_regex_str);
  std::string original_file_contents;
  {
    std::ifstream original_file(filename);
    std::stringstream buffer;
    buffer << original_file.rdbuf();
    original_file_contents = buffer.str();
  }
  auto trim = [](std::string str) {
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(),
                           [](unsigned char c) { return !std::isspace(c); }));
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](unsigned char c) { return !std::isspace(c); })
                  .base(),
              str.end());
    return str;
  };
  std::vector<std::string> includes;
  auto begin =
      std::sregex_iterator(original_file_contents.begin(),
                           original_file_contents.end(), include_regex);
  auto end = std::sregex_iterator();
  for (auto It = begin; It != end; ++It) {
    const auto &match = *It;
    if (blacklist.find(trim(match[1].str())) == blacklist.end()) {
      includes.push_back(trim(match.str()));
    }
    //std::cerr << match.str() << "\n";
    //std::cerr << match[1].str() << "\n";
  }

  return includes;
}

boost::filesystem::path get_tmp_filename() {
  const auto path = boost::filesystem::temp_directory_path();
  return path / boost::filesystem::unique_path();
}
