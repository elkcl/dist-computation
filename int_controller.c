#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/inc/common.h"
#include "lib/inc/controller.h"

constexpr size_t TASK_CNT = 30;
constexpr long double L = 0;
constexpr long double R = 46000;
constexpr long double EPS = 1e-7;

task_t *tasks;

int divide_tasks(controller_t *ctrl) {
    tasks = calloc(TASK_CNT, sizeof(*tasks));
    if (tasks == NULL) {
        fprintf(stderr, "calloc() failed\n");
        return -1;
    }
    long double delta = (R - L) / TASK_CNT;
    for (size_t i = 0; i < TASK_CNT; ++i) {
        tasks[i].data = calloc(3, sizeof(long double));
        if (tasks[i].data == NULL) {
            fprintf(stderr, "divide_tasks: calloc() failed\n");
            return -1;
        }
        tasks[i].size = 3 * sizeof(long double);
        const long double arr[3] = {L + delta * i, L + delta * (i + 1), EPS};
        memcpy(tasks[i].data, arr, tasks[i].size);
        if (controller_add_task(ctrl, &tasks[i]) < 0) {
            fprintf(stderr, "divide_tasks: failed to add task\n");
            return -1;
        }
    }
    return 0;
}

int collect_result(controller_t *ctrl, long double *ans) {
    *ans = 0;
    for (size_t i = 0; i < TASK_CNT; ++i) {
        size_t result_sz;
        uint8_t *result;
        if (controller_get_result(ctrl, tasks[i].id, &result, &result_sz) < 0) {
            fprintf(stderr, "collect_result: failed to get result\n");
            return -1;
        }
        long double term;
        memcpy(&term, result, sizeof(term));
        free(result);
        *ans += term;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Not enough arguments.\nUsage: %s <addr> <port> <num_workers>\n", argv[0]);
    }
    char *end;
    errno = 0;
    long num_workers = strtol(argv[3], &end, 10);
    if (end == argv[3] || *end != '\0' ||
        ((num_workers == LONG_MIN || num_workers == LONG_MAX) && (errno == ERANGE || errno == EINVAL)) ||
        num_workers < (long) INT_MIN || num_workers > (long) INT_MAX) {
        fprintf(stderr, "num_workers is not a valid number\n");
        exit(EXIT_FAILURE);
    }

    controller_t *ctrl = controller_init(argv[1], argv[2], num_workers);
    if (ctrl == NULL) {
        fprintf(stderr, "can't initialize controller\n");
        exit(EXIT_FAILURE);
    }
    if (divide_tasks(ctrl) < 0) {
        fprintf(stderr, "divide_tasks() failed\n");
        controller_destroy(ctrl);
        exit(EXIT_FAILURE);
    }
    if (set_timeout(10000) < 0) {
        fprintf(stderr, "set_timeout() failed\n");
        controller_destroy(ctrl);
        exit(EXIT_FAILURE);
    }
    if (controller_run(ctrl) < 0) {
        fprintf(stderr, "controller_run() failed\n");
        controller_destroy(ctrl);
        exit(EXIT_FAILURE);
    }
    if (controller_wait(ctrl) < 0) {
        fprintf(stderr, "controller_wait() failed\n");
        controller_destroy(ctrl);
        exit(EXIT_FAILURE);
    }
    long double ans;
    if (collect_result(ctrl, &ans) < 0) {
        fprintf(stderr, "collect_result() failed\n");
        controller_destroy(ctrl);
        exit(EXIT_FAILURE);
    }
    printf("result: %Lf\n", ans);
    controller_destroy(ctrl);
}
