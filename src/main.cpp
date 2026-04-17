#include <cstdio>
#include <cstdlib>
#include <variant>

#include "macgrind.hpp"

int main(int argc, char* argv[]) {
    auto result = macgrind::parse_args(argc, argv);

    if (std::holds_alternative<macgrind::ParseError>(result)) {
        fprintf(stderr, "macgrind: %s\n", std::get<macgrind::ParseError>(result).message.c_str());
        fprintf(stderr, "Run 'macgrind --help' for usage.\n");
        return 2;
    }

    macgrind::Args args = std::get<macgrind::Args>(result);

    if (args.help) {
        printf("%s", macgrind::help_string().c_str());
        return 0;
    }
    if (args.version) {
        printf("%s\n", macgrind::version_string().c_str());
        return 0;
    }

    // Classify before the Docker check — gives clear errors for bad targets even
    // when Docker isn't running (e.g., "that's a Mac binary").
    macgrind::FileType ft = macgrind::classify(args.target);
    switch (ft) {
        case macgrind::FileType::NotFound:
            fprintf(stderr, "macgrind: '%s': no such file or directory\n", args.target.c_str());
            return 2;

        case macgrind::FileType::IsDirectory:
            fprintf(stderr, "macgrind: '%s' is a directory, not a runnable file\n",
                    args.target.c_str());
            return 2;

        case macgrind::FileType::MachOBinary:
            fprintf(stderr,
                    "macgrind: '%s' is a Mac binary — it won't run in a Linux "
                    "container.\n"
                    "  Point macgrind at the source file instead.\n",
                    args.target.c_str());
            return 2;

        case macgrind::FileType::Unknown:
            fprintf(stderr,
                    "macgrind: '%s' is not a recognised file type.\n"
                    "  Supported: .c / .C / .cpp / .cc / .cxx / .c++ source files, "
                    "or Linux ELF binaries.\n",
                    args.target.c_str());
            return 2;

        case macgrind::FileType::CSource:
        case macgrind::FileType::CppSource:
        case macgrind::FileType::ElfBinary:
            break;
    }

    // Verify Docker daemon is reachable (after classification so bad-file errors
    // are reported even when Docker isn't running).
    // docker ps is ~20ms; docker info is ~500ms — don't penalise every invocation.
    if (std::system("docker ps -q > /dev/null 2>&1") != 0) {
        fprintf(stderr,
                "macgrind: Docker is not running or not installed.\n"
                "  Install Docker Desktop: "
                "https://www.docker.com/products/docker-desktop/\n");
        return 2;
    }

    // If the user asked for a fresh image, remove the cached one first.
    if (args.mv_rebuild_image) {
        macgrind::remove_image(true);
    }

    // Ensure the Docker image exists, building it on first run.
    if (!macgrind::ensure_image(args.mv_verbose)) {
        fprintf(stderr, "macgrind: failed to build Docker image — aborting.\n");
        return 2;
    }

    macgrind::RunConfig cfg;
    cfg.args = args;
    cfg.file_type = ft;
    cfg.keep_container = args.mv_keep;
    cfg.verbose = args.mv_verbose;

    return macgrind::run(cfg);
}
