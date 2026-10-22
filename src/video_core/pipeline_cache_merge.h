// SPDX-FileCopyrightText: Copyright 2026 NSPX
// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <filesystem>

#include "common/common_types.h"

namespace VideoCommon {

/// Appends to `local` the pipeline records of `incoming` that `local` lacks, comparing whole
/// records byte for byte (both in the SerializePipeline format: "yuzucach", version, then records
/// of environments + key). A missing or invalid `local` is started from `incoming`'s header; a
/// truncated tail of either file is ignored. Returns the records added, or -1 when `incoming` is
/// not a cache of `cache_version`. Not safe while the cache is being written: call before
/// LoadDiskResources. Adapted from NXbox's OpenGL ShaderCache::MergeCacheFiles
/// (NXbox by NSPX, https://github.com/NspxMiguel/NXbox, GPL-3.0).
long MergePipelineCacheFiles(const std::filesystem::path& local,
                             const std::filesystem::path& incoming, u32 cache_version,
                             std::size_t graphics_key_size, std::size_t compute_key_size);

} // namespace VideoCommon
