cmake_minimum_required(VERSION 4.2)
project(c_compiler C)

set(CMAKE_C_STANDARD 11)
# -std=c11 instead of -std=gnu11: ISO C only, so -pedantic (below) can flag GNU extensions. POSIX functions need a
# feature-test macro in the file that uses them (e.g. _POSIX_C_SOURCE in tests/test_source_text.c).
set(CMAKE_C_EXTENSIONS OFF)

# Lets CTest run the test executables registered below with add_test.
enable_testing()
# Only a failed test prints its output. This applies to the `test` target (cmake --build <dir> --target test); a plain
# `ctest` or CLion's runner needs the flag / the CTEST_OUTPUT_ON_FAILURE=1 environment variable instead.
set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")

# Warning flags, shared by every target that links this. Added one or two at a time, fixing each one's hits first.
#   -Wcast-qual:            a cast that drops const (conventions.md, "deep const").
#   -Wshadow:               a local variable or parameter that hides an outer one.
#   -Wmissing-declarations: a non-static function defined with no earlier declaration (a missing `static`, or a .c file
#                           that doesn't include its own header).
#   -Wwrite-strings:        string literals get the type const char[], so `char* s = "x"` warns (deep const again).
#   -Wformat=2:             stricter printf checking, e.g. a format string that isn't a literal.
#   -Wundef:                `#if FOO` where FOO isn't defined (it silently counts as 0).
#   -Wredundant-decls:      the same declaration twice in one scope.
#   -Wswitch-enum:          an enum value with no `case`, even when the switch has a `default` (plain -Wswitch, in
#                           -Wall, stays silent then). The __COUNT enumerators need a `case` too.
#   -pedantic:              everything ISO C11 forbids or requires a diagnostic for, mostly GNU extensions.
# Errors, because they're always bugs (-Werror=X also turns X on):
#   return-type, implicit-function-declaration, incompatible-pointer-types, int-conversion, format.
# Not used, by choice: -Wstrict-prototypes, -Wold-style-definition and -Wmissing-prototypes (empty parameter lists `f()`
# are allowed, and in C11 `f();` isn't a prototype, so all three would flag them).
add_library(c_compiler_warnings INTERFACE)
target_compile_options(c_compiler_warnings INTERFACE
        -Wall -Wextra -Wcast-qual -Wshadow -Wmissing-declarations -Wwrite-strings -Wformat=2 -Wundef -Wredundant-decls
        -Wswitch-enum -pedantic
        -Werror=return-type -Werror=implicit-function-declaration -Werror=incompatible-pointer-types
        -Werror=int-conversion -Werror=format)

# Instrumented builds (testing-roadmap.md, steps 6 and 9). The sanitizers are on by default in Debug builds, so every
# test run checks memory and undefined behavior; -DENABLE_SANITIZERS=OFF turns them off (Release never has them unless
# asked). Coverage is off by default.
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(sanitizers_default ON)
else ()
    set(sanitizers_default OFF)
endif ()
option(ENABLE_SANITIZERS "Build with AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer" ${sanitizers_default})
option(ENABLE_COVERAGE "Build unoptimized with gcov coverage counters (see tests/coverage.sh)" OFF)

