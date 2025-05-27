#include "inc/controller.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "inc/net.h"
#include "inc/server.h"
#include <sys/param.h>
#include <stdalign.h>

controller_t *controller_init(const char *addr, const char *port, size_t num_conns) {
    controller_t *ctrl = calloc(1, sizeof(*ctrl));
    if (ctrl == NULL) {
        fprintf(stderr, "controller_init: calloc() failed\n");
        return NULL;
    }
    ctrl->num_conns = num_conns;
    ctrl->conns = calloc(num_conns, sizeof(*ctrl->conns));
    if (ctrl->conns == NULL) {
        fprintf(stderr, "controller_init: calloc() failed\n");
        free(ctrl);
        return NULL;
    }
    for (size_t i = 0; i < num_conns; ++i) {
        ctrl->conns[i].connfd = -1;
    }
    if (pthread_mutex_init(&ctrl->lock, NULL) < 0) {
        fprintf(stderr, "controller_init: mutex init failed\n");
        free(ctrl->conns);
        free(ctrl);
        return NULL;
    }

    ctrl->sockfd = server_init(addr, port);
    if (ctrl->sockfd < 0) {
        fprintf(stderr, "controller_init: mutex init failed\n");
        pthread_mutex_destroy(&ctrl->lock);
        free(ctrl->conns);
        free(ctrl);
        return NULL;
    }

    return ctrl;
}

void controller_destroy(controller_t *ctrl) {
    /* for (size_t i = 0; i < ctrl->num_conns; ++i) { */
    /*     if (ctrl->conns[i].connfd != -1) { */
    /*         close(ctrl->conns[i].connfd); */
    /*     } */
    /* } */
    close(ctrl->sockfd);
    pthread_mutex_destroy(&ctrl->lock);
    free(ctrl->conns);
    free(ctrl->ctasks);
    free(ctrl);
}

int controller_add_task(controller_t *ctrl, task_t *task) {
    /* if (pthread_mutex_lock(&ctrl->lock)) { */
    /*     fprintf(stderr, "controller_add_task: mutex lock failed\n"); */
    /*     return -1; */
    /* } */
    if (ctrl->ctasks_len == ctrl->ctasks_cap) {
        if (ctrl->ctasks_cap == 0) {
            ctrl->ctasks_cap = 1;
        } else {
            ctrl->ctasks_cap *= 2;
        }
        ctrl->ctasks = realloc(ctrl->ctasks, ctrl->ctasks_cap * sizeof(*ctrl->ctasks));
        if (ctrl->ctasks == NULL) {
            fprintf(stderr, "controller_add_task: realloc() failed\n");
            return -1;
        }
    }
    controller_task_t *ctask = &ctrl->ctasks[ctrl->ctasks_len];
    task->id = ++ctrl->ctasks_len;
    ctask->task = *task;
    ctask->status = TASK_NEW;
    /* if (pthread_mutex_unlock(&ctrl->lock)) { */
    /*     fprintf(stderr, "controller_add_task: mutex unlock failed\n"); */
    /*     return -1; */
    /* } */
    return 0;
}

int controller_get_result(controller_t *ctrl, int32_t id, uint8_t **result, size_t *result_sz) {
    if (ctrl->ctasks[id - 1].status != TASK_FINISHED) {
        fprintf(stderr, "controller_get_result: task not finished\n");
        return -1;
    }
    *result = ctrl->ctasks[id - 1].result;
    *result_sz = ctrl->ctasks[id - 1].result_sz;
    return 0;
}

int controller_wait(controller_t *ctrl) {
    if (pthread_join(ctrl->tid, NULL) < 0) {
        fprintf(stderr, "controller_wait: can't join\n");
        return -1;
    }
    return 0;
}

struct connarg {
    controller_t *ctrl;
    worker_conn_t *conn;
};

