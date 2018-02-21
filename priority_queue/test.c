#include "pqueue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MALLOC_COUNT 327680
START_TEST(test_pqueue_malloc) {
    PriorityQueue *pq = pqueue_new(640000000000);
    ck_assert_ptr_eq(pq, NULL);
}
END_TEST

START_TEST(test_pqueue_push) {
    PriorityQueue *pq = pqueue_new(5);
    pqueue_push(pq, 5, 5);
    pqueue_push(pq, 1, 1);
    pqueue_push(pq, 4, 4);
    pqueue_push(pq, 2, 2);
    pqueue_push(pq, 3, 3);
    ck_assert_int_eq(pqueue_push(pq, 0, 0), PQUEUE_OVERFLOW);
    pqueue_delete(pq);
}
END_TEST

START_TEST(test_pqueue_peek) {
    int item = 0;
    PriorityQueue *pq = pqueue_new(10);
    pqueue_push(pq, 5, 5);
    pqueue_peek(pq, &item);
    ck_assert_int_eq(item, 5);
    pqueue_push(pq, 4, 4);
    pqueue_peek(pq, &item);
    ck_assert_int_eq(item, 5);
    pqueue_push(pq, 2, 2);
    pqueue_peek(pq, &item);
    ck_assert_int_eq(item, 5);
    pqueue_push(pq, 666, 666);
    pqueue_peek(pq, &item);
    ck_assert_int_eq(item, 666);
    pqueue_push(pq, 3, 3);
    pqueue_peek(pq, &item);
    ck_assert_int_eq(item, 666);

    pqueue_delete(pq);
}
END_TEST

START_TEST(test_pqueue_pop) {
    int item = 0;
    PriorityQueue *pq = pqueue_new(5);
    pqueue_push(pq, 5, 5);
    pqueue_push(pq, 4, 4);
    pqueue_push(pq, 2, 2);
    pqueue_push(pq, 666, 666);
    pqueue_push(pq, 3, 3);

    pqueue_pop(pq, &item);
    ck_assert_int_eq(item, 666);
    pqueue_pop(pq, &item);
    ck_assert_int_eq(item, 5);
    pqueue_pop(pq, &item);
    ck_assert_int_eq(item, 4);
    pqueue_pop(pq, &item);
    ck_assert_int_eq(item, 3);
    pqueue_pop(pq, &item);
    ck_assert_int_eq(item, 2);
    ck_assert_int_eq(pqueue_pop(pq, &item), PQUEUE_UNDERFLOW);

    pqueue_delete(pq);
}
END_TEST

typedef struct TestCtx {
    int arr[5];
    size_t index;
} TestCtx;

static int action(PriorityQueue* q, T el, void* ctx) {
    TestCtx *tctx = (TestCtx *)ctx;
    tctx->arr[tctx->index] = el;
    tctx->index++;
    return 0;
}

static int action_fail(PriorityQueue* q, T el, void* ctx) {
    ck_abort_msg("shouldn't be here");
    return 0;
}

START_TEST(test_pqueue_foreach) {
    int sample[5] = {5, 4, 3, 2, 1};
    TestCtx ctx = {0};

    PriorityQueue *pq = pqueue_new(5);
    pqueue_push(pq, 5, 5);
    pqueue_push(pq, 4, 4);
    pqueue_push(pq, 2, 2);
    pqueue_push(pq, 3, 3);
    pqueue_push(pq, 1, 1);

    pqueue_foreach(pq, action, &ctx);
    ck_assert_mem_eq(sample, ctx.arr, sizeof(sample));

    pqueue_delete(pq);

    pq = pqueue_new(1);
    pqueue_foreach(pq, action_fail, &ctx);
    pqueue_delete(pq);
}
END_TEST

Suite *pq_suite(void) {
    Suite *s = suite_create("Priority queue");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_pqueue_push);
    tcase_add_test(tc_core, test_pqueue_peek);
    tcase_add_test(tc_core, test_pqueue_pop);
    tcase_add_test(tc_core, test_pqueue_foreach);
    tcase_add_test(tc_core, test_pqueue_malloc);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(int argc, const char *argv[]) {
    Suite *s = pq_suite();
    SRunner *sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
