#!/bin/sh
# Written by Claude on 20-Oct-26.
#
# End-to-end test of one Cminor program (codegen-roadmap.md, Step 0d): compiles it with the Cminor compiler, assembles and
# links the .s with gcc, runs the program, and compares its exit status with the one in the program's .exit file.
#
# Usage: tests/run_e2e.sh <compiler> <program.c> <work-dir>
#          compiler:  the c_compiler executable
#          program.c: the test program. Next to it, program.exit holds the expected exit status N (0 <= N <= 255):
#                     the first line that is neither empty nor a "#" comment. The expectation lives outside the
#                     program, so the program needs nothing the compiler might not support yet (such as comments).
#          work-dir:  where the .s and the executable go (kept after the run, for debugging)
# Exit:  0 if the program ran and exited with N; 1 otherwise, with the reason printed.
#
# CTest runs it once per tests/e2e/**/*.c (CMakeLists.txt). Needs gcc and timeout (coreutils). Run it where the tests
# run (the WSL/Ubuntu side).

set -eu

[ "$#" -eq 3 ] || { echo "usage: $0 <compiler> <program.c> <work-dir>" >&2; exit 2; }
compiler=$1
program=$2
work_dir=$3
timeout_seconds=5

name=$(basename "$program" .c)
assembly="$work_dir/$name.s"
executable="$work_dir/$name"
mkdir -p "$work_dir"
rm -f "$assembly" "$executable"   # a file left by an earlier run must not pass for this one

fail() {
    echo "FAIL $program: $*" >&2
    exit 1
}

# 1. The expectation. tr: a file checked out with CRLF line endings still reads the same.
expectation_file="${program%.c}.exit"
[ -f "$expectation_file" ] || fail "no $expectation_file, so there is nothing to compare with"
expected=$(tr -d '\r' < "$expectation_file" | grep -v -e '^#' -e '^[[:space:]]*$' | head -n 1 | tr -d ' \t')
case "$expected" in
    '' | *[!0-9]*) fail "$expectation_file must hold the expected exit status, a number 0..255 (got \"$expected\")" ;;
esac
[ "$expected" -le 255 ] || fail "$expectation_file: $expected, but an exit status is 0..255 (the returned value mod 256)"

# 2. Compile. In the sanitizer build, an ASan or LeakSanitizer report makes the compiler exit non-zero.
status=0
"$compiler" "$program" "$assembly" || status=$?
[ "$status" -eq 0 ] || fail "the compiler exited with status $status"
[ -s "$assembly" ] || fail "the compiler exited with status 0 but wrote no $assembly"

# 3. Assemble and link. gcc adds the C runtime, which calls main and passes its return value to exit.
if ! gcc -o "$executable" "$assembly"; then
    echo "---- $assembly ----" >&2
    cat -n "$assembly" >&2
    fail "gcc could not assemble or link $assembly (its errors are above)"
fi

# 4. Run.
status=0
timeout "$timeout_seconds" "$executable" || status=$?

# 5. Compare. The shell spells "killed by signal S" as exit status 128 + S, and timeout reports a timeout as 124, so a
# program could also exit with those values itself. A match therefore passes first; only a mismatch is described.
if [ "$status" -eq "$expected" ]; then
    echo "PASS $program (exit $status)"
    exit 0
fi
if [ "$status" -eq 124 ]; then
    fail "no exit within ${timeout_seconds}s (an infinite loop?), expected exit $expected; see $assembly"
fi
if [ "$status" -gt 128 ]; then
    signal=$(kill -l "$((status - 128))" 2>/dev/null || echo "?")
    fail "killed by SIG$signal (or exit status $status), expected exit $expected; see $assembly"
fi
fail "exit status $status, expected $expected; see $assembly"
