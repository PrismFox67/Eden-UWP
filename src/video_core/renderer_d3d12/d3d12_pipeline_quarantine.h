// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>

#include "common/common_types.h"

/// Pipelines that crashed the process inside the driver. CallDriverGuarded recovers from access
/// violations, but not from faults that end the process (Cemu-UWP saw the Series X shader compiler
/// overflow its stack on one Mario Kart 8 pipeline). While a thread hands a pipeline to the driver
/// its key is noted; the crash handler appends that key to <shader dir>/<title>/d3d12_blocked.txt,
/// and later runs skip the pipeline (its draws are skipped) instead of crashing again. Delete the
/// file to retry them, e.g. after a system update. Ported from Cemu-UWP's D3D12PipelineQuarantine.
namespace D3D12::PipelineQuarantine {

/// Loads the title's list. Call before pipelines of that title are built.
void Open(const std::filesystem::path& path);

/// Whether the pipeline crashed the driver in an earlier run.
[[nodiscard]] bool IsBlocked(u64 key);

/// Notes the pipeline the calling thread is creating, for OnCrash.
class Scope {
public:
    explicit Scope(u64 key);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    u64 previous;
};

/// From the unhandled-exception filter: appends the key the crashing thread was creating, if any.
/// Only raw Win32 file calls, no allocation.
void OnCrash() noexcept;

} // namespace D3D12::PipelineQuarantine
