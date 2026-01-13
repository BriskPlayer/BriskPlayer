/*
 * BriskPlayer - Blazing fast audio player.
 * Copyright (C) 2000-2001 Niek Albers
 * Copyright (C) 2025 Zach Bacon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CP_TEST_H
#define CP_TEST_H

////////////////////////////////////////////////////////////////////////////////
//
// Simple Unit Test Framework for BriskPlayer
//
// A lightweight, header-only testing framework designed for Windows C code.
// No external dependencies required.
//
// Usage:
//
//   #define CP_TEST_IMPLEMENTATION
//   #include "CP_Test.h"
//
//   void test_example(void)
//   {
//       CP_TEST_ASSERT(1 + 1 == 2);
//       CP_TEST_ASSERT_EQ(42, 42);
//       CP_TEST_ASSERT_STR_EQ("hello", "hello");
//   }
//
//   int main(void)
//   {
//       CP_TEST_BEGIN("Example Tests");
//       CP_TEST_RUN(test_example);
//       return CP_TEST_END();
//   }
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Test Macros
////////////////////////////////////////////////////////////////////////////////

// Begin a test suite
#define CP_TEST_BEGIN(name) \
    cp_test_begin(name)

// End the test suite and return exit code (0 = all passed)
#define CP_TEST_END() \
    cp_test_end()

// Run a test function
#define CP_TEST_RUN(test_func) \
    cp_test_run(#test_func, test_func)

// Basic assertion
#define CP_TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            cp_test_fail(__FILE__, __LINE__, #expr, NULL); \
            return; \
        } \
    } while(0)

// Assert with custom message
#define CP_TEST_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            cp_test_fail(__FILE__, __LINE__, #expr, msg); \
            return; \
        } \
    } while(0)

// Assert equality (integers)
#define CP_TEST_ASSERT_EQ(expected, actual) \
    do { \
        long long _exp = (long long)(expected); \
        long long _act = (long long)(actual); \
        if (_exp != _act) { \
            cp_test_fail_eq(__FILE__, __LINE__, #expected, #actual, _exp, _act); \
            return; \
        } \
    } while(0)

// Assert inequality
#define CP_TEST_ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            cp_test_fail(__FILE__, __LINE__, #a " != " #b, "Values are equal"); \
            return; \
        } \
    } while(0)

// Assert pointer not NULL
#define CP_TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            cp_test_fail(__FILE__, __LINE__, #ptr " != NULL", "Pointer is NULL"); \
            return; \
        } \
    } while(0)

// Assert pointer is NULL
#define CP_TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            cp_test_fail(__FILE__, __LINE__, #ptr " == NULL", "Pointer is not NULL"); \
            return; \
        } \
    } while(0)

// Assert string equality
#define CP_TEST_ASSERT_STR_EQ(expected, actual) \
    do { \
        const char* _exp = (expected); \
        const char* _act = (actual); \
        if (_exp == NULL || _act == NULL || strcmp(_exp, _act) != 0) { \
            cp_test_fail_str(__FILE__, __LINE__, #expected, #actual, _exp, _act); \
            return; \
        } \
    } while(0)

// Assert within epsilon (floating point)
#define CP_TEST_ASSERT_FLOAT_EQ(expected, actual, epsilon) \
    do { \
        double _exp = (double)(expected); \
        double _act = (double)(actual); \
        double _eps = (double)(epsilon); \
        double _diff = _exp > _act ? _exp - _act : _act - _exp; \
        if (_diff > _eps) { \
            cp_test_fail_float(__FILE__, __LINE__, #expected, #actual, _exp, _act, _eps); \
            return; \
        } \
    } while(0)

// Skip the current test
#define CP_TEST_SKIP(reason) \
    do { \
        cp_test_skip(reason); \
        return; \
    } while(0)

////////////////////////////////////////////////////////////////////////////////
// Test State (Internal)
////////////////////////////////////////////////////////////////////////////////

typedef void (*cp_test_func)(void);

typedef struct {
    const char* suite_name;
    int tests_run;
    int tests_passed;
    int tests_failed;
    int tests_skipped;
    int current_failed;
    const char* current_test;
} cp_test_state;

// Global test state
static cp_test_state g_test_state = {0};

////////////////////////////////////////////////////////////////////////////////
// Test Functions (Internal)
////////////////////////////////////////////////////////////////////////////////

static void cp_test_begin(const char* name)
{
    g_test_state.suite_name = name;
    g_test_state.tests_run = 0;
    g_test_state.tests_passed = 0;
    g_test_state.tests_failed = 0;
    g_test_state.tests_skipped = 0;
    
    printf("\n");
    printf("========================================\n");
    printf(" %s\n", name);
    printf("========================================\n\n");
    
#ifdef _WIN32
    // Enable ANSI colors on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
    }
#endif
}

static void cp_test_run(const char* name, cp_test_func func)
{
    g_test_state.tests_run++;
    g_test_state.current_failed = 0;
    g_test_state.current_test = name;
    
    printf("  [    ] %s", name);
    fflush(stdout);
    
    func();
    
    if (g_test_state.current_failed) {
        g_test_state.tests_failed++;
        printf("\r  [\033[31mFAIL\033[0m] %s\n", name);
    } else {
        g_test_state.tests_passed++;
        printf("\r  [\033[32mPASS\033[0m] %s\n", name);
    }
}

static int cp_test_end(void)
{
    printf("\n");
    printf("----------------------------------------\n");
    printf(" Results: %d passed, %d failed, %d skipped (of %d)\n",
           g_test_state.tests_passed,
           g_test_state.tests_failed,
           g_test_state.tests_skipped,
           g_test_state.tests_run);
    printf("----------------------------------------\n\n");
    
    return g_test_state.tests_failed > 0 ? 1 : 0;
}

static void cp_test_fail(const char* file, int line, const char* expr, const char* msg)
{
    g_test_state.current_failed = 1;
    printf("\n    FAILED at %s:%d\n", file, line);
    printf("    Assertion: %s\n", expr);
    if (msg) {
        printf("    Message: %s\n", msg);
    }
}

static void cp_test_fail_eq(const char* file, int line, 
                            const char* exp_name, const char* act_name,
                            long long expected, long long actual)
{
    g_test_state.current_failed = 1;
    printf("\n    FAILED at %s:%d\n", file, line);
    printf("    Expected %s == %s\n", exp_name, act_name);
    printf("    Expected: %lld\n", expected);
    printf("    Actual:   %lld\n", actual);
}

static void cp_test_fail_str(const char* file, int line,
                             const char* exp_name, const char* act_name,
                             const char* expected, const char* actual)
{
    g_test_state.current_failed = 1;
    printf("\n    FAILED at %s:%d\n", file, line);
    printf("    Expected %s == %s\n", exp_name, act_name);
    printf("    Expected: \"%s\"\n", expected ? expected : "(null)");
    printf("    Actual:   \"%s\"\n", actual ? actual : "(null)");
}

static void cp_test_fail_float(const char* file, int line,
                               const char* exp_name, const char* act_name,
                               double expected, double actual, double epsilon)
{
    g_test_state.current_failed = 1;
    printf("\n    FAILED at %s:%d\n", file, line);
    printf("    Expected %s ~= %s (epsilon: %g)\n", exp_name, act_name, epsilon);
    printf("    Expected: %g\n", expected);
    printf("    Actual:   %g\n", actual);
}

static void cp_test_skip(const char* reason)
{
    g_test_state.tests_skipped++;
    g_test_state.tests_run--;  // Don't count as run
    printf("\r  [\033[33mSKIP\033[0m] %s - %s\n", g_test_state.current_test, reason);
}

#ifdef __cplusplus
}
#endif

#endif // CP_TEST_H
