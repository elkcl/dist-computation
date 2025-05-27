#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <pthread.h>
#include "common.h"

typedef enum {
    TASK_EMPTY = 0,
    TASK_NEW = 1,
    TASK_RUNNING = 2,
    TASK_FINISHED = 3,
} task_status_t;

typedef struct {
    int connfd;
    pthread_t tid;
} worker_conn_t;

typedef struct {
    task_t task;
    task_status_t status;
    worker_conn_t *worker;

    size_t result_sz;
    uint8_t *result;
} controller_task_t;

typedef struct {
    int sockfd;
    pthread_mutex_t lock;

    pthread_t tid;

    size_t num_conns;
    worker_conn_t *conns;

    size_t ctasks_len;
    size_t ctasks_cap;
    controller_task_t *ctasks;
} controller_t;

controller_t* controller_init(const char *addr, const char *port, size_t num_conns);
void controller_destroy(controller_t *ctrl);
int controller_run(controller_t *ctrl);
int controller_wait(controller_t *ctrl);
int controller_add_task(controller_t *ctrl, task_t *task);
int controller_get_result(controller_t *ctrl, int32_t id, uint8_t **result, size_t *result_sz);

#endif // CONTROLLER_H
