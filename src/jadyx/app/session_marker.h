// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Notices that the last game session ended without a clean shutdown (ported from Cemu-UWP's
// HostTools). A marker file exists while a game runs; if it is still there at the next start, the
// app was closed with a game running. The diag log since the marker tells a crash ("CRASH:" from
// the unhandled-exception filter) from a silent end (frozen, closed from the Xbox menu, or out of
// memory). A session the console ended while the app was suspended is not reported.
namespace Jadyx::SessionMarker {

struct Paths {
    std::filesystem::path marker; ///< LocalState\jadyx\log\session.marker
    std::filesystem::path diag;   ///< jadyx_diag.txt
};

void MarkStarted(const Paths& paths, const std::string& game);
void MarkStopped(const Paths& paths);
void MarkSuspended(const Paths& paths, bool suspended);

struct LastSession {
    std::string game;
    bool crashed{};
    int memory_percent{-1}; ///< last "app memory X MiB of Y MiB limit", -1 if none
};
/// Reads and removes the marker. Empty if the last session ended cleanly (or while suspended).
std::optional<LastSession> CheckLastSession(const Paths& paths);

} // namespace Jadyx::SessionMarker
