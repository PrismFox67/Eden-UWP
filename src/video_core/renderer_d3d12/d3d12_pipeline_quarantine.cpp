// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_d3d12/d3d12_pipeline_quarantine.h"

#include <cstdio>
#include <cwchar>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <windows.h>

#include "common/logging.h"

namespace D3D12::PipelineQuarantine {
namespace {

thread_local u64 t_creating_key = 0;
wchar_t s_path[MAX_PATH]{}; // fixed buffer: OnCrash must not allocate
std::shared_mutex s_mutex;
std::unordered_set<u64> s_blocked;

} // Anonymous namespace

void Open(const std::filesystem::path& path) {
    std::unordered_set<u64> blocked;
    {
        std::ifstream file(path);
        std::string line;
        while (std::getline(file, line)) {
            unsigned long long key = 0;
            if (std::sscanf(line.c_str(), "%llx", &key) == 1 && key != 0) {
                blocked.insert(key);
            }
        }
    }
    if (!blocked.empty()) {
        LOG_WARNING(Render, "D3D12: skipping {} pipeline(s) that crashed the driver before ({})",
                    blocked.size(), path.string());
    }
    std::unique_lock lock{s_mutex};
    s_blocked = std::move(blocked);
    const std::wstring wide = path.wstring();
    if (wide.size() < MAX_PATH) {
        wcscpy_s(s_path, wide.c_str());
    } else {
        s_path[0] = L'\0';
    }
}

bool IsBlocked(u64 key) {
    std::shared_lock lock{s_mutex};
    return s_blocked.contains(key);
}

Scope::Scope(u64 key) : previous{t_creating_key} {
    t_creating_key = key;
}

Scope::~Scope() {
    t_creating_key = previous;
}

void OnCrash() noexcept {
    const u64 key = t_creating_key;
    if (key == 0 || s_path[0] == L'\0') {
        return;
    }
    char line[24];
    const int length =
        std::snprintf(line, sizeof(line), "%016llx\n", static_cast<unsigned long long>(key));
    const HANDLE file = CreateFile2(s_path, FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_ALWAYS, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
}

} // namespace D3D12::PipelineQuarantine