# Compile and link flags for the options above, shared like the warnings. Linked PUBLIC by the library so that every
# executable linking it gets the link flags too.
add_library(c_compiler_instrumentation INTERFACE)
if (ENABLE_SANITIZERS)
    # no-sanitize-recover: a UBSan finding stops the program (and fails the test) instead of printing and carrying on.
    target_compile_options(c_compiler_instrumentation INTERFACE
            -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(c_compiler_instrumentation INTERFACE -fsanitize=address,undefined)
endif ()
if (ENABLE_COVERAGE)
    # -O0: optimization merges and removes lines and branches, which makes the counts hard to read.
    target_compile_options(c_compiler_instrumentation INTERFACE --coverage -O0 -g)
    target_link_options(c_compiler_instrumentation INTERFACE --coverage)
endif ()

# Everything except src/main.c, so test executables can link it with their own main.
add_library(c_compiler_core STATIC
        src/lexer/Lexer.c
        src/lexer/Lexer.h
        src/lexer/Token.c
        src/lexer/Token.h
        src/common/Diagnostic.c
        src/common/Diagnostic.h
        src/common/Utils.c
        src/common/Utils.h
        src/common/Context.c
        src/common/Context.h
        src/driver/Driver.c
        src/driver/Driver.h
        src/data_structures/List.c
        src/data_structures/List.h
        src/data_structures/FilePosition.c
        src/data_structures/FilePosition.h
        src/data_structures/SourceText.c
        src/data_structures/SourceText.h
        src/data_structures/Range.c
        src/data_structures/Range.h
        src/data_structures/Text.c
        src/data_structures/Text.h
        src/data_structures/TextView.c
        src/data_structures/TextView.h
        src/parser/Expression.h
        src/parser/Statement.h
        src/parser/Declaration.h
        src/parser/Parser.c
        src/parser/Parser.h
        src/parser/ExternalDefinition.h
        src/data_structures/Arena.c
        src/data_structures/Arena.h
        src/parser/Type.c
        src/parser/Type.h
        src/sema/Symbol.c
        src/sema/Symbol.h
        src/sema/ScopeStack.c
        src/sema/ScopeStack.h
        src/sema/Sema.c
        src/sema/Sema.h
        src/codegen/Codegen.c
        src/codegen/Codegen.h
        src/codegen/Emitter.c
        src/codegen/Emitter.h
        src/codegen/Operand.c
        src/codegen/Operand.h
        src/data_structures/Printer.h
        src/data_structures/Printer.c
)
# -iquote (not -I, which target_include_directories would give): the project's headers are found by #include "x.h"
# only, so a header whose name matches a system header (the case-insensitive /mnt/c: String.h vs <string.h>) can
# never hide the system one from #include <x.h>. Propagates to every target that links the library.
# One directory per stage, so a header is included by its bare name ("Lexer.h") from anywhere, tests included.
target_compile_options(c_compiler_core PUBLIC
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/common"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/driver"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/data_structures"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/parser"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/sema"
        "SHELL:-iquote ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen")
target_link_libraries(c_compiler_core PRIVATE c_compiler_warnings PUBLIC c_compiler_instrumentation)

add_executable(c_compiler src/main.c)
target_link_libraries(c_compiler PRIVATE c_compiler_core c_compiler_warnings)

# -------- Tests -------- #

# The hand-rolled test harness (CHECK, RUN_TEST), shared by every test executable.
add_library(test_harness STATIC
        tests/Harness.c
        tests/Harness.h)
target_link_libraries(test_harness PRIVATE c_compiler_warnings c_compiler_instrumentation)

# The AST dump (docs/specifications/ast-dump-format.md), for tests that compare an AST against a string. A test tool,
# not part of the compiler, so it lives here and not in c_compiler_core.
add_library(test_ast_serializer STATIC
        tests/AstSerializer.c
        tests/AstSerializer.h)
target_link_libraries(test_ast_serializer PRIVATE c_compiler_core c_compiler_warnings)

add_executable(test_list tests/test_list.c)
target_link_libraries(test_list PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME list COMMAND test_list)

add_executable(test_text_view tests/test_text_view.c)
target_link_libraries(test_text_view PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME text_view COMMAND test_text_view)

add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME lexer COMMAND test_lexer)

add_executable(test_source_text tests/test_source_text.c)
target_link_libraries(test_source_text PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME source_text COMMAND test_source_text)

add_executable(test_arena tests/test_arena.c)
target_link_libraries(test_arena PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME arena COMMAND test_arena)

add_executable(test_parser tests/test_parser.c)
target_link_libraries(test_parser PRIVATE c_compiler_core test_ast_serializer test_harness c_compiler_warnings)
add_test(NAME parser COMMAND test_parser)
# A parser loop that never consumes a token hangs instead of failing; CTest's default timeout is 1500 seconds.
set_tests_properties(parser PROPERTIES TIMEOUT 10)

add_executable(test_type tests/test_type.c)
target_link_libraries(test_type PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME type COMMAND test_type)

add_executable(test_scope_stack tests/test_scope_stack.c)
target_link_libraries(test_scope_stack PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME scope_stack COMMAND test_scope_stack)

add_executable(test_sema tests/test_sema.c)
target_link_libraries(test_sema PRIVATE c_compiler_core test_ast_serializer test_harness c_compiler_warnings)
add_test(NAME sema COMMAND test_sema)

add_executable(test_operand tests/test_operand.c)
target_link_libraries(test_operand PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME operand COMMAND test_operand)

add_executable(test_emitter tests/test_emitter.c)
target_link_libraries(test_emitter PRIVATE c_compiler_core test_harness c_compiler_warnings)
add_test(NAME emitter COMMAND test_emitter)

