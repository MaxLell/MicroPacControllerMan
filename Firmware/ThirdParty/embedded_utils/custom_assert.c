/**
 * MIT License
 *
 * Copyright (c) <2025> <Max Koell (maxkoell@proton.me)>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * \file            custom_assert.c
 * \brief           Custom assertion implementation for embedded systems
 */

#include "custom_assert.h"
#include <stddef.h>

#ifndef NDEBUG
/* Private variables - only needed in debug mode */
static custom_assert_callback_fn registered_assert_handler = NULL;

/**
 * \brief           Default assertion handler (fallback)
 * \param[in]       file: Source file where assertion failed
 * \param[in]       line: Line number where assertion failed
 * \param[in]       expr: Expression that failed
 */
static void default_assert_handler(const char* file, uint32_t line, const char* expr)
{
    (void)file;
    (void)line;
    (void)expr;

    /* Default behavior: halt execution */
    while (1)
    {
        /* Infinite loop to stop execution */
        /* You can set a breakpoint here for debugging */
    }
}

/**
 * \brief           Handle assertion failure
 * \param[in]       file: Source file where assertion failed
 * \param[in]       line: Line number where assertion failed
 * \param[in]       expr: Expression that failed
 */
void custom_assert_failed(const char* file, uint32_t line, const char* expr)
{
    if (registered_assert_handler != NULL)
    {
        registered_assert_handler(file, line, expr);
    }
    else
    {
        default_assert_handler(file, line, expr);
    }
}

/**
 * \brief           Set custom assertion callback
 * \param[in]       callback: Callback function to handle assertions
 */
void custom_assert_init(custom_assert_callback_fn callback) { registered_assert_handler = callback; }

/**
 * \brief           Reset assertion callback to default
 */
void custom_assert_deinit(void) { registered_assert_handler = NULL; }

#else /* NDEBUG */

/* NDEBUG mode - provide empty stub implementations */

/**
 * \brief           Handle assertion failure (stub for release mode)
 * \param[in]       file: Source file where assertion failed (unused)
 * \param[in]       line: Line number where assertion failed (unused)
 * \param[in]       expr: Expression that failed (unused)
 */
void custom_assert_failed(const char* file, uint32_t line, const char* expr)
{
    (void)file;
    (void)line;
    (void)expr;
    /* Empty stub - do nothing in release mode */
}

/**
 * \brief           Set custom assertion callback (stub for release mode)
 * \param[in]       callback: Callback function to handle assertions (unused)
 */
void custom_assert_init(custom_assert_callback_fn callback)
{
    (void)callback;
    /* Empty stub - do nothing in release mode */
}

/**
 * \brief           Reset assertion callback to default (stub for release mode)
 */
void custom_assert_deinit(void) { /* Empty stub - do nothing in release mode */ }

#endif /* NDEBUG */