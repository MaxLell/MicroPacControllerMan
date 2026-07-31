/*
 * assert_probe.h
 *
 * Lets a unit test assert that a precondition ASSERT fires.
 *
 * `custom_assert` dispatches a failed assertion to a handler registered with
 * custom_assert_init(). A test registers this probe's handler, which records the
 * failure and aborts the test body via Unity's TEST_ABORT(), so the call that
 * violated the precondition does not carry on into undefined behaviour.
 *
 * Usage:
 *
 *     void setUp(void)    { assert_probe_begin(); }
 *     void tearDown(void) { assert_probe_end(); }
 *
 *     void test_rejects_null(void)
 *     {
 *         ASSERT_PROBE_EXPECT(some_function(NULL), "in_thing != NULL");
 *     }
 */

#ifndef ASSERT_PROBE_H
#define ASSERT_PROBE_H

#include <stdbool.h>
#include <stdint.h>

/*! \brief Install the probe as the assertion handler and clear its state. */
void assert_probe_begin(void);

/*! \brief Uninstall the probe. */
void assert_probe_end(void);

/*! \brief Number of assertions that have fired since #assert_probe_begin. */
uint32_t assert_probe_get_count(void);

/*! \brief The expression text of the most recent assertion, or `NULL`. */
const char* assert_probe_get_expression(void);

/*! \brief Arm the probe so the next assertion aborts the running test body. */
void assert_probe_arm(void);

/*! \brief Whether an assertion aborted the test body since #assert_probe_arm. */
bool assert_probe_has_aborted(void);

/*! \brief Run `statement` expecting exactly one assertion on `expected_expression`.
 *
 * Fails the test if the statement completes without asserting, if more than one
 * assertion fires, or if the expression text does not match.
 */
#define ASSERT_PROBE_EXPECT(statement, expected_expression)                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        assert_probe_arm();                                                                                            \
                                                                                                                       \
        if (TEST_PROTECT())                                                                                            \
        {                                                                                                              \
            statement;                                                                                                 \
            TEST_FAIL_MESSAGE("expected an assertion, but the statement completed");                                   \
        }                                                                                                              \
                                                                                                                       \
        TEST_ASSERT_TRUE_MESSAGE(assert_probe_has_aborted(), "the test body was not aborted by an assertion");         \
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, assert_probe_get_count(), "expected exactly one assertion");              \
        TEST_ASSERT_EQUAL_STRING(expected_expression, assert_probe_get_expression());                                  \
    } while (0)

#endif /* ASSERT_PROBE_H */
