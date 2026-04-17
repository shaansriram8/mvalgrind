#include <catch2/catch_test_macros.hpp>
#include <string>
#include <variant>
#include <vector>

#include "macgrind.hpp"

// Helper: build a null-terminated argv from a list of strings and call parse_args.
static macgrind::Args must_parse(std::vector<std::string> tokens) {
    std::vector<char*> argv;
    for (auto& s : tokens) argv.push_back(s.data());
    argv.push_back(nullptr);
    auto result = macgrind::parse_args(static_cast<int>(tokens.size()), argv.data());
    REQUIRE(std::holds_alternative<macgrind::Args>(result));
    return std::get<macgrind::Args>(result);
}

static macgrind::ParseError must_fail(std::vector<std::string> tokens) {
    std::vector<char*> argv;
    for (auto& s : tokens) argv.push_back(s.data());
    argv.push_back(nullptr);
    auto result = macgrind::parse_args(static_cast<int>(tokens.size()), argv.data());
    REQUIRE(std::holds_alternative<macgrind::ParseError>(result));
    return std::get<macgrind::ParseError>(result);
}

// ── Basic parsing ─────────────────────────────────────────────────────────────

TEST_CASE("bare target", "[args]") {
    auto a = must_parse({"macgrind", "./my_prog"});
    CHECK(a.target == "./my_prog");
    CHECK(a.valgrind_flags.empty());
    CHECK(a.program_args.empty());
    CHECK(!a.mv_keep);
    CHECK(!a.mv_verbose);
}

TEST_CASE("valgrind flag before target", "[args]") {
    auto a = must_parse({"macgrind", "--leak-check=full", "./prog"});
    REQUIRE(a.valgrind_flags.size() == 1);
    CHECK(a.valgrind_flags[0] == "--leak-check=full");
    CHECK(a.target == "./prog");
}

TEST_CASE("multiple valgrind flags", "[args]") {
    auto a = must_parse({"macgrind", "--leak-check=full", "--track-origins=yes", "-v", "./prog"});
    REQUIRE(a.valgrind_flags.size() == 3);
    CHECK(a.valgrind_flags[0] == "--leak-check=full");
    CHECK(a.valgrind_flags[1] == "--track-origins=yes");
    CHECK(a.valgrind_flags[2] == "-v");
    CHECK(a.target == "./prog");
}

TEST_CASE("program args after target", "[args]") {
    auto a = must_parse({"macgrind", "./prog", "arg1", "arg2", "--flag"});
    CHECK(a.target == "./prog");
    REQUIRE(a.program_args.size() == 3);
    CHECK(a.program_args[0] == "arg1");
    CHECK(a.program_args[1] == "arg2");
    // Flags after the target are program args, not Valgrind flags.
    CHECK(a.program_args[2] == "--flag");
}

TEST_CASE("flags + target + program args", "[args]") {
    auto a = must_parse({"macgrind", "--leak-check=full", "./prog", "foo", "--bar"});
    REQUIRE(a.valgrind_flags.size() == 1);
    CHECK(a.target == "./prog");
    REQUIRE(a.program_args.size() == 2);
    CHECK(a.program_args[1] == "--bar");
}

// ── --mv-* flags ──────────────────────────────────────────────────────────────

TEST_CASE("flag mv-keep sets mv_keep", "[args]") {
    auto a = must_parse({"macgrind", "--mv-keep", "./prog"});
    CHECK(a.mv_keep);
    CHECK(!a.mv_verbose);
}

TEST_CASE("flag mv-verbose sets mv_verbose", "[args]") {
    auto a = must_parse({"macgrind", "--mv-verbose", "./prog"});
    CHECK(a.mv_verbose);
    CHECK(!a.mv_keep);
}

TEST_CASE("both mv flags together", "[args]") {
    auto a = must_parse({"macgrind", "--mv-keep", "--mv-verbose", "./prog"});
    CHECK(a.mv_keep);
    CHECK(a.mv_verbose);
}

TEST_CASE("unknown mv flag is an error", "[args]") {
    auto e = must_fail({"macgrind", "--mv-unknown", "./prog"});
    CHECK(e.message.find("--mv-unknown") != std::string::npos);
}

// ── -- end-of-flags sentinel ──────────────────────────────────────────────────

TEST_CASE("double-dash stops flag parsing", "[args]") {
    auto a = must_parse({"macgrind", "--leak-check=full", "--", "./prog", "--prog-flag"});
    REQUIRE(a.valgrind_flags.size() == 1);
    CHECK(a.target == "./prog");
    REQUIRE(a.program_args.size() == 1);
    CHECK(a.program_args[0] == "--prog-flag");
}

TEST_CASE("double-dash immediately before target", "[args]") {
    auto a = must_parse({"macgrind", "--", "prog"});
    CHECK(a.target == "prog");
    CHECK(a.valgrind_flags.empty());
}

// ── Help / version ────────────────────────────────────────────────────────────

TEST_CASE("short help flag", "[args]") {
    auto a = must_parse({"macgrind", "-h"});
    CHECK(a.help);
}

