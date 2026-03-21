/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include <check.h>

#include <razorback/thread.h>
#include <razorback/timer.h>

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define TIMER_OVERFLOW_THRESHOLD_INTERVAL 4294968U
#define TIMER_TEST_POLL_MS 10U

struct TimerCallbackState {
    _Atomic unsigned int callCount;
    _Atomic unsigned int concurrentCount;
    _Atomic unsigned int maxConcurrentCount;
    _Atomic bool callbackEntered;
    _Atomic bool callbackExited;
    _Atomic bool allowCallbackExit;
    _Atomic bool sawExpectedUserData;
    _Atomic bool selfDestroyReturned;
    struct Timer *selfDestroyTimer;
    unsigned int callbackDelayMs;
};

struct DestroyThreadContext {
    struct Timer *timer;
    _Atomic bool started;
    _Atomic bool completed;
};

static void
test_sleep_ms(unsigned int milliseconds)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = milliseconds / 1000U;
    req.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
        req = rem;
}

static bool
wait_for_condition(_Atomic bool *condition, unsigned int timeoutMs)
{
    unsigned int elapsed = 0U;

    while (elapsed < timeoutMs) {
        if (atomic_load(condition))
            return true;
        test_sleep_ms(TIMER_TEST_POLL_MS);
        elapsed += TIMER_TEST_POLL_MS;
    }

    return atomic_load(condition);
}

static bool
wait_for_call_count_at_least(_Atomic unsigned int *count,
                             unsigned int target,
                             unsigned int timeoutMs)
{
    unsigned int elapsed = 0U;

    while (elapsed < timeoutMs) {
        if (atomic_load(count) >= target)
            return true;
        test_sleep_ms(TIMER_TEST_POLL_MS);
        elapsed += TIMER_TEST_POLL_MS;
    }

    return atomic_load(count) >= target;
}

static bool
wait_for_thread_count(uint32_t target, unsigned int timeoutMs)
{
    unsigned int elapsed = 0U;

    while (elapsed < timeoutMs) {
        if (Thread_getCount() == target)
            return true;
        test_sleep_ms(TIMER_TEST_POLL_MS);
        elapsed += TIMER_TEST_POLL_MS;
    }

    return Thread_getCount() == target;
}

static void
update_max_concurrency(_Atomic unsigned int *maxConcurrent, unsigned int value)
{
    unsigned int current = atomic_load(maxConcurrent);

    while (value > current &&
           !atomic_compare_exchange_weak(maxConcurrent, &current, value)) {
    }
}

static void
timer_test_callback(void *userData)
{
    struct TimerCallbackState *state = userData;
    unsigned int concurrent;

    concurrent = atomic_fetch_add(&state->concurrentCount, 1U) + 1U;
    update_max_concurrency(&state->maxConcurrentCount, concurrent);
    atomic_store(&state->callbackEntered, true);
    atomic_store(&state->sawExpectedUserData, true);
    atomic_fetch_add(&state->callCount, 1U);

    while (!atomic_load(&state->allowCallbackExit))
        test_sleep_ms(TIMER_TEST_POLL_MS);

    if (state->callbackDelayMs > 0U)
        test_sleep_ms(state->callbackDelayMs);

    atomic_fetch_sub(&state->concurrentCount, 1U);
    atomic_store(&state->callbackExited, true);
}

static void
timer_self_destroy_callback(void *userData)
{
    struct TimerCallbackState *state = userData;

    atomic_store(&state->callbackEntered, true);
    atomic_fetch_add(&state->callCount, 1U);
    Timer_Destroy(state->selfDestroyTimer);
    atomic_store(&state->selfDestroyReturned, true);
    atomic_store(&state->callbackExited, true);
}

static void
timer_self_destroy_and_block_callback(void *userData)
{
    struct TimerCallbackState *state = userData;

    atomic_store(&state->callbackEntered, true);
    atomic_fetch_add(&state->callCount, 1U);
    Timer_Destroy(state->selfDestroyTimer);
    atomic_store(&state->selfDestroyReturned, true);

    while (!atomic_load(&state->allowCallbackExit))
        test_sleep_ms(TIMER_TEST_POLL_MS);

    atomic_store(&state->callbackExited, true);
}

static void *
destroy_timer_thread(void *arg)
{
    struct DestroyThreadContext *context = arg;

    atomic_store(&context->started, true);
    Timer_Destroy(context->timer);
    atomic_store(&context->completed, true);
    return NULL;
}

START_TEST(test_timer_create_rejects_zero_interval)
{
    struct TimerCallbackState state = { 0 };

    state.allowCallbackExit = true;
    ck_assert_ptr_eq(Timer_Create(0U, timer_test_callback, &state), NULL);
}
END_TEST

START_TEST(test_timer_callback_fires_and_receives_userdata)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    state.allowCallbackExit = true;
    timer = Timer_Create(1U, timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    ck_assert(wait_for_call_count_at_least(&state.callCount, 1U, 2500U));
    ck_assert(atomic_load(&state.sawExpectedUserData));

    Timer_Destroy(timer);
}
END_TEST

START_TEST(test_timer_destroy_before_first_fire_prevents_callback)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    state.allowCallbackExit = true;
    timer = Timer_Create(1U, timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    Timer_Destroy(timer);
    test_sleep_ms(1300U);
    ck_assert_uint_eq(atomic_load(&state.callCount), 0U);
}
END_TEST

