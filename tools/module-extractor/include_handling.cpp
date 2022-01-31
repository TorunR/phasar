#include "include_handling.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <string>

std::vector<std::string>
get_includes_to_extract(const std::string &filename,
                        const std::unordered_set<std::string> &blacklist) {

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
    // std::cerr << match.str() << "\n";
    //  std::cerr << match[1].str() << "\n";
  }

  return includes;
}

boost::filesystem::path get_tmp_filename() {
  const auto path = boost::filesystem::temp_directory_path();
  return path / boost::filesystem::unique_path("slice-%%%%-%%%%-%%%%-%%%%");
}
bool is_iwyu_available() {
  static bool is_available = []() {
    const bool available =
        (WEXITSTATUS(std::system("iwyu_tool.py --help")) == 0) &&
        (WEXITSTATUS(std::system("fix_includes.py --help")) == 0);
    if (!available) {
      std::cout << "iwyu or fix_include tool not found. Skipping header cleanup"
                << std::endl;
    }
    return available;
  };
  return is_available;
}
void cleanup_includes(const std::string &original_file,
                      const std::string &header,
                      const std::string &compile_commands_path) {
  if (!is_iwyu_available()) {
    return;
  }
  if (compile_commands_path == "none") {
    return;
  }
  /**
   * iwyu expects a compile_commands.json and paths to match, so we use the
   * following approach:
   *
   * 1: Rename the original source file to source file.slicerback
   *
   * 2: Copy the extracted header to the original source file
   *
   * 3: Run iwyu
   *
   * 4: Run fix_include (for some reason iwyu does not include absolute paths
   * grrr, so this is actually a multi step process. 4.a: copy the file from 2
   * to the tmp dir. (we cant move here as it could be between filessystems) 4.b
   * change into the tmp dir 4.c run fix_include 4.d copy the modified source
   * file to the extracted header location)
   *
   *
   * 5: Cleanup temporary files
   *
   * 6: Undo 1
   */

  boost::filesystem::path original_path(original_file);
  boost::filesystem::path original_backup(original_file + ".slicerback");
  boost::filesystem::path header_path(header);
  const std::string iwyu_output_file = get_tmp_filename().native();

  // 1: (this should not fail)
  boost::filesystem::rename(original_path, original_backup);
  // 2: (this should not fail)
  boost::filesystem::copy_file(
      header_path, original_path,
      boost::filesystem::copy_options::overwrite_existing);
  // 3: (this can fail, for example if we have weird headers)
  const std::string iwyu_command = "iwyu_tool.py -p " + compile_commands_path +
                                   " " + original_path.native() +
                                   " -- -Xiwyu --no_fwd_decls 1> " +
                                   iwyu_output_file;
  // std::cerr << iwyu_command << std::endl;
  const auto iwyu_return = WEXITSTATUS(std::system(iwyu_command.c_str()));
  // iwyu has strange exit codes
  // https://github.com/include-what-you-use/include-what-you-use/issues/440
  // https://github.com/include-what-you-use/include-what-you-use/blob/master/iwyu_globals.h#L33
  // https://github.com/include-what-you-use/include-what-you-use/blob/master/iwyu.cc#L3648
  if (iwyu_return != 0) {
    std::cerr << "An error occurred running the following command: "
              << iwyu_command << std::endl;
    // Just cleanup but leave the output behind
  } else {
    // 4 a
    const auto tmpdir = boost::filesystem::temp_directory_path();
    const auto name = original_path.filename();
    const auto tmpname = tmpdir / name;
    boost::filesystem::copy_file(
        original_path, tmpname,
        boost::filesystem::copy_options::overwrite_existing);
    // const auto previous_cp = boost::filesystem::current_path();
    //  4 b
    // boost::filesystem::current_path(tmpdir);
    // 4 c (this should not fail, but we have no way to check it anyway)
    const std::string fix_command = "fix_includes.py -p " + tmpdir.native() +
                                    " --nosafe_headers " + tmpname.native() +
                                    " < " + iwyu_output_file;

    // std::cerr << fix_command << std::endl;
    std::system(fix_command.c_str());
    // boost::filesystem::current_path(previous_cp);
    // 5 (this should not fail)
    boost::filesystem::copy_file(
        tmpname, header_path,
        boost::filesystem::copy_options::overwrite_existing);
    boost::filesystem::remove(tmpname);
    // 5 (this should not fail)
    boost::filesystem::remove(iwyu_output_file);
  }

  // 6: (this should not fail)
  boost::filesystem::rename(original_backup, original_path);
}
