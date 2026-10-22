// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>

namespace D3D12 {

/// Merges the pipeline records of another console's d3d12.bin into the local one (shader sharing,
/// jadyx/storage/shader_share.h). Returns the records added, -1 if `incoming` is not a cache of
/// this build's format. Call before LoadDiskResources. Standalone so the frontend can call it
/// without the renderer's headers.
long MergeDiskCacheFile(const std::filesystem::path& local, const std::filesystem::path& incoming);

} // namespace D3D12
