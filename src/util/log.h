#pragma once

#include <stdio.h>
#include <chrono>

inline long long whatTimeIsIt() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

#define LOGCOLOR_RESET "\033[0m"
#define LOGCOLOR_RED "\033[31m"
#define LOGCOLOR_BLUE "\033[34m"
#define LOGCOLOR_GREEN "\033[32m"
#define LOGCOLOR_YELLOW "\033[33m"
#define LOGCOLOR_PURPLE "\033[35m"
#define LOGCOLOR_CYAN "\033[36m"

#define TIME() whatTimeIsIt()
#define INFO(...)  {printf("%s[INFO]%s  ", LOGCOLOR_GREEN, LOGCOLOR_RESET);  printf(__VA_ARGS__); printf("\n");}
#define ERROR(...) {printf("%s[ERROR]%s Error detected in %s:%d - \"", LOGCOLOR_RED, LOGCOLOR_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n");}
#define FATAL(...) {printf("%s[FATAL]%s Critical failure in %s:%d - \"", LOGCOLOR_RED, LOGCOLOR_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n"); exit(1);}
#define WARN(...)  {printf("%s[WARN]%s  ", LOGCOLOR_YELLOW, LOGCOLOR_RESET); printf(__VA_ARGS__); printf("\n");}
#define DEBUG(...) {printf("%s[DEBUG]%s ", LOGCOLOR_BLUE, LOGCOLOR_RESET);   printf(__VA_ARGS__); printf("\n");}
#define CUSTOM(precursor, ...) {printf("%s[%s]%s  ", LOGCOLOR_CYAN, precursor, LOGCOLOR_RESET);   printf(__VA_ARGS__); printf("\n");}
#define SCAN(...)  {printf("%s[INPUT]%s ", LOGCOLOR_PURPLE, LOGCOLOR_RESET); scanf(__VA_ARGS__);}
#define ASSERT(x, ...) if (!(x)) { printf("%s[FAIL]%s  Assertion failed in %s:%d - \"", LOGCOLOR_RED, LOGCOLOR_RESET, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\"\n"); exit(1);}
