#include "zbar1c/qr_reader.hpp"

#include <png.h>
#include <zbar.h>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <cstdint>

namespace zbar1c {
namespace {
constexpr std::uintmax_t max_file_bytes = 64 * 1024 * 1024;
constexpr std::uint64_t max_pixels = 32 * 1024 * 1024;
struct PngImage {
    png_image value{};
    PngImage() { value.version = PNG_IMAGE_VERSION; }
    ~PngImage() { png_image_free(&value); }
};
}

std::vector<std::string> read_qr_png(const std::filesystem::path& path) {
    // filesystem::path preserves Unicode file names on Windows.
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Cannot open PNG file");
    const auto size = stream.tellg();
    if (size <= 0 || static_cast<std::uintmax_t>(size) > max_file_bytes)
        throw std::runtime_error("PNG file is empty or exceeds 64 MiB");
    std::vector<unsigned char> encoded(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(encoded.data()), size))
        throw std::runtime_error("Cannot read PNG file");

    PngImage png;
    if (!png_image_begin_read_from_memory(&png.value, encoded.data(), encoded.size()))
        throw std::runtime_error(std::string("Invalid PNG: ") + png.value.message);
    const auto width = png.value.width;
    const auto height = png.value.height;
    if (!width || !height || static_cast<std::uint64_t>(width) * height > max_pixels)
        throw std::runtime_error("PNG dimensions exceed 32 megapixels");

    // Decode all PNG variants to RGBA, then composite transparency on white.
    png.value.format = PNG_FORMAT_RGBA;
    std::vector<unsigned char> rgba(PNG_IMAGE_SIZE(png.value));
    if (!png_image_finish_read(&png.value, nullptr, rgba.data(), 0, nullptr))
        throw std::runtime_error(std::string("Cannot decode PNG: ") + png.value.message);
    std::vector<unsigned char> gray(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < gray.size(); ++i) {
        const auto* p = &rgba[i * 4];
        const unsigned luminance = (299u * p[0] + 587u * p[1] + 114u * p[2] + 500u) / 1000u;
        gray[i] = static_cast<unsigned char>((luminance * p[3] + 255u * (255u - p[3]) + 127u) / 255u);
    }

    using Scanner = std::unique_ptr<zbar::zbar_image_scanner_t, decltype(&zbar::zbar_image_scanner_destroy)>;
    using Image = std::unique_ptr<zbar::zbar_image_t, decltype(&zbar::zbar_image_destroy)>;
    Scanner scanner(zbar::zbar_image_scanner_create(), zbar::zbar_image_scanner_destroy);
    Image image(zbar::zbar_image_create(), zbar::zbar_image_destroy);
    if (!scanner || !image) throw std::runtime_error("Cannot allocate ZBar scanner");
    if (zbar::zbar_image_scanner_set_config(scanner.get(), zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0) ||
        zbar::zbar_image_scanner_set_config(scanner.get(), zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1))
        throw std::runtime_error("Cannot enable ZBar QR decoder");
    zbar::zbar_image_set_format(image.get(), zbar_fourcc('Y', '8', '0', '0'));
    zbar::zbar_image_set_size(image.get(), width, height);
    // gray outlives image; ZBar must not free this vector's storage.
    zbar::zbar_image_set_data(image.get(), gray.data(), gray.size(), nullptr);
    if (zbar::zbar_scan_image(scanner.get(), image.get()) < 0)
        throw std::runtime_error("ZBar scan failed");
    std::vector<std::string> result;
    for (auto symbol = zbar::zbar_image_first_symbol(image.get()); symbol;
         symbol = zbar::zbar_symbol_next(symbol)) {
        if (zbar::zbar_symbol_get_type(symbol) == zbar::ZBAR_QRCODE)
            result.emplace_back(zbar::zbar_symbol_get_data(symbol), zbar::zbar_symbol_get_data_length(symbol));
    }
    return result;
}
}
