#include "project.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace coldx::content::detail {

namespace {

std::string Trim(std::string_view s) {
    const size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string_view::npos) return {};
    const size_t b = s.find_last_not_of(" \t\r");
    return std::string(s.substr(a, b - a + 1));
}

bool ValidWord(const std::string& w) {
    return !w.empty() && std::all_of(w.begin(), w.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

char Lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool Match(std::string_view p, std::string_view s) {
    if (p.empty()) return s.empty();
    if (p.size() >= 2 && p[0] == '*' && p[1] == '*') {
        std::string_view rest = p.substr(2);
        // "**/" also matches no folders at all.
        if (!rest.empty() && rest[0] == '/' && Match(rest.substr(1), s)) return true;
        for (size_t i = 0; i <= s.size(); ++i)
            if (Match(rest, s.substr(i))) return true;
        return false;
    }
    if (p[0] == '*') {
        for (size_t i = 0; i <= s.size(); ++i) {
            if (Match(p.substr(1), s.substr(i))) return true;
            if (i < s.size() && s[i] == '/') break;
        }
        return false;
    }
    if (s.empty()) return false;
    if (p[0] == '?') return s[0] != '/' && Match(p.substr(1), s.substr(1));
    return Lower(p[0]) == Lower(s[0]) && Match(p.substr(1), s.substr(1));
}

} // namespace

bool GlobMatch(std::string_view pattern, std::string_view path) {
    return Match(pattern, path);
}

bool ParseProject(std::string_view text, Project& project, std::vector<std::string>& errors) {
    std::istringstream lines{std::string(text)};
    std::string raw;
    int number = 0;
    const size_t before = errors.size();
    auto error = [&](const std::string& what) { errors.push_back("line " + std::to_string(number) + ": " + what); };
    while (std::getline(lines, raw)) {
        ++number;
        const std::string line = Trim(std::string_view(raw).substr(0, raw.find('#')));
        if (line.empty()) continue;
        if (line[0] == '[') {
            const size_t close = line.find(']');
            if (close == std::string::npos) {
                error("expected [processor] patterns");
                continue;
            }
            Section section;
            section.processor = Trim(std::string_view(line).substr(1, close - 1));
            section.line = number;
            std::istringstream words(line.substr(close + 1));
            for (std::string w; words >> w;) {
                std::replace(w.begin(), w.end(), '\\', '/');
                if (w.find("..") != std::string::npos || w[0] == '/' || (w.size() > 1 && w[1] == ':'))
                    error("pattern \"" + w + "\" leaves the project folder");
                section.patterns.push_back(w);
            }
            if (!ValidWord(section.processor)) error("\"" + section.processor + "\" is not a processor name");
            if (section.patterns.empty()) error("[" + section.processor + "] lists no files");
            project.sections.push_back(std::move(section));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            error("expected key = value, or [processor] patterns");
            continue;
        }
        const std::string key = Trim(std::string_view(line).substr(0, eq));
        const std::string value = Trim(std::string_view(line).substr(eq + 1));
        if (!ValidWord(key)) {
            error("\"" + key + "\" is not a setting name");
        } else if (project.sections.empty()) {
            if (key == "seed") {
                project.seed = value;
                project.seedSet = true;
            } else {
                error("unknown project setting \"" + key + "\" (settings for a processor go under its section)");
            }
        } else if (!project.sections.back().settings.emplace(key, value).second) {
            error("\"" + key + "\" set twice");
        }
    }
    return errors.size() == before;
}

std::vector<std::string> ListFiles(const std::string& directory) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        // UTF-8 whatever the system code page, so non-ASCII names do not throw.
        const std::u8string name = fs::relative(it->path(), directory, ec).generic_u8string();
        files.emplace_back(name.begin(), name.end());
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace coldx::content::detail
