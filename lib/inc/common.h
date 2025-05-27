#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

typedef struct {
    int32_t id;
    size_t size;
    uint8_t *data;
} task_t;

typedef struct {
    int32_t id;
    size_t size;
    uint8_t data[];
} inline_task_t;

constexpr task_t ERR_TASK = {.id = -1, .size = 0, .data = NULL};
constexpr task_t EMPTY_TASK = {.id = 0, .size = 0, .data = NULL};

typedef enum {
    MSG_GET_TASK,
    MSG_TASK_LIST,
    MSG_SEND_RESULT,
} msg_type_t;

typedef struct {
    msg_type_t type;
    union {
        struct {
            uint32_t count;
        } get_task;
        struct {
            uint32_t count;
            alignas(alignof(inline_task_t)) uint8_t data[];
        } task_list;
        struct {
            int32_t id;
            size_t size;
            uint8_t data[];
        } send_result;
    } val;
} msg_t;

int set_timeout(int delay);

#endif  // COMMON_H

