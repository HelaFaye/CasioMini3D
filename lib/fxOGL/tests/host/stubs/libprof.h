#pragma once

#include <stdint.h>

typedef struct
{
    uint32_t depth;
    uint32_t start;
    uint32_t elapsed;
} prof_t;

extern uint32_t fxmark_test_prof_counter;

static inline void prof_init(void) {}
static inline void prof_quit(void) {}

static inline prof_t prof_make(void)
{
    prof_t context = {0, 0, 0};
    return context;
}

static inline void fxmark_test_prof_enter(prof_t* context)
{
    if (context->depth++ == 0)
        context->start = fxmark_test_prof_counter;
}

static inline void fxmark_test_prof_leave(prof_t* context)
{
    if (context->depth > 0 && --context->depth == 0)
        context->elapsed += fxmark_test_prof_counter - context->start;
}

static inline uint32_t fxmark_test_prof_time(prof_t context)
{
    return context.elapsed;
}

#define prof_enter(context) fxmark_test_prof_enter(&(context))
#define prof_leave(context) fxmark_test_prof_leave(&(context))
#define prof_time(context) fxmark_test_prof_time(context)
