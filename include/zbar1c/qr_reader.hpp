#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace zbar1c {
// Each string contains the bytes returned by ZBar; no locale conversion is made.
// No QR code is a successful empty result. Read/decode errors throw std::runtime_error.
std::vector<std::string> read_qr_png(const std::filesystem::path& path);
}
