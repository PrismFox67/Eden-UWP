// SPDX-FileCopyrightText: Copyright 2026 NSPX
// SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "jadyx/storage/shader_share.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <vector>

#include <fmt/format.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>

#include "common/fs/path_util.h"
#include "common/logging.h"
#include "jadyx/network/http.h"
#include "video_core/renderer_d3d12/d3d12_disk_cache_merge.h"

namespace Jadyx::ShaderShare {
namespace {

constexpr std::size_t MaxCacheBytes = 64u << 20;

std::filesystem::path LocalCache(std::uint64_t title_id) {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir) /
           fmt::format("{:016x}", title_id) / "d3d12.bin";
}

std::string CacheUrl(std::string base, std::uint64_t title_id) {
    while (!base.empty() && base.back() == '/') base.pop_back();
    return fmt::format("{}/{:016x}/d3d12.bin", base, title_id);
}

} // namespace

void Download(const std::string& base_url, std::uint64_t title_id) {
    if (base_url.empty() || title_id == 0) return;
    std::stop_source stop;
    const auto body = Http::Get(CacheUrl(base_url, title_id), stop.get_token(), MaxCacheBytes);
    if (!body || body->empty()) {
        LOG_INFO(Frontend, "Shader share: no shared cache for {:016x}", title_id);
        return;
    }
    try {
        const auto local = LocalCache(title_id);
        std::filesystem::create_directories(local.parent_path());
        const auto incoming = local.parent_path() / "d3d12.shared.bin";
        std::ofstream(incoming, std::ios::binary | std::ios::trunc)
            .write(body->data(), static_cast<std::streamsize>(body->size()));
        const long added = D3D12::MergeDiskCacheFile(local, incoming);
        std::error_code ec;
        std::filesystem::remove(incoming, ec);
        if (added < 0) {
            LOG_WARNING(Frontend, "Shader share: the shared cache ({} bytes) is not this build's format",
                        body->size());
        } else {
            LOG_INFO(Frontend, "Shader share: downloaded {} bytes, merged {} new pipelines",
                     body->size(), added);
        }
    } catch (const std::exception& e) {
        LOG_WARNING(Frontend, "Shader share: download failed: {}", e.what());
    }
}

void Upload(const std::string& base_url, std::uint64_t title_id) {
    if (base_url.empty() || title_id == 0) return;
    using namespace winrt::Windows::Web::Http;
    using namespace winrt::Windows::Storage::Streams;
    try {
        const auto local = LocalCache(title_id);
        std::error_code ec;
        const auto size = std::filesystem::file_size(local, ec);
        if (ec || size <= 12 || size > MaxCacheBytes) return;
        std::vector<std::uint8_t> bytes(size);
        std::ifstream(local, std::ios::binary)
            .read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
        DataWriter writer;
        writer.WriteBytes(bytes);
        HttpClient client;
        const winrt::Windows::Foundation::Uri uri{winrt::to_hstring(CacheUrl(base_url, title_id))};
        auto put = client.PutAsync(uri, HttpBufferContent{writer.DetachBuffer()});
        if (put.wait_for(std::chrono::seconds{20}) != winrt::Windows::Foundation::AsyncStatus::Completed) {
            put.Cancel();
            LOG_WARNING(Frontend, "Shader share: upload timed out");
            return;
        }
        LOG_INFO(Frontend, "Shader share: uploaded {} bytes, status {}", size,
                 static_cast<int>(put.GetResults().StatusCode()));
    } catch (const winrt::hresult_error& e) {
        LOG_WARNING(Frontend, "Shader share: upload failed: {}", winrt::to_string(e.message()));
    } catch (const std::exception& e) {
        LOG_WARNING(Frontend, "Shader share: upload failed: {}", e.what());
    }
}

} // namespace Jadyx::ShaderShare