START_TEST(test_timer_large_interval_does_not_fire_early)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    state.allowCallbackExit = true;
    timer = Timer_Create(TIMER_OVERFLOW_THRESHOLD_INTERVAL,
                         timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    test_sleep_ms(1500U);
    ck_assert_uint_eq(atomic_load(&state.callCount), 0U);

    Timer_Destroy(timer);
}
END_TEST

START_TEST(test_timer_repeats_callback)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    state.allowCallbackExit = true;
    timer = Timer_Create(1U, timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    ck_assert(wait_for_call_count_at_least(&state.callCount, 2U, 3500U));

    Timer_Destroy(timer);
}
END_TEST

START_TEST(test_timer_destroy_waits_for_callback_completion)
{
    struct TimerCallbackState state = { 0 };
    struct DestroyThreadContext destroyContext = { 0 };
    struct Timer *timer;
    pthread_t destroyThread;

    state.allowCallbackExit = false;
    timer = Timer_Create(1U, timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    ck_assert(wait_for_condition(&state.callbackEntered, 2500U));

    destroyContext.timer = timer;
    ck_assert_int_eq(pthread_create(&destroyThread, NULL, destroy_timer_thread,
                                    &destroyContext), 0);
    ck_assert(wait_for_condition(&destroyContext.started, 500U));

    test_sleep_ms(200U);
    ck_assert(!atomic_load(&destroyContext.completed));

    atomic_store(&state.allowCallbackExit, true);
    ck_assert_int_eq(pthread_join(destroyThread, NULL), 0);
    ck_assert(atomic_load(&destroyContext.completed));

    test_sleep_ms(300U);
    ck_assert_uint_eq(atomic_load(&state.callCount), 1U);
}
END_TEST

START_TEST(test_timer_callbacks_do_not_overlap)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    state.allowCallbackExit = true;
    state.callbackDelayMs = 1200U;
    timer = Timer_Create(1U, timer_test_callback, &state);
    ck_assert_ptr_ne(timer, NULL);

    ck_assert(wait_for_call_count_at_least(&state.callCount, 2U, 4500U));
    ck_assert_uint_eq(atomic_load(&state.maxConcurrentCount), 1U);

    Timer_Destroy(timer);
}
END_TEST

START_TEST(test_timer_destroy_from_callback_defers_cleanup)
{
    struct TimerCallbackState state = { 0 };
    struct Timer *timer;

    timer = Timer_Create(1U, timer_self_destroy_callback, &state);
    ck_assert_ptr_ne(timer, NULL);
    state.selfDestroyTimer = timer;

    ck_assert(wait_for_condition(&state.callbackEntered, 2500U));
    ck_assert(wait_for_condition(&state.selfDestroyReturned, 500U));
    ck_assert(wait_for_condition(&state.callbackExited, 500U));

    test_sleep_ms(1300U);
    ck_assert_uint_eq(atomic_load(&state.callCount), 1U);
    ck_assert(wait_for_thread_count(0U, 1000U));
}
END_TEST

START_TEST(test_timer_external_destroy_after_self_destroy_is_safe)
{
    struct TimerCallbackState state = { 0 };
    struct DestroyThreadContext destroyContext = { 0 };
    struct Timer *timer;
    pthread_t destroyThread;

    state.allowCallbackExit = false;
    timer = Timer_Create(1U, timer_self_destroy_and_block_callback, &state);
    ck_assert_ptr_ne(timer, NULL);
    state.selfDestroyTimer = timer;

    ck_assert(wait_for_condition(&state.callbackEntered, 2500U));
    ck_assert(wait_for_condition(&state.selfDestroyReturned, 500U));

    destroyContext.timer = timer;
    ck_assert_int_eq(pthread_create(&destroyThread, NULL, destroy_timer_thread,
                                    &destroyContext), 0);
    ck_assert(wait_for_condition(&destroyContext.started, 500U));

    test_sleep_ms(200U);
    ck_assert(!atomic_load(&destroyContext.completed));

    atomic_store(&state.allowCallbackExit, true);
    ck_assert(wait_for_condition(&state.callbackExited, 500U));
    ck_assert_int_eq(pthread_join(destroyThread, NULL), 0);
    ck_assert(atomic_load(&destroyContext.completed));

    test_sleep_ms(1300U);
    ck_assert_uint_eq(atomic_load(&state.callCount), 1U);
}
END_TEST

static Suite *
timer_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("timer");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_timer_create_rejects_zero_interval);
    tcase_add_test(testcase, test_timer_callback_fires_and_receives_userdata);
    tcase_add_test(testcase, test_timer_destroy_before_first_fire_prevents_callback);
    tcase_add_test(testcase, test_timer_large_interval_does_not_fire_early);
    tcase_add_test(testcase, test_timer_repeats_callback);
    tcase_add_test(testcase, test_timer_destroy_waits_for_callback_completion);
    tcase_add_test(testcase, test_timer_callbacks_do_not_overlap);
    tcase_add_test(testcase, test_timer_destroy_from_callback_defers_cleanup);
    tcase_add_test(testcase, test_timer_external_destroy_after_self_destroy_is_safe);
    tcase_set_timeout(testcase, 20);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = timer_suite();
    runner = srunner_create(suite);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);
    return (failed == 0) ? 0 : 1;
}
