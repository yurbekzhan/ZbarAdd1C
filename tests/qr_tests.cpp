#include "zbar1c/qr_reader.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Fixture path required");
        const std::filesystem::path fixtures(argv[1]);
        for (const char* name : {"qr.png", "palette.png", "transparent.png", "rotated.png"}) {
            const auto result = zbar1c::read_qr_png(fixtures / name);
            require(result == std::vector<std::string>{"https://example.org/1c?qr=12345"}, name);
        }
        auto multi = zbar1c::read_qr_png(fixtures / "multiple.png");
        std::sort(multi.begin(), multi.end());
        require(multi == std::vector<std::string>{"FIRST", "SECOND"}, "Multiple QR codes");
        require(zbar1c::read_qr_png(fixtures / "blank.png").empty(), "Blank PNG");
        const auto unicode = zbar1c::read_qr_png(fixtures / "unicode.png");
        require(unicode == std::vector<std::string>{u8"Привет, 1С!"}, "Unicode payload");
        for (const char* name : {"missing.png", "broken.png", "truncated.png"}) {
            bool failed = false;
            try { (void)zbar1c::read_qr_png(fixtures / name); }
            catch (const std::runtime_error&) { failed = true; }
            require(failed, name);
        }
        std::cout << "10 functional cases passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