void *controller_conn(void *args) {
    struct connarg *arr = args;
    controller_t *ctrl = arr->ctrl;
    worker_conn_t *conn = arr->conn;
    free(args);

    for (;;) {
        size_t msg_sz;
        msg_t *msg = (msg_t *) conn_read(conn->connfd, &msg_sz);
        if (msg_sz == 0) {
            break;
        }

        switch (msg->type) {
            case MSG_GET_TASK:
                size_t *task_ind = calloc(msg->val.get_task.count, sizeof(*task_ind));
                if (task_ind == NULL) {
                    fprintf(stderr, "controller_conn: calloc() failed\n");
                    exit(EXIT_FAILURE);
                }
                size_t resp_cnt = 0;
                msg_t *resp;
                size_t resp_sz = sizeof(*resp);
                if (pthread_mutex_lock(&ctrl->lock) < 0) {
                    fprintf(stderr, "controller_conn: failed to lock mutex\n");
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < ctrl->ctasks_len; ++i) {
                    if (ctrl->ctasks[i].status == TASK_NEW) {
                        ctrl->ctasks[i].status = TASK_RUNNING;
                        ctrl->ctasks[i].worker = conn;
                        /* resp->val.task_list.tasks[resp->val.task_list.count++] = ctrl->ctasks[i].task; */
                        task_ind[resp_cnt++] = i;
                        resp_sz += sizeof(inline_task_t) + roundup(ctrl->ctasks[i].task.size, alignof(inline_task_t));
                    }
                    if (resp_cnt == msg->val.get_task.count) {
                        break;
                    }
                }
                if (pthread_mutex_unlock(&ctrl->lock) < 0) {
                    fprintf(stderr, "controller_conn: failed to unlock mutex\n");
                    exit(EXIT_FAILURE);
                }
                resp = calloc(1, resp_sz);
                if (resp == NULL) {
                    fprintf(stderr, "controller_conn: calloc() failed\n");
                    exit(EXIT_FAILURE);
                }
                resp->type = MSG_TASK_LIST;
                resp->val.task_list.count = resp_cnt;
                uint8_t *curr = resp->val.task_list.data;
                for (size_t i = 0; i < resp_cnt; ++i) {
                    inline_task_t *curr_task = (inline_task_t *) curr;
                    curr_task->id = ctrl->ctasks[task_ind[i]].task.id;
                    curr_task->size = ctrl->ctasks[task_ind[i]].task.size;
                    memcpy(curr_task->data, ctrl->ctasks[task_ind[i]].task.data, curr_task->size);
                    curr += sizeof(inline_task_t) + roundup(curr_task->size, alignof(inline_task_t));
                }
                free(task_ind);
                if (conn_write(conn->connfd, (uint8_t *) resp, resp_sz) < 0) {
                    fprintf(stderr, "controller_conn: conn_write failed\n");
                    exit(EXIT_FAILURE);
                }
                free(resp);
                break;
            case MSG_SEND_RESULT:
                controller_task_t *ctask = &ctrl->ctasks[msg->val.send_result.id - 1];
                ctask->status = TASK_FINISHED;
                ctask->result_sz = msg->val.send_result.size;
                ctask->result = calloc(1, msg->val.send_result.size);
                if (ctask->result == NULL) {
                    fprintf(stderr, "controller_conn: calloc() failed\n");
                    exit(EXIT_FAILURE);
                }
                memcpy(ctask->result, msg->val.send_result.data, ctask->result_sz);
                break;
            default:
                break;
        }
        free(msg);
    }

    for (size_t i = 0; i < ctrl->ctasks_len; ++i) {
        if (ctrl->ctasks[i].worker == conn && ctrl->ctasks[i].status == TASK_RUNNING) {
            fprintf(stderr, "controller_conn: worker exited too early\n");
            exit(EXIT_FAILURE);
        }
    }

    close(conn->connfd);

    return NULL;
}

void *controller_loop(void *args) {
    puts("waiting for connections...");
    controller_t *ctrl = args;
    for (size_t i = 0; i < ctrl->num_conns; ++i) {
        int connfd = -2;
        for (;;) {
            connfd = server_try_accept(ctrl->sockfd);
            if (connfd == -1) {
                fprintf(stderr, "controller_loop: server_try_accept failed\n");
                exit(EXIT_FAILURE);
            }
            if (connfd == -2) {
                usleep(1000);
                continue;
            }
            break;
        }
        ctrl->conns[i].connfd = connfd;
        struct connarg *args = calloc(1, sizeof(*args));
        if (args == NULL) {
            fprintf(stderr, "controller_loop: calloc() failed\n");
            exit(EXIT_FAILURE);
        }
        args->ctrl = ctrl;
        args->conn = &ctrl->conns[i];
        if (pthread_create(&ctrl->conns[i].tid, NULL, controller_conn, args) < 0) {
            fprintf(stderr, "controller_loop: can't create thread\n");
            exit(EXIT_FAILURE);
        }
        printf("%zu connected!\n", i);
    }

    for (size_t i = 0; i < ctrl->num_conns; ++i) {
        if (pthread_join(ctrl->conns[i].tid, NULL) < 0) {
            fprintf(stderr, "controller_loop: can't join\n");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;
}

int controller_run(controller_t *ctrl) {
    if (pthread_create(&ctrl->tid, NULL, controller_loop, ctrl) < 0) {
        fprintf(stderr, "controller_wait: can't create thread\n");
        return -1;
    }
    return 0;
}
