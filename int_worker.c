#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lib/inc/common.h"
#include "lib/inc/worker.h"

long double func(long double x) {
    return 0.1 * x * sinl(sqrtl(x) / 20);
}

long double integrate(long double (*f)(long double), long double a, long double b, long double eps) {
    int n = 2;
    long double h = (b - a) / n;
    long double prev = 0, sum = 0;
    do {
        prev = sum;
        sum = f(a) + f(b);
        for (int i = 1; i <= n / 2 - 1; ++i) {
            sum += 2 * f(a + h * (2 * i));
        }
        for (int i = 1; i <= n / 2; ++i) {
            sum += 4 * f(a + h * (2 * i - 1));
        }
        sum *= h / 3;
        n *= 2;
        h = (b - a) / n;
    } while (fabsl(sum - prev) > 15.0L * eps);
    return sum;
}

int worker_int_proc(const uint8_t *data, size_t size, uint8_t **result, size_t *result_sz) {
    (void) size;
    long double arr[3];
    memcpy(arr, data, sizeof(arr));
    usleep(5000);
    long double ans = integrate(func, arr[0], arr[1], arr[2]);
    *result = calloc(1, sizeof(long double));
    if (*result == NULL) {
        fprintf(stderr, "worker_int_proc: calloc() failed\n");
        return -1;
    }
    *result_sz = sizeof(long double);
    memcpy(*result, &ans, *result_sz);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Not enough arguments.\nUsage: %s <addr> <port> <num_proc>\n", argv[0]);
    }
    char *end;
    errno = 0;
    long num_proc = strtol(argv[3], &end, 10);
    if (end == argv[3] || *end != '\0' ||
        ((num_proc == LONG_MIN || num_proc == LONG_MAX) && (errno == ERANGE || errno == EINVAL)) ||
        num_proc < (long) INT_MIN || num_proc > (long) INT_MAX) {
        fprintf(stderr, "num_workers is not a valid number\n");
        exit(EXIT_FAILURE);
    }

    worker_t *worker = worker_init(argv[1], argv[2], num_proc, worker_int_proc);
    if (worker == NULL) {
        fprintf(stderr, "failed to initialize worker\n");
        exit(EXIT_FAILURE);
    }
    if (set_timeout(10000) < 0) {
        fprintf(stderr, "set_timeout() failed\n");
        worker_destroy(worker);
        exit(EXIT_FAILURE);
    }
    if (worker_run(worker) < 0) {
        fprintf(stderr, "worker_run() failed\n");
        worker_destroy(worker);
        exit(EXIT_FAILURE);
    }
    worker_destroy(worker);
}
