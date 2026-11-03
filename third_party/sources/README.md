# Bundled dependency sources

All archives contain complete, unmodified Git source trees, including licenses.
Created with `git archive --format=tar.gz --prefix=<project>/ HEAD`.
CMake verifies SHA256 before extracting them into the build directory. No Git,
package manager or network connection is required to configure or build.

| Project | Upstream | Version | Full commit |
| --- | --- | --- | --- |
| ZBar | https://github.com/mchehab/zbar | 0.23.93 | bb05ec54eec57f8397cb13fb9161372a281a1219 |
| libpng | https://github.com/pnggroup/libpng | 1.6.48 | ea127968204cc5d10f3fc9250c306b9e8cbd9b80 |
| zlib | https://github.com/madler/zlib | 1.3.1 | 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf |
| win-iconv | https://github.com/win-iconv/win-iconv | 0.0.8 | c9df88a284d448da5434c6ad2737b54a907f888c |

Build integration is in `cmake/BundledDependencies.cmake`. ZBar includes only the
image scanner and QR decoder: camera, GUI, D-Bus and other barcode types are not
built. libpng uses its upstream prebuilt feature configuration, with optional
CPU-specific SIMD disabled. Windows encoding conversion uses static win-iconv
(Windows code pages), while Linux uses system iconv. This is a custom build
integration; upstream source files are not patched.

ZBar is LGPL-2.1-or-later. libpng and zlib carry their respective permissive
licenses. win-iconv 0.0.8 states in its source that it is in the public domain.
The generated Windows ZIP includes license texts and complete source/build inputs
(including the component), allowing modification and relinking of ZBar. Retain
these materials and notices when passing on the generated package. Adding further
proprietary code requires reassessing the LGPL relinking requirements.
