#include "mvalgrind.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace mvalgrind {

// Returns the file extension (including the dot), preserving case.
static std::string file_extension(const std::string& path) {
    auto dot = path.rfind('.');
    auto sep = path.rfind('/');
    if (dot == std::string::npos) return "";
    // Ignore a dot that appears before the last path separator.
    if (sep != std::string::npos && dot < sep) return "";
    return path.substr(dot);
}

// Read the first four bytes of a regular file.
static bool read_magic(const std::string& path, uint8_t out[4]) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    size_t n = fread(out, 1, 4, f);
    fclose(f);
    return n == 4;
}

FileType classify(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) return FileType::NotFound;
    if (S_ISDIR(st.st_mode)) return FileType::IsDirectory;

    // Extension takes precedence — trust the user's naming convention.
    std::string ext = file_extension(path);

    // Lowercase .c → C source; uppercase .C follows GCC convention → C++.
    if (ext == ".c") return FileType::CSource;
    if (ext == ".C" || ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c++")
        return FileType::CppSource;

    // No recognised extension — inspect magic bytes.
    uint8_t magic[4];
    if (!read_magic(path, magic)) return FileType::Unknown;

    // ELF: 0x7F 'E' 'L' 'F'
    if (magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F')
        return FileType::ElfBinary;

    // Mach-O magic values (big-endian and little-endian, 32/64-bit, fat binary)
    uint32_t m = (static_cast<uint32_t>(magic[0]) << 24) |
                 (static_cast<uint32_t>(magic[1]) << 16) |
                 (static_cast<uint32_t>(magic[2]) << 8) |
                 static_cast<uint32_t>(magic[3]);
    if (m == 0xFEEDFACEu || m == 0xFEEDFACFu || m == 0xCAFEBABEu || m == 0xCAFEBABFu ||
        m == 0xCEFAEDFEu || m == 0xCFFAEDFEu)
        return FileType::MachOBinary;

    return FileType::Unknown;
}

}  // namespace mvalgrind
