// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include <dynarmic/interface/code_memory.h>
#include <dynarmic/interface/halt_reason.h>

#include "common/logging.h"

#include "core/arm/arm_interface.h"

namespace Core {

constexpr Dynarmic::HaltReason StepThread = Dynarmic::HaltReason::Step;
constexpr Dynarmic::HaltReason DataAbort = Dynarmic::HaltReason::MemoryAbort;
constexpr Dynarmic::HaltReason BreakLoop = Dynarmic::HaltReason::UserDefined2;
constexpr Dynarmic::HaltReason SupervisorCall = Dynarmic::HaltReason::UserDefined3;
constexpr Dynarmic::HaltReason InstructionBreakpoint = Dynarmic::HaltReason::UserDefined4;
constexpr Dynarmic::HaltReason PrefetchAbort = Dynarmic::HaltReason::UserDefined6;

constexpr HaltReason TranslateHaltReason(Dynarmic::HaltReason hr) {
    static_assert(u64(HaltReason::StepThread) == u64(StepThread));
    static_assert(u64(HaltReason::DataAbort) == u64(DataAbort));
    static_assert(u64(HaltReason::BreakLoop) == u64(BreakLoop));
    static_assert(u64(HaltReason::SupervisorCall) == u64(SupervisorCall));
    static_assert(u64(HaltReason::InstructionBreakpoint) == u64(InstructionBreakpoint));
    static_assert(u64(HaltReason::PrefetchAbort) == u64(PrefetchAbort));
    return HaltReason(hr);
}

/// Per-core JIT code cache size for the JITs created afterwards; 0 keeps the default. A full cache
/// is cleared at once together with every block's metadata (links, ranges), so a smaller one
/// bounds the whole JIT's memory at the cost of a recompilation burst each time it fills.
inline std::atomic<u32> g_jit_code_cache_size{};
inline void SetJitCodeCacheSize(u32 bytes) {
    g_jit_code_cache_size.store(bytes, std::memory_order_relaxed);
}

/// Logs whether the JIT just created runs from read/write/execute or W^X code memory, on changes.
inline void LogJitCodeMemoryMode() {
    static std::atomic<int> logged{-1};
    const int rwx = Dynarmic::RwxCodeMemoryActive() ? 1 : 0;
    if (logged.exchange(rwx, std::memory_order_relaxed) != rwx) {
        LOG_INFO(Core_ARM, "JIT code memory: {}", rwx ? "read/write/execute" : "W^X");
    }
}
inline u32 JitCodeCacheSize(u32 default_bytes) {
    const u32 bytes = g_jit_code_cache_size.load(std::memory_order_relaxed);
    return bytes != 0 ? bytes : default_bytes;
}

} // namespace Core
