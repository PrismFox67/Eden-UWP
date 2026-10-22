// SPDX-FileCopyrightText: Copyright 2026 NSPX
// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared shader cache over the LAN (Settings > System > Shared shader cache; empty = off).
// The server (tools/shader-share/server.py) keeps one d3d12.bin per title. Before the disk cache
// loads, the console downloads the title's shared cache and merges the pipeline records it lacks,
// so the precompile builds every pipeline any console has met instead of compiling them in game;
// after the session it uploads its merged cache, which the server keeps when it is larger.
// The records are guest shaders and pipeline keys only (no PSO blobs): every console still
// builds its own DXIL and PSOs, at load time instead of mid-game.
// After NXbox's shader_share (NXbox by NSPX, https://github.com/NspxMiguel/NXbox, GPL-3.0),
// moved from its OpenGL cache to the D3D12 one.

#include <cstdint>
#include <string>

namespace Jadyx::ShaderShare {

/// Merges the server's cache for the title into the local one. Blocks up to ~10 s.
void Download(const std::string& base_url, std::uint64_t title_id);
/// Sends the local cache for the title to the server. Blocks up to ~20 s.
void Upload(const std::string& base_url, std::uint64_t title_id);

} // namespace Jadyx::ShaderShare
