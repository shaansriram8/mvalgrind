#include <fcntl.h>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <string>

#include "macgrind.hpp"

// ── Helpers ───────────────────────────────────────────────────────────────────

// Create a regular temp file with a given extension and optional content.
// Returns the path; the caller owns deletion via unlink().
static std::string make_temp(const std::string& suffix, const uint8_t* data = nullptr,
                             size_t len = 0) {
    // mkstemps needs the X's immediately before the suffix.
    std::string tmpl = "/tmp/macgrind_test_XXXXXX" + suffix;
    int fd = mkstemps(tmpl.data(), static_cast<int>(suffix.size()));
    if (fd < 0) return "";
    if (data && len > 0) {
        (void)write(fd, data, len);
    }
    close(fd);
    return tmpl;
}

// ── Extension tests ───────────────────────────────────────────────────────────

TEST_CASE(".c extension → CSource", "[classify]") {
    auto path = make_temp(".c");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CSource);
    unlink(path.c_str());
}

TEST_CASE(".cpp extension → CppSource", "[classify]") {
    auto path = make_temp(".cpp");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

TEST_CASE(".cc extension → CppSource", "[classify]") {
    auto path = make_temp(".cc");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

TEST_CASE(".cxx extension → CppSource", "[classify]") {
    auto path = make_temp(".cxx");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

TEST_CASE(".c++ extension → CppSource", "[classify]") {
    auto path = make_temp(".c++");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

TEST_CASE(".C (uppercase) extension → CppSource (GCC convention)", "[classify]") {
    auto path = make_temp(".C");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

// ── Magic byte tests ──────────────────────────────────────────────────────────

TEST_CASE("ELF magic bytes → ElfBinary", "[classify]") {
    static const uint8_t elf[] = {0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00};
    auto path = make_temp("", elf, sizeof(elf));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::ElfBinary);
    unlink(path.c_str());
}

TEST_CASE("Mach-O 32-bit (big-endian) magic → MachOBinary", "[classify]") {
    // 0xFEEDFACE in big-endian
    static const uint8_t macho[] = {0xFE, 0xED, 0xFA, 0xCE};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

TEST_CASE("Mach-O 64-bit (big-endian) magic → MachOBinary", "[classify]") {
    // 0xFEEDFACF
    static const uint8_t macho[] = {0xFE, 0xED, 0xFA, 0xCF};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

TEST_CASE("Mach-O fat binary magic → MachOBinary", "[classify]") {
    // 0xCAFEBABE
    static const uint8_t macho[] = {0xCA, 0xFE, 0xBA, 0xBE};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

TEST_CASE("Mach-O 32-bit (little-endian) magic → MachOBinary", "[classify]") {
    // 0xCEFAEDFE (reversed 0xFEEDFACE)
    static const uint8_t macho[] = {0xCE, 0xFA, 0xED, 0xFE};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

TEST_CASE("Mach-O 64-bit (little-endian) magic → MachOBinary", "[classify]") {
    // 0xCFFAEDFE (reversed 0xFEEDFACF)
    static const uint8_t macho[] = {0xCF, 0xFA, 0xED, 0xFE};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST_CASE("unknown file type", "[classify]") {
    static const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    auto path = make_temp("", data, sizeof(data));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

TEST_CASE("file too short for magic", "[classify]") {
    static const uint8_t data[] = {0x7F};  // only 1 byte
    auto path = make_temp("", data, sizeof(data));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

TEST_CASE("nonexistent file → NotFound", "[classify]") {
    CHECK(macgrind::classify("/tmp/macgrind_does_not_exist_xyz") == macgrind::FileType::NotFound);
}

TEST_CASE("directory → IsDirectory", "[classify]") {
    CHECK(macgrind::classify("/tmp") == macgrind::FileType::IsDirectory);
}

// Extension takes precedence over magic bytes: a file named .c is C source
// even if it has ELF magic bytes inside.
TEST_CASE("extension beats magic bytes", "[classify]") {
    static const uint8_t elf[] = {0x7F, 'E', 'L', 'F', 0, 0, 0, 0};
    auto path = make_temp(".c", elf, sizeof(elf));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CSource);
    unlink(path.c_str());
}

TEST_CASE("extension beats magic for cpp too", "[classify]") {
    static const uint8_t elf[] = {0x7F, 'E', 'L', 'F', 0, 0, 0, 0};
    auto path = make_temp(".cpp", elf, sizeof(elf));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::CppSource);
    unlink(path.c_str());
}

// ── File content edge cases ───────────────────────────────────────────────────

TEST_CASE("empty file has no magic", "[classify]") {
    auto path = make_temp("");  // no content written
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

TEST_CASE("3-byte file is too short for magic", "[classify]") {
    static const uint8_t data[] = {0x7F, 'E', 'L'};  // ELF prefix but only 3 bytes
    auto path = make_temp("", data, sizeof(data));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

// ── Unrecognised extensions fall through to magic check ───────────────────────

TEST_CASE("dot-h header extension falls through to magic", "[classify]") {
    // No magic bytes → Unknown (not a source file we can compile).
    auto path = make_temp(".h");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

TEST_CASE("dot-py extension falls through to magic", "[classify]") {
    auto path = make_temp(".py");
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::Unknown);
    unlink(path.c_str());
}

TEST_CASE("no extension falls through to ELF magic check", "[classify]") {
    static const uint8_t elf[] = {0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00};
    auto path = make_temp("", elf, sizeof(elf));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::ElfBinary);
    unlink(path.c_str());
}

// ── Mach-O variant not yet covered ───────────────────────────────────────────

TEST_CASE("Mach-O fat binary variant 0xCAFEBABF", "[classify]") {
    static const uint8_t macho[] = {0xCA, 0xFE, 0xBA, 0xBF};
    auto path = make_temp("", macho, sizeof(macho));
    REQUIRE(!path.empty());
    CHECK(macgrind::classify(path) == macgrind::FileType::MachOBinary);
    unlink(path.c_str());
}

// ── Real host binary ──────────────────────────────────────────────────────────

// /bin/ls on macOS is a Mach-O binary; on Linux it is an ELF.
TEST_CASE("real host ls binary is classified correctly", "[classify]") {
    auto ft = macgrind::classify("/bin/ls");
#if defined(__APPLE__)
    CHECK(ft == macgrind::FileType::MachOBinary);
#else
    CHECK(ft == macgrind::FileType::ElfBinary);
#endif
}
