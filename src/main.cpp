#include <cstdio>
#include <cstdlib>
#include <variant>

#include "mvalgrind.hpp"

int main(int argc, char* argv[]) {
    auto result = mvalgrind::parse_args(argc, argv);

    if (std::holds_alternative<mvalgrind::ParseError>(result)) {
        fprintf(stderr, "mvalgrind: %s\n", std::get<mvalgrind::ParseError>(result).message.c_str());
        fprintf(stderr, "Run 'mvalgrind --help' for usage.\n");
        return 2;
    }

    mvalgrind::Args args = std::get<mvalgrind::Args>(result);

    if (args.help) {
        printf("%s", mvalgrind::help_string().c_str());
        return 0;
    }
    if (args.version) {
        printf("%s\n", mvalgrind::version_string().c_str());
        return 0;
    }

    // Classify before the Docker check — gives clear errors for bad targets even
    // when Docker isn't running (e.g., "that's a Mac binary").
    mvalgrind::FileType ft = mvalgrind::classify(args.target);
    switch (ft) {
        case mvalgrind::FileType::NotFound:
            fprintf(stderr, "mvalgrind: '%s': no such file or directory\n", args.target.c_str());
            return 2;

        case mvalgrind::FileType::IsDirectory:
            fprintf(stderr, "mvalgrind: '%s' is a directory, not a runnable file\n",
                    args.target.c_str());
            return 2;

        case mvalgrind::FileType::MachOBinary:
            fprintf(stderr,
                    "mvalgrind: '%s' is a Mac binary — it won't run in a Linux "
                    "container.\n"
                    "  Point mvalgrind at the source file instead.\n",
                    args.target.c_str());
            return 2;

        case mvalgrind::FileType::Unknown:
            fprintf(stderr,
                    "mvalgrind: '%s' is not a recognised file type.\n"
                    "  Supported: .c / .C / .cpp / .cc / .cxx / .c++ source files, "
                    "or Linux ELF binaries.\n",
                    args.target.c_str());
            return 2;

        case mvalgrind::FileType::CSource:
        case mvalgrind::FileType::CppSource:
        case mvalgrind::FileType::ElfBinary:
            break;
    }

    // Verify Docker daemon is reachable (after classification so bad-file errors
    // are reported even when Docker isn't running).
    // docker ps is ~20ms; docker info is ~500ms — don't penalise every invocation.
    if (std::system("docker ps -q > /dev/null 2>&1") != 0) {
        fprintf(stderr,
                "mvalgrind: Docker is not running or not installed.\n"
                "  Install Docker Desktop: "
                "https://www.docker.com/products/docker-desktop/\n");
        return 2;
    }

    // If the user asked for a fresh image, remove the cached one first.
    if (args.mv_rebuild_image) {
        mvalgrind::remove_image(true);
    }

    // Ensure the Docker image exists, building it on first run.
    if (!mvalgrind::ensure_image(args.mv_verbose)) {
        fprintf(stderr, "mvalgrind: failed to build Docker image — aborting.\n");
        return 2;
    }

    mvalgrind::RunConfig cfg;
    cfg.args = args;
    cfg.file_type = ft;
    cfg.keep_container = args.mv_keep;
    cfg.verbose = args.mv_verbose;

    return mvalgrind::run(cfg);
}
