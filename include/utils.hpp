#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

std::string trim(const std::string& input);
std::string to_lower(std::string input);
std::vector<std::string> split_lines(const std::string& input);
std::vector<std::string> split_any(const std::string& input, const std::string& delimiters);
std::string url_decode(const std::string& input);
std::string replace_all(std::string input, const std::string& from, const std::string& to);

#endif
