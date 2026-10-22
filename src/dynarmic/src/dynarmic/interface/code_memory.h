// SPDX-FileCopyrightText: Copyright 2026 Jadyx Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

namespace Dynarmic {

/// Bytes committed by every x64 code cache in the process, all JIT instances together. On Windows
/// code memory is committed as code is emitted, so this is what the JIT costs the app's commit;
/// elsewhere the whole cache is committed up front and this stays 0.
std::size_t CommittedCodeBytes() noexcept;

/// Builds with W^X support (Windows): ask code caches created from now on for read/write/execute
/// pages instead of flipping pages between RW and RX. A cache the system refuses RWX keeps W^X.
void SetPreferRwxCodeMemory(bool prefer) noexcept;

/// Whether the last code cache created got read/write/execute pages.
bool RwxCodeMemoryActive() noexcept;

} // namespace Dynarmic
