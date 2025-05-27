#include "inc/worker.h"
#include "inc/client.h"
#include "inc/net.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <stdalign.h>

worker_t *worker_init(const char *addr, const char *port, size_t num_procs, worker_callback_t callback) {
    worker_t *worker = calloc(1, sizeof(*worker));
    if (worker == NULL) {
        fprintf(stderr, "worker_init: calloc() failed\n");
        return NULL;
    }

    worker->connfd = connect_to_server(addr, port);
    if (worker->connfd < 0) {
        fprintf(stderr, "worker_init: connect_to_server() failed\n");
        free(worker);
        return NULL;
    }

    worker->tasks = calloc(num_procs, sizeof(*worker->tasks));
    if (worker->tasks == NULL) {
        fprintf(stderr, "worker_init: calloc() failed\n");
        free(worker);
        return NULL;
    }

    worker->procs = calloc(num_procs, sizeof(*worker->procs));
    if (worker->procs == NULL) {
        fprintf(stderr, "worker_init: calloc() failed\n");
        free(worker->tasks);
        free(worker);
        return NULL;
    }

    worker->num_procs = num_procs;
    worker->num_tasks = num_procs;
    worker->curr_task = num_procs;
    worker->callback = callback;
    if (pthread_mutex_init(&worker->lock, NULL) < 0) {
        fprintf(stderr, "worker_init: pthread_mutex_init() failed\n");
        free(worker->tasks);
        free(worker->procs);
        free(worker);
        return NULL;
    }

    return worker;
}

void worker_destroy(worker_t *worker) {
    close(worker->connfd);
    free(worker->tasks);
    free(worker->procs);
    pthread_mutex_destroy(&worker->lock);
    free(worker);
}

task_t get_task(worker_t *worker) {
    if (pthread_mutex_lock(&worker->lock) < 0) {
        fprintf(stderr, "get_task: failed to lock mutex\n");
        return ERR_TASK;
    }

    if (worker->num_tasks == 0) {
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "get_task: failed to unlock mutex\n");
        }
        return EMPTY_TASK;
    }

    if (worker->curr_task < worker->num_tasks) {
        task_t ans = worker->tasks[worker->curr_task];
        ++worker->curr_task;
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "get_task: failed to unlock mutex\n");
            return ERR_TASK;
        }
        return ans;
    }

    msg_t request = {.type = MSG_GET_TASK, .val = {.get_task = {.count = worker->num_tasks}}};
    if (conn_write(worker->connfd, (uint8_t *) &request, sizeof(request)) < 0) {
        fprintf(stderr, "get_task: conn_write() failed\n");
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "get_task: failed to unlock mutex\n");
        }
        return ERR_TASK;
    }

    size_t ans_size;
    msg_t *tasks = (msg_t *) conn_read(worker->connfd, &ans_size);
    if (tasks == NULL) {
        fprintf(stderr, "get_task: conn_read() failed\n");
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "get_task: failed to unlock mutex\n");
        }
        return ERR_TASK;
    }

    worker->num_tasks = tasks->val.task_list.count;
    if (worker->num_tasks == 0) {
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "get_task: failed to unlock mutex\n");
        }
        free(tasks);
        return EMPTY_TASK;
    }

    /* memcpy(worker->tasks, tasks->val.task_list.tasks, worker->num_tasks * sizeof(*worker->tasks)); */
    uint8_t *curr = tasks->val.task_list.data;
    for (size_t i = 0; i < worker->num_tasks; ++i) {
        inline_task_t *curr_task = (inline_task_t *) curr;
        worker->tasks[i].id = curr_task->id;
        worker->tasks[i].size = curr_task->size;
        worker->tasks[i].data = calloc(1, curr_task->size);
        if (worker->tasks[i].data == NULL) {
            fprintf(stderr, "get_task: calloc() failed\n");
            if (pthread_mutex_unlock(&worker->lock) < 0) {
                fprintf(stderr, "get_task: failed to unlock mutex\n");
            }
            return ERR_TASK;
        }
        memcpy(worker->tasks[i].data, curr_task->data, curr_task->size);
        curr += sizeof(inline_task_t) + roundup(curr_task->size, alignof(inline_task_t));
    }

    free(tasks);
    worker->curr_task = 0;
    task_t ans = worker->tasks[worker->curr_task];
    ++worker->curr_task;
    if (pthread_mutex_unlock(&worker->lock) < 0) {
        fprintf(stderr, "get_task: failed to unlock mutex\n");
        return ERR_TASK;
    }
    return ans;
}

void *worker_proc(void *args) {
    worker_t *worker = args;
    for (;;) {
        task_t task = get_task(worker);
        if (task.id == -1) {
            fprintf(stderr, "worker_proc: get_task() failed\n");
            exit(EXIT_FAILURE);
        } else if (task.id == 0) {
            break;
        }
        size_t result_sz;
        uint8_t *result;
        if (worker->callback(task.data, task.size, &result, &result_sz) < 0) {
            fprintf(stderr, "worker_proc: callback failed\n");
            exit(EXIT_FAILURE);
        }
        free(task.data);

        msg_t *send;
        const size_t send_sz = sizeof(*send) + result_sz;
        send = calloc(1, send_sz);
        if (send == NULL) {
            fprintf(stderr, "worker_proc: calloc() failed\n");
            exit(EXIT_FAILURE);
        }
        send->type = MSG_SEND_RESULT;
        send->val.send_result.id = task.id;
        send->val.send_result.size = result_sz;
        memcpy(send->val.send_result.data, result, result_sz);
        free(result);
        if (pthread_mutex_lock(&worker->lock) < 0) {
            fprintf(stderr, "worker_proc: failed to lock mutex\n");
            free(send);
            exit(EXIT_FAILURE);
        }
        if (conn_write(worker->connfd, (uint8_t *) send, send_sz) < 0) {
            fprintf(stderr, "worker_proc: conn_write() failed\n");
            if (pthread_mutex_unlock(&worker->lock) < 0) {
                fprintf(stderr, "worker_proc: failed to unlock mutex\n");
            }
            free(send);
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_unlock(&worker->lock) < 0) {
            fprintf(stderr, "worker_proc: failed to unlock mutex\n");
            free(send);
            exit(EXIT_FAILURE);
        }
        free(send);
    }
    return NULL;
}

int worker_run(worker_t *worker) {
    for (size_t i = 0; i < worker->num_procs; ++i) {
        int res = pthread_create(&worker->procs[i], NULL, worker_proc, worker);
        if (res < 0) {
            fprintf(stderr, "worker_run: can't create thread\n");
            return -1;
        }
    }
    for (size_t i = 0; i < worker->num_procs; ++i) {
        int res = pthread_join(worker->procs[i], NULL);
        if (res < 0) {
            fprintf(stderr, "worker_run: can't join thread\n");
            return -1;
        }
    }
    return 0;
}
