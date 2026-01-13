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

////////////////////////////////////////////////////////////////////////////////
//
// BriskPlayer Unit Tests
//
// Build: cl /nologo tests.c /Fe:tests.exe
// Run:   tests.exe
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the test framework
#include "CP_Test.h"

// Include modules to test
#include "CP_Constants.h"
#include "CP_Result.h"
#include "CP_Cleanup.h"
#include "CP_Unicode.h"
#include "safe_string.h"

////////////////////////////////////////////////////////////////////////////////
// CP_Result Tests
////////////////////////////////////////////////////////////////////////////////

void test_result_success(void)
{
    CP_TEST_ASSERT(CP_SUCCEEDED(CP_OK));
    CP_TEST_ASSERT(CP_SUCCEEDED(CP_WARN_ALREADY_DONE));
    CP_TEST_ASSERT(!CP_FAILED(CP_OK));
}

void test_result_failure(void)
{
    CP_TEST_ASSERT(CP_FAILED(CP_ERROR_UNKNOWN));
    CP_TEST_ASSERT(CP_FAILED(CP_ERROR_OUT_OF_MEMORY));
    CP_TEST_ASSERT(!CP_SUCCEEDED(CP_ERROR_FILE_NOT_FOUND));
}

void test_result_to_string(void)
{
    const char* str = CP_ResultToString(CP_OK);
    CP_TEST_ASSERT_NOT_NULL(str);
    CP_TEST_ASSERT_STR_EQ("CP_OK", str);
    
    str = CP_ResultToString(CP_ERROR_OUT_OF_MEMORY);
    CP_TEST_ASSERT_STR_EQ("CP_ERROR_OUT_OF_MEMORY", str);
}

void test_result_from_win32(void)
{
    CP_Result result = CP_ResultFromWin32(ERROR_SUCCESS);
    CP_TEST_ASSERT_EQ(CP_OK, result);
    
    result = CP_ResultFromWin32(ERROR_FILE_NOT_FOUND);
    CP_TEST_ASSERT_EQ(CP_ERROR_FILE_NOT_FOUND, result);
    
    result = CP_ResultFromWin32(ERROR_ACCESS_DENIED);
    CP_TEST_ASSERT_EQ(CP_ERROR_ACCESS_DENIED, result);
}

////////////////////////////////////////////////////////////////////////////////
// CP_Cleanup Tests
////////////////////////////////////////////////////////////////////////////////

void test_safe_free(void)
{
    char* ptr = (char*)malloc(100);
    CP_TEST_ASSERT_NOT_NULL(ptr);
    
    SAFE_FREE(ptr);
    CP_TEST_ASSERT_NULL(ptr);
    
    // Should be safe to call again
    SAFE_FREE(ptr);
    CP_TEST_ASSERT_NULL(ptr);
}

void test_safe_close_handle(void)
{
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    CP_TEST_ASSERT_NOT_NULL(hEvent);
    CP_TEST_ASSERT(hEvent != INVALID_HANDLE_VALUE);
    
    SAFE_CLOSE_HANDLE(hEvent);
    CP_TEST_ASSERT_NULL(hEvent);
    
    // Should be safe to call again
    SAFE_CLOSE_HANDLE(hEvent);
}

////////////////////////////////////////////////////////////////////////////////
// CP_Unicode Tests
////////////////////////////////////////////////////////////////////////////////

void test_utf8_to_wide(void)
{
    wchar_t buffer[256];
    int result = CPU_Utf8ToWide("Hello", buffer, 256);
    CP_TEST_ASSERT(result > 0);
    CP_TEST_ASSERT(wcscmp(buffer, L"Hello") == 0);
}

void test_wide_to_utf8(void)
{
    char buffer[256];
    int result = CPU_WideToUtf8(L"World", buffer, 256);
    CP_TEST_ASSERT(result > 0);
    CP_TEST_ASSERT_STR_EQ("World", buffer);
}

void test_utf8_roundtrip(void)
{
    const char* original = "Test String 123";
    wchar_t wide[256];
    char back[256];
    
    CPU_Utf8ToWide(original, wide, 256);
    CPU_WideToUtf8(wide, back, 256);
    
    CP_TEST_ASSERT_STR_EQ(original, back);
}

