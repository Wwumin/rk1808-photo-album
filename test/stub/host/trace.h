/*
 * test/stub/host/trace.h  —  冒烟测试的日志宏
 */

#ifndef TEST_TRACE_H
#define TEST_TRACE_H

#include <stdio.h>

#define trace(...) do { printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

#endif
