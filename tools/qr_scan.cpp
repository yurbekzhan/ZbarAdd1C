#include "zbar1c/qr_reader.hpp"
#include <iostream>

static int run(const std::filesystem::path& path) {
    try {
        const auto codes = zbar1c::read_qr_png(path);
        for (const auto& code : codes) {
            std::cout.write(code.data(), static_cast<std::streamsize>(code.size()));
            std::cout << '\n';
        }
        return codes.empty() ? 2 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 2) {
        std::cerr << "Usage: qr_scan <image.png>\n";
        return 1;
    }
    return run(argv[1]);
}
