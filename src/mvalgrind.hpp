#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mvalgrind {

// ── version ───────────────────────────────────────────────────────────────────

#ifndef MVALGRIND_VERSION
#define MVALGRIND_VERSION "0.1.0"
#endif

inline std::string version_string() { return "mvalgrind " MVALGRIND_VERSION; }
std::string help_string();

// ── args ──────────────────────────────────────────────────────────────────────

struct Args {
    std::vector<std::string> valgrind_flags;
    std::string target;
    std::vector<std::string> program_args;
    bool mv_keep          = false;
    bool mv_verbose       = false;
    bool mv_rebuild_image = false;
    bool help             = false;
    bool version          = false;
};

struct ParseError {
    std::string message;
};

std::variant<Args, ParseError> parse_args(int argc, char* argv[]);

// ── classify ──────────────────────────────────────────────────────────────────

enum class FileType {
    CSource,
    CppSource,
    ElfBinary,
    MachOBinary,
    Unknown,
    NotFound,
    IsDirectory,
};

FileType classify(const std::string& path);

// ── image ─────────────────────────────────────────────────────────────────────

static constexpr const char* IMAGE_NAME = "mvalgrind-ubuntu:latest";

// Ensures the Docker image exists, building it on first run.
// Returns true on success, false on failure.
bool ensure_image(bool verbose);

// Force-removes the cached image so the next ensure_image() does a fresh build.
void remove_image(bool verbose);

// ── runner ────────────────────────────────────────────────────────────────────

struct RunConfig {
    Args      args;
    FileType  file_type;
    bool      keep_container = false;
    bool      verbose        = false;
};

// Forks docker run, wires signals, waits for completion.
// Returns the child's exit code, or 2 on orchestration error.
int run(const RunConfig& cfg);

}  // namespace mvalgrind
