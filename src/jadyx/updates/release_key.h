// SPDX-FileCopyrightText: Copyright 2026 Jadyx Project
// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string_view>
namespace Jadyx::Updates {
// Eden-UWP has no release signing key yet. While this is empty the updater never goes online,
// so it can't pick up another project's releases.
inline constexpr std::string_view ReleasePublicKey = "";
inline constexpr std::string_view PackageName = "EdenUWP.Unofficial";
inline constexpr std::string_view PackagePublisher = "CN=EdenUWP";
inline constexpr std::string_view ReleaseRepository = "PrismFox67/Eden-UWP";
inline constexpr std::string_view AssetPrefix = "EdenUWP_";
}
