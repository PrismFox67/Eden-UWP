// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "jadyx/app/session_marker.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Jadyx::SessionMarker {
namespace {

std::string ReadFrom(const std::filesystem::path& path, std::uintmax_t offset) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::uintmax_t>(file.tellg());
    // The diag rotates past 8 MiB; then the whole (new) file is this session's.
    file.seekg(offset <= size ? static_cast<std::streamoff>(offset) : 0);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace

void MarkStarted(const Paths& paths, const std::string& game) {
    std::error_code ec;
    const auto diag_size = std::filesystem::file_size(paths.diag, ec);
    std::ofstream(paths.marker, std::ios::trunc)
        << "game " << game << "\ndiag " << (ec ? 0 : diag_size) << "\n";
}

void MarkStopped(const Paths& paths) {
    std::error_code ec;
    std::filesystem::remove(paths.marker, ec);
}

void MarkSuspended(const Paths& paths, bool suspended) {
    std::error_code ec;
    if (!std::filesystem::exists(paths.marker, ec)) return;
    std::ofstream(paths.marker, std::ios::app) << (suspended ? "suspended\n" : "resumed\n");
}

std::optional<LastSession> CheckLastSession(const Paths& paths) {
    std::error_code ec;
    if (!std::filesystem::exists(paths.marker, ec)) return std::nullopt;
    LastSession session;
    std::uintmax_t diag_offset = 0;
    bool suspended = false;
    {
        std::ifstream file(paths.marker);
        for (std::string line; std::getline(file, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("game ")) session.game = line.substr(5);
            else if (line.starts_with("diag ")) diag_offset = std::strtoull(line.c_str() + 5, nullptr, 10);
            else if (line == "suspended") suspended = true;
            else if (line == "resumed") suspended = false;
        }
    }
    std::filesystem::remove(paths.marker, ec);
    // The console may end a suspended app without notice (another game starts): not a problem.
    if (suspended) return std::nullopt;

    const std::string diag = ReadFrom(paths.diag, diag_offset);
    session.crashed = diag.find("CRASH:") != std::string::npos;
    // "... | app memory 918 MiB of 30737 MiB limit ..."
    constexpr std::string_view key = "app memory ";
    for (size_t pos = diag.find(key); pos != std::string::npos; pos = diag.find(key, pos + 1)) {
        const char* p = diag.c_str() + pos + key.size();
        char* end = nullptr;
        const double used = std::strtod(p, &end);
        const size_t of = diag.find(" of ", end - diag.c_str());
        if (of == std::string::npos || of > pos + 64) continue;
        const double limit = std::strtod(diag.c_str() + of + 4, nullptr);
        if (limit > 0) session.memory_percent = static_cast<int>(used * 100.0 / limit + 0.5);
    }
    return session;
}

} // namespace Jadyx::SessionMarker