TEST_CASE("long help flag", "[args]") {
    auto a = must_parse({"macgrind", "--help"});
    CHECK(a.help);
}

TEST_CASE("short version flag", "[args]") {
    auto a = must_parse({"macgrind", "-V"});
    CHECK(a.version);
}

TEST_CASE("long version flag", "[args]") {
    auto a = must_parse({"macgrind", "--version"});
    CHECK(a.version);
}

// ── Flag ordering and mixing ──────────────────────────────────────────────────

TEST_CASE("valgrind flag order is preserved", "[args]") {
    auto a = must_parse({"macgrind", "--show-leak-kinds=all", "--leak-check=full",
                         "--track-origins=yes", "./prog"});
    REQUIRE(a.valgrind_flags.size() == 3);
    CHECK(a.valgrind_flags[0] == "--show-leak-kinds=all");
    CHECK(a.valgrind_flags[1] == "--leak-check=full");
    CHECK(a.valgrind_flags[2] == "--track-origins=yes");
}

TEST_CASE("single-dash short flags pass through to valgrind", "[args]") {
    auto a = must_parse({"macgrind", "-v", "-q", "./prog"});
    REQUIRE(a.valgrind_flags.size() == 2);
    CHECK(a.valgrind_flags[0] == "-v");
    CHECK(a.valgrind_flags[1] == "-q");
}

TEST_CASE("mv flags do not appear in valgrind_flags", "[args]") {
    auto a = must_parse({"macgrind", "--mv-verbose", "--leak-check=full", "--mv-keep", "./prog"});
    REQUIRE(a.valgrind_flags.size() == 1);
    CHECK(a.valgrind_flags[0] == "--leak-check=full");
    CHECK(a.mv_keep);
    CHECK(a.mv_verbose);
}

TEST_CASE("full kitchen-sink parse", "[args]") {
    auto a = must_parse({"macgrind", "--mv-keep", "--mv-verbose", "--leak-check=full",
                         "--track-origins=yes", "-v", "./my_prog.c", "arg1", "--prog-opt", "arg2"});
    CHECK(a.mv_keep);
    CHECK(a.mv_verbose);
    REQUIRE(a.valgrind_flags.size() == 3);
    CHECK(a.valgrind_flags[0] == "--leak-check=full");
    CHECK(a.valgrind_flags[1] == "--track-origins=yes");
    CHECK(a.valgrind_flags[2] == "-v");
    CHECK(a.target == "./my_prog.c");
    REQUIRE(a.program_args.size() == 3);
    CHECK(a.program_args[0] == "arg1");
    CHECK(a.program_args[1] == "--prog-opt");
    CHECK(a.program_args[2] == "arg2");
}

// ── Target forms ──────────────────────────────────────────────────────────────

TEST_CASE("absolute path as target", "[args]") {
    auto a = must_parse({"macgrind", "/home/user/project/prog"});
    CHECK(a.target == "/home/user/project/prog");
    CHECK(a.valgrind_flags.empty());
    CHECK(a.program_args.empty());
}

TEST_CASE("target without dot-slash prefix", "[args]") {
    auto a = must_parse({"macgrind", "prog"});
    CHECK(a.target == "prog");
}

// ── Help / version short-circuit ──────────────────────────────────────────────

TEST_CASE("help flag short-circuits remaining args", "[args]") {
    // Even with flags and a target that wouldn't parse, -h returns cleanly.
    auto a = must_parse({"macgrind", "-h", "--leak-check=full", "./nonexistent"});
    CHECK(a.help);
}

TEST_CASE("version flag short-circuits remaining args", "[args]") {
    auto a = must_parse({"macgrind", "--version", "--leak-check=full", "./prog"});
    CHECK(a.version);
}

// ── double-dash edge cases ────────────────────────────────────────────────────

TEST_CASE("double-dash allows program args that start with dashes", "[args]") {
    auto a = must_parse({"macgrind", "--", "./prog", "--alpha", "-b", "--gamma=3"});
    CHECK(a.valgrind_flags.empty());
    CHECK(a.target == "./prog");
    REQUIRE(a.program_args.size() == 3);
    CHECK(a.program_args[0] == "--alpha");
    CHECK(a.program_args[1] == "-b");
    CHECK(a.program_args[2] == "--gamma=3");
}

// ── Error cases ───────────────────────────────────────────────────────────────

TEST_CASE("no arguments → error", "[args]") {
    auto e = must_fail({"macgrind"});
    CHECK(!e.message.empty());
}

TEST_CASE("flags only, no target → error", "[args]") {
    auto e = must_fail({"macgrind", "--leak-check=full"});
    CHECK(!e.message.empty());
}

TEST_CASE("double-dash with no following target is an error", "[args]") {
    auto e = must_fail({"macgrind", "--"});
    CHECK(!e.message.empty());
}

TEST_CASE("multiple unknown mv flags all reported", "[args]") {
    // Parser stops at the first unknown --mv-* flag; error message names it.
    auto e = must_fail({"macgrind", "--mv-foo", "./prog"});
    CHECK(e.message.find("--mv-foo") != std::string::npos);
}
