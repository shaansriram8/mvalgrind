#include <cstring>
#include <string>
#include <variant>
#include <vector>

#include "mvalgrind.hpp"

namespace mvalgrind {

std::string help_string() {
    return R"(Usage: mvalgrind [valgrind-flags...] <target> [program-args...]

mvalgrind runs Valgrind inside a local Docker container so you get native
Valgrind output on macOS without needing a Linux machine.

  <target>          A C/C++ source file (.c, .cpp, .cc, .cxx, .c++) or a
                    Linux ELF binary.  Source files are compiled inside the
                    container with -g -O0 before Valgrind runs.

  --                End flag scanning; next argument is the target.

mvalgrind-specific flags (will not be passed to Valgrind):
  --mv-keep           Do not --rm the container on exit (debugging aid).
  --mv-verbose        Print the docker command before running it.
  --mv-rebuild-image  Delete and rebuild the cached Docker image from scratch.
                      Use this after upgrading mvalgrind to pick up image changes.
  -h, --help          Show this help and exit.
  -V, --version       Show version and exit.

All other flags are forwarded to Valgrind unchanged.

Examples:
  mvalgrind --leak-check=full ./leak.c
  mvalgrind --track-origins=yes ./uninit.cpp
  mvalgrind --leak-check=full ./my_linux_binary arg1 arg2
  mvalgrind --mv-verbose --leak-check=full ./leak.c
)";
}

std::variant<Args, ParseError> parse_args(int argc, char* argv[]) {
    if (argc < 1) {
        return ParseError{"internal error: argc < 1"};
    }

    Args args;
    bool end_of_flags = false;  // set after "--"
    bool target_found = false;

    for (int i = 1; i < argc; ++i) {
        std::string tok(argv[i]);

        if (target_found) {
            args.program_args.push_back(tok);
            continue;
        }

        if (end_of_flags) {
            args.target = tok;
            target_found = true;
            continue;
        }

        // End-of-flags sentinel
        if (tok == "--") {
            end_of_flags = true;
            continue;
        }

        // Help / version — handled entirely by us
        if (tok == "-h" || tok == "--help") {
            args.help = true;
            return args;
        }
        if (tok == "-V" || tok == "--version") {
            args.version = true;
            return args;
        }

        // Our namespaced flags
        if (tok == "--mv-keep") {
            args.mv_keep = true;
            continue;
        }
        if (tok == "--mv-verbose") {
            args.mv_verbose = true;
            continue;
        }
        if (tok == "--mv-rebuild-image") {
            args.mv_rebuild_image = true;
            continue;
        }
        if (tok.rfind("--mv-", 0) == 0) {
            return ParseError{"unknown mvalgrind flag: " + tok +
                              "\n  (all --mv-* flags are reserved for mvalgrind itself)"};
        }

        // Any other flag → Valgrind
        if (!tok.empty() && tok[0] == '-') {
            args.valgrind_flags.push_back(tok);
            continue;
        }

        // First positional argument → target
        args.target = tok;
        target_found = true;
    }

    if (!target_found && !args.help && !args.version) {
        return ParseError{"no target specified — provide a source file or ELF binary"};
    }

    return args;
}

}  // namespace mvalgrind
