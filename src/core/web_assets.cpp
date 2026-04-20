#include "web_assets.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::string& load_asset(const std::string& filename, const std::string& fallback) {
    static std::vector<std::pair<std::string, std::string>> cache;

    for (const auto& entry : cache) {
        if (entry.first == filename) {
            return entry.second;
        }
    }

    const std::vector<std::string> candidates = {
        "web/" + filename,
        "../web/" + filename,
    };

    for (const auto& path : candidates) {
        std::ifstream stream(path);
        if (!stream) {
            continue;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        cache.push_back({filename, buffer.str()});
        return cache.back().second;
    }

    cache.push_back({filename, fallback});
    return cache.back().second;
}
}  // namespace

const std::string& index_html() {
    return load_asset(
        "index.html",
        "<html><body><h1>UI assets not found.</h1><p>Expected web/index.html next to the project.</p></body></html>"
    );
}

const std::string& stylesheet() {
    return load_asset("styles.css", "body{font-family:sans-serif;background:#111;color:#fff;}");
}

const std::string& app_script() {
    return load_asset("app.js", "console.error('app.js not found');");
}
