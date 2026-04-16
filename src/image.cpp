#include "mvalgrind.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace mvalgrind {

// Must be kept byte-for-byte identical to docker/Dockerfile.
// CI verifies this with scripts/check-dockerfile-sync.py.
static constexpr const char* DOCKERFILE_CONTENT = R"DOCKERFILE(FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        valgrind gcc g++ make libc6-dbg && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["/bin/bash"]
)DOCKERFILE";

void remove_image(bool verbose) {
    if (verbose) {
        fprintf(stderr, "mvalgrind: removing cached image '%s'...\n", IMAGE_NAME);
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "docker rmi -f %s > /dev/null 2>&1", IMAGE_NAME);
    std::system(cmd);
}

bool ensure_image(bool verbose) {
    // Check whether the image already exists.
    char check_cmd[256];
    snprintf(check_cmd, sizeof(check_cmd), "docker image inspect %s > /dev/null 2>&1",
             IMAGE_NAME);
    int rc = std::system(check_cmd);
    if (rc == 0) {
        if (verbose) {
            fprintf(stderr, "mvalgrind: image '%s' already exists\n", IMAGE_NAME);
        }
        return true;
    }

    fprintf(stderr,
            "mvalgrind: building Docker image '%s' on first run "
            "(this takes ~1–2 minutes)...\n",
            IMAGE_NAME);

    // Write the embedded Dockerfile to a temp file.
    char tmpfile[] = "/tmp/mvalgrind-dockerfile-XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) {
        perror("mvalgrind: mkstemp");
        return false;
    }
    size_t len = strlen(DOCKERFILE_CONTENT);
    if (write(fd, DOCKERFILE_CONTENT, len) != static_cast<ssize_t>(len)) {
        perror("mvalgrind: write Dockerfile");
        close(fd);
        unlink(tmpfile);
        return false;
    }
    close(fd);

    // Build the image.  We use /tmp as the (empty) build context; the
    // Dockerfile has no COPY/ADD instructions so context size is trivial.
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "docker build -t %s -f %s /tmp", IMAGE_NAME, tmpfile);

    if (verbose) {
        fprintf(stderr, "mvalgrind: running: %s\n", cmd);
    }

    rc = std::system(cmd);
    unlink(tmpfile);

    if (rc != 0) {
        // system() returns the raw waitpid status; extract the actual exit code.
        int code = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
        fprintf(stderr, "mvalgrind: docker build failed (exit %d)\n", code);
        return false;
    }

    fprintf(stderr, "mvalgrind: image built successfully.\n");
    return true;
}

}  // namespace mvalgrind
