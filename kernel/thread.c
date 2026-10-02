#include "thread.h"
#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "serial.h"

/* Provided by switch.asm.
   context_switch(&old_rsp, new_rsp)
   Saves current callee-saved regs, stores RSP into *old_rsp,
   loads new_rsp into RSP, restores, returns into the new thread. */
extern void context_switch(uint64_t *old_rsp, uint64_t new_rsp);

/* The trampoline a new thread starts in.
   context_switch "returns" here, with the new thread's RSP. */
extern void thread_start_trampoline(void);

static thread_t threads[THREAD_MAX];
static int      thread_count = 0;
static int      current_idx  = -1;
static uint64_t next_stack_virt = 0x41000000ULL;

void scheduler_init(void)
{
    thread_count = 0;
    current_idx  = -1;
}

thread_t *thread_create(void (*fn)(void), const char *name)
{
    if (thread_count >= THREAD_MAX) return 0;

    thread_t *t = &threads[thread_count];
    t->id         = thread_count;
    t->name       = name;
    t->fn         = fn;
    t->state      = THREAD_READY;
    t->stack_base = next_stack_virt;
    t->stack_size = THREAD_STACK_SIZE;
    next_stack_virt += THREAD_STACK_SIZE;

    /* Allocate + map stack pages */
    uint64_t pages = THREAD_STACK_SIZE / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page(t->stack_base + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }

    /* Build the initial stack frame.
       The thread's first execution is:
           thread_start_trampoline
       which will call t->fn.

       Stack layout (low -> high):
           [r15..rax]  saved callee-saved regs (zeroed)
           [ret addr]  = thread_start_trampoline
    */
    uint64_t top = (t->stack_base + t->stack_size) & ~0xFULL;
    uint64_t *sp = (uint64_t *)top;

    /* Push return address FIRST — it's the highest slot, popped by `ret`. */
    sp--;
    *sp = (uint64_t)thread_start_trampoline;

    /* Then 6 register slots — popped by pop r15..rbp. */
    for (int i = 0; i < 6; i++) {
        sp--;
        *sp = 0;
    }

    t->rsp = (uint64_t)sp;

    thread_count++;
    return t;
}

/* Called from timer IRQ (irq_handler). */
void scheduler_tick(void)
{
    if (thread_count == 0) return;

    /* Save current rsp into current thread's struct, advance index,
       switch to next thread. */
    int old_idx = current_idx;

    current_idx = (current_idx + 1) % thread_count;

    if (current_idx == old_idx) {
        /* Only one thread. Do nothing. */
        return;
    }

    threads[old_idx].state = THREAD_READY;
    threads[current_idx].state = THREAD_RUNNING;

    /* Switch stacks. */
    context_switch(&threads[old_idx].rsp, threads[current_idx].rsp);
}

/* Called by the trampoline to run the thread function. */
void thread_runner(void)
{
    if (current_idx < 0) return;
    thread_t *t = &threads[current_idx];
    if (t->fn) t->fn();

    /* If fn returns, spin. */
    for (;;) __asm__ volatile("hlt");
}

void thread_yield(void)
{
    scheduler_tick();
}

thread_t *thread_current(void)
{
    if (current_idx < 0) return 0;
    return &threads[current_idx];
}

/* Called once at boot to prime the scheduler into thread 0. */
void scheduler_start(void)
{
    if (thread_count == 0) return;
    current_idx = 0;
    threads[0].state = THREAD_RUNNING;

    /* Switch from boot context into thread 0. The "old_rsp" we pass
       is a dummy — we never come back to boot. */
    static uint64_t dummy_rsp;
    context_switch(&dummy_rsp, threads[0].rsp);
}
