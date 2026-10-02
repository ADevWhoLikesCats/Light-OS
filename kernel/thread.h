#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <stddef.h>

#define THREAD_STACK_SIZE  (16 * 1024)
#define THREAD_MAX         16

enum thread_state {
    THREAD_READY   = 0,
    THREAD_RUNNING = 1,
    THREAD_BLOCKED = 2,
    THREAD_DEAD    = 3,
};

struct thread {
    uint64_t rsp;              /* saved RSP */
    uint64_t stack_base;
    uint64_t stack_size;
    enum thread_state state;
    uint64_t id;
    const char *name;
    void (*fn)(void);          /* thread entry point */
};

typedef struct thread thread_t;

void      scheduler_init(void);
thread_t *thread_create(void (*fn)(void), const char *name);
void      thread_yield(void);
void      scheduler_tick(void);
thread_t *thread_current(void);

#endif
