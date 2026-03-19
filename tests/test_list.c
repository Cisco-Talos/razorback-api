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

#include <razorback/list.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_destroy_count;

static int *
create_int(int value)
{
    int *item;

    item = malloc(sizeof(*item));
    ck_assert_ptr_ne(item, NULL);
    *item = value;
    return item;
}

static int
int_cmp(void *a, void *b)
{
    const int *lhs = a;
    const int *rhs = b;

    if (*lhs == *rhs)
        return 0;
    return (*lhs < *rhs) ? -1 : 1;
}

static int
int_key_cmp(void *item, const void *key)
{
    return int_cmp(item, (void *)key);
}

static void
int_destroy(void *item)
{
    g_destroy_count++;
    free(item);
}

static void *
int_clone(void *item)
{
    const int *value = item;
    return create_int(*value);
}

static int
sum_ints(void *item, void *userData)
{
    int *sum = userData;
    *sum += *(int *)item;
    return LIST_EACH_OK;
}

START_TEST(test_queue_mode_pop_returns_each_inserted_item_once)
{
    List_t *list;
    int *first;
    int *second;
    int *third;
    int *popped[3];
    bool sawFirst = false;
    bool sawSecond = false;
    bool sawThird = false;
    size_t index;

    list = List_Create(LIST_MODE_QUEUE, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);

    first = create_int(1);
    second = create_int(2);
    third = create_int(3);

    ck_assert(List_Push(list, first));
    ck_assert(List_Push(list, second));
    ck_assert(List_Push(list, third));

    popped[0] = List_Pop(list);
    popped[1] = List_Pop(list);
    popped[2] = List_Pop(list);

    for (index = 0; index < 3; index++) {
        if (popped[index] == first)
            sawFirst = true;
        else if (popped[index] == second)
            sawSecond = true;
        else if (popped[index] == third)
            sawThird = true;
    }

    ck_assert(sawFirst);
    ck_assert(sawSecond);
    ck_assert(sawThird);

    free(first);
    free(second);
    free(third);
    List_Destroy(list);
}
END_TEST

START_TEST(test_stack_mode_pop_preserves_lifo_order)
{
    List_t *list;
    int *first;
    int *second;
    int *third;

    list = List_Create(LIST_MODE_STACK, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);

    first = create_int(1);
    second = create_int(2);
    third = create_int(3);

    ck_assert(List_Push(list, first));
    ck_assert(List_Push(list, second));
    ck_assert(List_Push(list, third));

    ck_assert_ptr_eq(List_Pop(list), third);
    ck_assert_ptr_eq(List_Pop(list), second);
    ck_assert_ptr_eq(List_Pop(list), first);

    free(first);
    free(second);
    free(third);
    List_Destroy(list);
}
END_TEST

START_TEST(test_find_and_remove_use_comparators)
{
    List_t *list;
    int key = 7;
    int *item;

    g_destroy_count = 0;
    list = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);

    item = create_int(7);
    ck_assert(List_Push(list, item));

    ck_assert_ptr_eq(List_Find(list, &key), item);
    ck_assert(List_Remove(list, &key));
    ck_assert_uint_eq(g_destroy_count, 1U);
    ck_assert_uint_eq(List_Length(list), 0U);
    ck_assert_ptr_eq(List_Find(list, &key), NULL);

    List_Destroy(list);
}
END_TEST

START_TEST(test_set_limit_blocks_push_beyond_capacity)
{
    List_t *list;
    int *first;
    int *second;
    int *third;

    g_destroy_count = 0;
    list = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);

    List_SetLimit(list, 2U);

    first = create_int(1);
    second = create_int(2);
    third = create_int(3);

    ck_assert(List_Push(list, first));
    ck_assert(List_Push(list, second));
    ck_assert(!List_Push(list, third));
    ck_assert_uint_eq(List_Length(list), 2U);

    free(third);
    List_Destroy(list);
    ck_assert_uint_eq(g_destroy_count, 2U);
}
END_TEST