void test_unicode_null_handling(void)
{
    wchar_t buffer[256];
    
    // NULL input should return 0
    int result = CPU_Utf8ToWide(NULL, buffer, 256);
    CP_TEST_ASSERT_EQ(0, result);
    
    // NULL output should return 0
    result = CPU_Utf8ToWide("test", NULL, 256);
    CP_TEST_ASSERT_EQ(0, result);
    
    // Zero buffer size should return 0
    result = CPU_Utf8ToWide("test", buffer, 0);
    CP_TEST_ASSERT_EQ(0, result);
}

////////////////////////////////////////////////////////////////////////////////
// Safe String Tests
////////////////////////////////////////////////////////////////////////////////

void test_cp_strcpy_s(void)
{
    char buffer[10];
    
    int result = cp_strcpy_s(buffer, sizeof(buffer), "Hello");
    CP_TEST_ASSERT_EQ(0, result);
    CP_TEST_ASSERT_STR_EQ("Hello", buffer);
}

void test_cp_strcpy_s_truncation(void)
{
    char buffer[5];
    
    // This should truncate
    int result = cp_strcpy_s(buffer, sizeof(buffer), "Hello World");
    
    // Result depends on implementation - just check buffer is valid
    CP_TEST_ASSERT(strlen(buffer) < sizeof(buffer));
    CP_TEST_ASSERT(buffer[sizeof(buffer) - 1] == '\0');
}

void test_cp_strcat_s(void)
{
    char buffer[20] = "Hello";
    
    int result = cp_strcat_s(buffer, sizeof(buffer), " World");
    CP_TEST_ASSERT_EQ(0, result);
    CP_TEST_ASSERT_STR_EQ("Hello World", buffer);
}

void test_cp_snprintf(void)
{
    char buffer[32];
    
    int result = cp_snprintf(buffer, sizeof(buffer), "Value: %d", 42);
    CP_TEST_ASSERT(result > 0);
    CP_TEST_ASSERT_STR_EQ("Value: 42", buffer);
}

////////////////////////////////////////////////////////////////////////////////
// Constants Tests
////////////////////////////////////////////////////////////////////////////////

void test_constants_defined(void)
{
    // Timer IDs should be unique
    CP_TEST_ASSERT_NE(CPC_TIMER_SCROLL, CPC_TIMER_ANIMATION);
    CP_TEST_ASSERT_NE(CPC_TIMER_ANIMATION, CPC_TIMER_TOOLTIP);
    
    // Buffer sizes should be reasonable
    CP_TEST_ASSERT(CPC_PATH_BUFFER >= 260);
    CP_TEST_ASSERT(CPC_TITLE_BUFFER >= 256);
    
    // Volume range
    CP_TEST_ASSERT(CPC_VOLUME_MAX > CPC_VOLUME_MIN);
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

int main(void)
{
    CP_TEST_BEGIN("BriskPlayer Unit Tests");
    
    // CP_Result tests
    CP_TEST_RUN(test_result_success);
    CP_TEST_RUN(test_result_failure);
    CP_TEST_RUN(test_result_to_string);
    CP_TEST_RUN(test_result_from_win32);
    
    // CP_Cleanup tests
    CP_TEST_RUN(test_safe_free);
    CP_TEST_RUN(test_safe_close_handle);
    
    // CP_Unicode tests
    CP_TEST_RUN(test_utf8_to_wide);
    CP_TEST_RUN(test_wide_to_utf8);
    CP_TEST_RUN(test_utf8_roundtrip);
    CP_TEST_RUN(test_unicode_null_handling);
    
    // Safe string tests
    CP_TEST_RUN(test_cp_strcpy_s);
    CP_TEST_RUN(test_cp_strcpy_s_truncation);
    CP_TEST_RUN(test_cp_strcat_s);
    CP_TEST_RUN(test_cp_snprintf);
    
    // Constants tests
    CP_TEST_RUN(test_constants_defined);
    
    return CP_TEST_END();
}
