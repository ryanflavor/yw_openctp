#pragma once
#include <fstream>
#include <map>
#include <string>

inline std::map<std::string, std::string> load_env(const std::string &path) {
    std::map<std::string, std::string> env;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        env[key] = val;
    }
    return env;
}
