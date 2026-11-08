#pragma once
#define GINT_CALL(function, argument) function, argument
int gint_world_switch(int (*function)(void *), void *argument);