START_TEST(test_clone_copies_items_without_aliasing)
{
    List_t *original;
    List_t *clone;
    int key = 11;
    int *item;
    int *originalFound;
    int *clonedFound;

    original = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                           int_clone, NULL, NULL);
    ck_assert_ptr_ne(original, NULL);

    item = create_int(11);
    ck_assert(List_Push(original, item));

    clone = List_Clone(original);
    ck_assert_ptr_ne(clone, NULL);

    originalFound = List_Find(original, &key);
    clonedFound = List_Find(clone, &key);

    ck_assert_ptr_eq(originalFound, item);
    ck_assert_ptr_ne(clonedFound, NULL);
    ck_assert_ptr_ne(clonedFound, originalFound);
    ck_assert_int_eq(*clonedFound, *originalFound);

    *originalFound = 99;
    ck_assert_int_eq(*clonedFound, 11);

    List_Destroy(clone);
    List_Destroy(original);
}
END_TEST

START_TEST(test_transfer_moves_contents_and_clears_destination)
{
    List_t *source;
    List_t *dest;
    int *srcOne;
    int *srcTwo;
    int *destItem;
    int keyOne = 1;
    int keyTwo = 2;
    int keyDest = 99;

    g_destroy_count = 0;
    source = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                         int_clone, NULL, NULL);
    dest = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(source, NULL);
    ck_assert_ptr_ne(dest, NULL);

    srcOne = create_int(1);
    srcTwo = create_int(2);
    destItem = create_int(99);

    ck_assert(List_Push(source, srcOne));
    ck_assert(List_Push(source, srcTwo));
    ck_assert(List_Push(dest, destItem));

    ck_assert(List_Transfer(dest, source));
    ck_assert_uint_eq(g_destroy_count, 1U);
    ck_assert_uint_eq(List_Length(source), 0U);
    ck_assert_uint_eq(List_Length(dest), 2U);
    ck_assert_ptr_eq(List_Find(source, &keyOne), NULL);
    ck_assert_ptr_eq(List_Find(source, &keyTwo), NULL);
    ck_assert_ptr_eq(List_Find(dest, &keyDest), NULL);
    ck_assert_ptr_eq(List_Find(dest, &keyOne), srcOne);
    ck_assert_ptr_eq(List_Find(dest, &keyTwo), srcTwo);

    List_Destroy(dest);
    List_Destroy(source);
    ck_assert_uint_eq(g_destroy_count, 3U);
}
END_TEST

START_TEST(test_generic_mode_empty_pop_returns_null)
{
    List_t *list;

    list = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);
    ck_assert_ptr_eq(List_Pop(list), NULL);

    List_Destroy(list);
}
END_TEST

START_TEST(test_for_each_accumulates_values)
{
    List_t *list;
    int sum = 0;

    list = List_Create(LIST_MODE_GENERIC, int_cmp, int_key_cmp, int_destroy,
                       int_clone, NULL, NULL);
    ck_assert_ptr_ne(list, NULL);

    ck_assert(List_Push(list, create_int(3)));
    ck_assert(List_Push(list, create_int(4)));
    ck_assert(List_Push(list, create_int(5)));

    ck_assert(List_ForEach(list, sum_ints, &sum));
    ck_assert_int_eq(sum, 12);

    List_Destroy(list);
}
END_TEST

static Suite *
list_suite(void)
{
    Suite *suite;
    TCase *testcase;

    suite = suite_create("list");
    testcase = tcase_create("core");

    tcase_add_test(testcase, test_queue_mode_pop_returns_each_inserted_item_once);
    tcase_add_test(testcase, test_stack_mode_pop_preserves_lifo_order);
    tcase_add_test(testcase, test_find_and_remove_use_comparators);
    tcase_add_test(testcase, test_set_limit_blocks_push_beyond_capacity);
    tcase_add_test(testcase, test_clone_copies_items_without_aliasing);
    tcase_add_test(testcase, test_transfer_moves_contents_and_clears_destination);
    tcase_add_test(testcase, test_generic_mode_empty_pop_returns_null);
    tcase_add_test(testcase, test_for_each_accumulates_values);

    suite_add_tcase(suite, testcase);
    return suite;
}

int
main(void)
{
    Suite *suite;
    SRunner *runner;
    int failed;

    suite = list_suite();
    runner = srunner_create(suite);
    srunner_set_fork_status(runner, CK_NOFORK);
    srunner_run_all(runner, CK_NORMAL);
    failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
