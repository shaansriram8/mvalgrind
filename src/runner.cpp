#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "macgrind.hpp"

namespace macgrind {

namespace fs = std::filesystem;

// ── async-signal-safe global state ───────────────────────────────────────────

// Container name is stored in a fixed-size char array so the signal handler
// can reference it without calling malloc or touching a std::string.
static char g_container_name[128] = "";
static volatile sig_atomic_t g_signal_count = 0;

// Fork + exec 'docker rm -f <container>' and wait for it.
// Called from the signal handler — every function used here must be
// async-signal-safe (fork, execlp, waitpid, open, dup2, close, _exit, write).
static void kill_container() {
    if (g_container_name[0] == '\0') return;

    pid_t pid = fork();
    if (pid == 0) {
        // Silence docker's output so it doesn't clobber the user's terminal.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("docker", "docker", "rm", "-f", g_container_name, static_cast<char*>(nullptr));
        _exit(1);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}

static void signal_handler(int sig) {
    // On a second signal the user is really impatient — get out immediately.
    if (g_signal_count > 0) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    g_signal_count = 1;

    const char msg[] = "\nmacgrind: caught signal, removing container...\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);

    kill_container();

    // Restore default and re-raise so the shell sees the correct exit status.
    signal(sig, SIG_DFL);
    raise(sig);
}

// ── shell quoting ─────────────────────────────────────────────────────────────

// Wraps s in single quotes and escapes any embedded single quotes as '\''.
// This is the only safe way to pass arbitrary strings through bash -c.
static std::string shell_quote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '\'';
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

// ── run ───────────────────────────────────────────────────────────────────────

int run(const RunConfig& cfg) {
    const Args& args = cfg.args;

    // Resolve the target to an absolute path so the bind-mount is unambiguous.
    std::error_code ec;
    fs::path abs_target = fs::absolute(fs::path(args.target), ec);
    if (ec) {
        fprintf(stderr, "macgrind: cannot resolve path '%s': %s\n", args.target.c_str(),
                ec.message().c_str());
        return 2;
    }

    std::string mount_dir = abs_target.parent_path().string();
    std::string filename = abs_target.filename().string();

    // Set the global container name before installing signal handlers.
    snprintf(g_container_name, sizeof(g_container_name), "macgrind-run-%d",
             static_cast<int>(getpid()));

    // Install handlers for the signals that typically interrupt interactive use.
    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    // ── Build the shell command that runs inside the container ──────────────

    // All user-supplied values are single-quoted so embedded spaces, shell
    // metacharacters, and adversarial flags can't escape into the bash command.
    std::string inner_cmd;

    if (cfg.file_type == FileType::CSource || cfg.file_type == FileType::CppSource) {
        const char* compiler = (cfg.file_type == FileType::CSource) ? "gcc" : "g++";
        std::string src_in_container = "/work/" + filename;

        inner_cmd = compiler;
        inner_cmd += " -g -O0 -Wall -Wextra -o /tmp/macgrind_prog ";
        inner_cmd += shell_quote(src_in_container);
        inner_cmd += " && valgrind";

        for (const auto& flag : args.valgrind_flags) {
            inner_cmd += ' ';
            inner_cmd += shell_quote(flag);
        }
        inner_cmd += " /tmp/macgrind_prog";
        for (const auto& parg : args.program_args) {
            inner_cmd += ' ';
            inner_cmd += shell_quote(parg);
        }
    } else {
        // ELF binary already in the container via the bind-mount.
        std::string bin_in_container = "/work/" + filename;
        inner_cmd = "valgrind";
        for (const auto& flag : args.valgrind_flags) {
            inner_cmd += ' ';
            inner_cmd += shell_quote(flag);
        }
        inner_cmd += ' ';
        inner_cmd += shell_quote(bin_in_container);
        for (const auto& parg : args.program_args) {
            inner_cmd += ' ';
            inner_cmd += shell_quote(parg);
        }
    }

    // ── Assemble the docker-run argv ────────────────────────────────────────

    std::vector<std::string> docker_argv;
    docker_argv.emplace_back("docker");
    docker_argv.emplace_back("run");

    if (!cfg.keep_container) {
        docker_argv.emplace_back("--rm");
    }

    docker_argv.emplace_back("--name");
    docker_argv.emplace_back(g_container_name);

    // Bind-mount the directory that contains the target (read-only).
    docker_argv.emplace_back("-v");
    docker_argv.emplace_back(mount_dir + ":/work:ro");

    // Mirror TTY state so Valgrind's colour output and interactive programs
    // behave the same as they would natively.
    if (isatty(STDIN_FILENO)) docker_argv.emplace_back("-i");
    if (isatty(STDOUT_FILENO)) docker_argv.emplace_back("-t");

    docker_argv.emplace_back(IMAGE_NAME);
    docker_argv.emplace_back("bash");
    docker_argv.emplace_back("-c");
    docker_argv.emplace_back(inner_cmd);

    if (cfg.verbose) {
        fprintf(stderr, "macgrind: running:");
        for (const auto& s : docker_argv) fprintf(stderr, " %s", s.c_str());
        fprintf(stderr, "\n");
    }

    // Convert to the null-terminated char* array that execvp expects.
    std::vector<char*> execvp_argv;
    execvp_argv.reserve(docker_argv.size() + 1);
    for (auto& s : docker_argv) execvp_argv.push_back(s.data());
    execvp_argv.push_back(nullptr);

    // ── Fork + exec ─────────────────────────────────────────────────────────

    pid_t child = fork();
    if (child < 0) {
        perror("macgrind: fork");
        return 2;
    }
    if (child == 0) {
        execvp("docker", execvp_argv.data());
        perror("macgrind: exec docker");
        _exit(2);
    }

    // Parent: wait, restarting on EINTR (signals will also call kill_container
    // via the handler and then re-raise, so this loop normally exits cleanly).
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("macgrind: waitpid");
            return 2;
        }
    }

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 2;
}

}  // namespace macgrind
