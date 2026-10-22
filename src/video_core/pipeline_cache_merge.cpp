// SPDX-FileCopyrightText: Copyright 2026 NSPX
// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_core/pipeline_cache_merge.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "shader_recompiler/stage.h"
#include "video_core/shader_environment.h"

namespace VideoCommon {
namespace {

constexpr std::array<char, 8> MAGIC{'y', 'u', 'z', 'u', 'c', 'a', 'c', 'h'};
constexpr u32 MAX_ENVS = 6; // Maxwell::MaxShaderProgram

/// The raw records of a cache file; `valid` is false when the header does not match.
std::vector<std::string> ReadRecords(const std::filesystem::path& path, u32 cache_version,
                                     std::size_t graphics_key_size, std::size_t compute_key_size,
                                     bool& valid) {
    std::vector<std::string> records;
    valid = false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return records;
    }
    const auto end = file.tellg();
    file.seekg(0, std::ios::beg);
    std::array<char, 8> magic{};
    u32 version{};
    file.read(magic.data(), magic.size()).read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!file || magic != MAGIC || version != cache_version) {
        return records;
    }
    valid = true;
    try {
        file.exceptions(std::ifstream::failbit);
        std::vector<char> key(std::max(graphics_key_size, compute_key_size));
        while (file.tellg() != end) {
            const auto start = file.tellg();
            u32 num_envs{};
            file.read(reinterpret_cast<char*>(&num_envs), sizeof(num_envs));
            if (num_envs == 0 || num_envs > MAX_ENVS) {
                break;
            }
            std::vector<FileEnvironment> envs(num_envs);
            for (FileEnvironment& env : envs) {
                env.Deserialize(file);
            }
            const bool compute = envs.front().ShaderStage() == Shader::Stage::Compute;
            file.read(key.data(),
                      static_cast<std::streamsize>(compute ? compute_key_size : graphics_key_size));
            const auto stop = file.tellg();
            std::string raw(static_cast<std::size_t>(stop - start), '\0');
            file.seekg(start);
            file.read(raw.data(), static_cast<std::streamsize>(raw.size()));
            records.push_back(std::move(raw));
        }
    } catch (const std::exception&) {
        // A truncated or corrupt tail is dropped; the complete records before it are kept.
    }
    return records;
}

} // Anonymous namespace

long MergePipelineCacheFiles(const std::filesystem::path& local,
                             const std::filesystem::path& incoming, u32 cache_version,
                             std::size_t graphics_key_size, std::size_t compute_key_size) {
    bool incoming_valid = false;
    const auto remote =
        ReadRecords(incoming, cache_version, graphics_key_size, compute_key_size, incoming_valid);
    if (!incoming_valid) {
        return -1;
    }
    bool local_valid = false;
    const auto existing =
        ReadRecords(local, cache_version, graphics_key_size, compute_key_size, local_valid);
    std::unordered_set<std::string> known;
    std::ofstream out;
    if (local_valid) {
        known.insert(existing.begin(), existing.end());
        // Rewrite rather than append: a truncated tail in the local file must not precede new
        // records.
        out.open(local, std::ios::binary | std::ios::trunc);
        out.write(MAGIC.data(), MAGIC.size())
            .write(reinterpret_cast<const char*>(&cache_version), sizeof(cache_version));
        for (const std::string& record : existing) {
            out.write(record.data(), static_cast<std::streamsize>(record.size()));
        }
    } else {
        out.open(local, std::ios::binary | std::ios::trunc);
        out.write(MAGIC.data(), MAGIC.size())
            .write(reinterpret_cast<const char*>(&cache_version), sizeof(cache_version));
    }
    long added = 0;
    for (const std::string& record : remote) {
        if (known.insert(record).second) {
            out.write(record.data(), static_cast<std::streamsize>(record.size()));
            ++added;
        }
    }
    return out ? added : -1;
}

} // namespace VideoCommon