if (ENABLE_SANITIZERS)
    # ASan and LeakSanitizer already print a stack trace; UBSan only does on request.
    set_tests_properties(list text_view lexer source_text arena parser type scope_stack sema operand emitter PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=print_stacktrace=1")
endif ()

# End-to-end tests (codegen-roadmap.md, Step 0d): one CTest test per tests/e2e/<slice>/<name>.c, named
# e2e/<slice>/<name>. tests/run_e2e.sh compiles, assembles, links and runs it, and checks the exit status against
# <name>.exit next to it.
# CONFIGURE_DEPENDS: a new program is picked up on the next build, without re-running CMake by hand. Run only
# these with: ctest -L e2e. Needs a POSIX shell and gcc.
# e2e_pending_slices: slices whose tests are written ahead of the compiler. They aren't registered at all, so no test
# runner (CTest or CLion's) can count them as failed. Not the DISABLED property: CLion reports a disabled test as a
# failure. Remove a slice from the list once it's implemented.
set(e2e_pending_slices slice04)
if (UNIX)
    file(GLOB_RECURSE e2e_programs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/*.c)
    foreach (program IN LISTS e2e_programs)
        file(RELATIVE_PATH relative_path ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e ${program}) # slice01/return_42.c
        string(REGEX REPLACE "\\.c$" "" test_name ${relative_path})                         # slice01/return_42
        get_filename_component(test_directory ${test_name} DIRECTORY)                       # slice01
        if (test_directory IN_LIST e2e_pending_slices)
            continue()
        endif ()
        add_test(NAME e2e/${test_name}
                COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_e2e.sh $<TARGET_FILE:c_compiler> ${program}
                        ${CMAKE_CURRENT_BINARY_DIR}/e2e/${test_directory})
        # The script's own timeout (5 s) covers the program; this one covers a compiler that hangs.
        set_tests_properties(e2e/${test_name} PROPERTIES TIMEOUT 30 LABELS e2e)
        if (ENABLE_SANITIZERS)
            set_tests_properties(e2e/${test_name} PROPERTIES ENVIRONMENT "UBSAN_OPTIONS=print_stacktrace=1")
        endif ()
    endforeach ()
endif ()

# Coverage without blocking anything. tests/coverage.sh makes its own instrumented build (in a subdirectory of this
# build tree, so this build stays as it is), runs the tests in it and reports the lines and branches no test reached.
# Neither is part of the normal build or of "All CTest", so the commit check stays fast and can't fail on coverage.
# Not defined in the instrumented build itself (ENABLE_COVERAGE), where they would start themselves again. Needs a
# POSIX shell.
if (UNIX AND NOT ENABLE_COVERAGE)
    # The report, on demand (report only, never fails): cmake --build <build dir> --target coverage
    add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E env BUILD_DIR=${CMAKE_BINARY_DIR}/coverage-build
            sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/coverage.sh src/lexer/Lexer.c
            USES_TERMINAL VERBATIM)

    # The gate, for CI: the CTest test `coverage` fails if a line of Lexer.c never ran. Off by default, because it is
    # slow (a second build) and would turn "I haven't written the test yet" into a blocked commit.
    option(ENABLE_COVERAGE_CHECK "Register the CTest test 'coverage' (fails if a line of Lexer.c never ran)" OFF)
    if (ENABLE_COVERAGE_CHECK)
        add_test(NAME coverage
                COMMAND ${CMAKE_COMMAND} -E env BUILD_DIR=${CMAKE_BINARY_DIR}/coverage-build
                sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/coverage.sh --fail-on-uncovered-lines src/lexer/Lexer.c)
        set_tests_properties(coverage PROPERTIES TIMEOUT 300)
    endif ()
endif ()

# -------- Forced full rebuild -------- #

# Recompiles every file on every build, so every file's warnings show each time (otherwise only the recompiled files
# show theirs). Every object file depends on a stamp file that the force_rebuild target touches before anything is
# compiled. The sources themselves are never touched. Turn it off with -DFORCE_FULL_REBUILD=OFF, or delete this section.
# Must stay at the end of this file: it applies to the targets defined above it.
option(FORCE_FULL_REBUILD "Recompile every file on every build, so every file's warnings show" ON)
if (FORCE_FULL_REBUILD)
    set(force_rebuild_stamp ${CMAKE_BINARY_DIR}/force_rebuild.stamp)
    add_custom_target(force_rebuild
            COMMAND ${CMAKE_COMMAND} -E touch ${force_rebuild_stamp}
            BYPRODUCTS ${force_rebuild_stamp}
            COMMENT "FORCE_FULL_REBUILD: recompiling every file")

    get_directory_property(all_targets BUILDSYSTEM_TARGETS)
    foreach (target IN LISTS all_targets)
        get_target_property(target_type ${target} TYPE)
        # Only targets that compile something: not INTERFACE libraries (the flag groups) or custom targets (coverage).
        if (target_type MATCHES "^(STATIC_LIBRARY|EXECUTABLE)$")
            add_dependencies(${target} force_rebuild)
            get_target_property(target_sources ${target} SOURCES)
            set_property(SOURCE ${target_sources} APPEND PROPERTY OBJECT_DEPENDS ${force_rebuild_stamp})
        endif ()
    endforeach ()
endif ()
