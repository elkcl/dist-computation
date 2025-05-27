#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include "common.h"

typedef int (*worker_callback_t)(const uint8_t *data, size_t size, uint8_t **result, size_t *result_sz);

typedef struct {
    int connfd;
    pthread_mutex_t lock;

    size_t curr_task;
    size_t num_tasks;
    task_t *tasks;

    size_t num_procs;
    pthread_t *procs;

    worker_callback_t callback;
} worker_t;

worker_t *worker_init(const char *addr, const char *port, size_t num_procs, worker_callback_t callback);
void worker_destroy(worker_t *worker);
int worker_run(worker_t *worker);

#endif  // WORKER_H
