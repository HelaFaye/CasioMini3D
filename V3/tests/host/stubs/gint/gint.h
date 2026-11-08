#pragma once

struct gint_call_t
{
    int (*function)(void *);
    void *argument;
};

#define GINT_CALL(function, argument) gint_call_t{function, argument}
int gint_world_switch(gint_call_t call);
