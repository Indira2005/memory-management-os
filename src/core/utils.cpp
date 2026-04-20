#include "utils.hpp"

#include <algorithm>
#include <cctype>

std::string trim(const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

std::string to_lower(std::string input) {
    std::transform(
        input.begin(),
        input.end(),
        input.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    return input;
}

std::vector<std::string> split_lines(const std::string& input) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : input) {
        if (ch == '\n') {
            lines.push_back(trim(current));
            current.clear();
        } else if (ch != '\r') {
            current.push_back(ch);
        }
    }
    if (!current.empty() || !input.empty()) {
        lines.push_back(trim(current));
    }
    return lines;
}

std::vector<std::string> split_any(const std::string& input, const std::string& delimiters) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : input) {
        if (delimiters.find(ch) != std::string::npos) {
            if (!current.empty()) {
                parts.push_back(trim(current));
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(trim(current));
    }
    return parts;
}

std::string url_decode(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '+') {
            output.push_back(' ');
        } else if (input[i] == '%' && i + 2 < input.size()) {
            const std::string hex = input.substr(i + 1, 2);
            const char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
            output.push_back(decoded);
            i += 2;
        } else {
            output.push_back(input[i]);
        }
    }

    return output;
}

std::string replace_all(std::string input, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return input;
    }

    std::size_t start = 0;
    while ((start = input.find(from, start)) != std::string::npos) {
        input.replace(start, from.length(), to);
        start += to.length();
    }
    return input;
}
