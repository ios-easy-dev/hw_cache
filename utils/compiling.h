// IWYU begin_export

#include <fcntl.h> // IWYU keep
#include <stddef.h> // IWYU keep
#include <stdint.h> // IWYU keep
#include <stdio.h> // IWYU keep
#include <stdlib.h> // IWYU keep
#include <sys/mman.h> // IWYU keep
#include <unistd.h> // IWYU keep
#include <cstdint> // IWYU keep
#include <fstream> // IWYU keep
#include <iostream> // IWYU keep
#include <string> // IWYU keep
#include <vector> // IWYU keep
#include <cassert> // IWYU keep
#include <cstring> // IWYU keep

// IWYU end_export

#define NOINLINE __attribute__((noinline))
#define PACKED __attribute__((packed))
#define COMPILER_BARRIER asm volatile("": : :"memory")
#define CACHE_LINE_ALIGNED __attribute__((__aligned__((CACHE_LINE_SIZE))))
#define OPTIMIZER_HIDE_VAR(var) __asm__ ("" : "=r" (var) : "0" (var)) // Make the optimizer believe the variable can be manipulated arbitrarily
#define IS_ARM (!__x86_64)
#define IS_INTEL (!!__x86_64)
#define EMPTY_MAIN int main() { return 0; }
#define IS_CPP20 (__cplusplus >= 202002L)

#if IS_ARM
#define CACHE_LINE_SIZE 128
#define PAGE_SIZE 16384
#else
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096
#endif


#define SYS_CALL_CHECK(FAILURE, WHAT) \
    if (FAILURE) { \
        perror(WHAT); \
        std::cerr << "ERROR: " << __FUNCTION__ << ' ' << __FILE__ << ':' << __LINE__ << std::endl;\
        exit(1);\
    }

#define SYS_CALL_COND(CALL, COND, WHAT) SYS_CALL_CHECK((CALL) COND, WHAT)
#define SYS_CALL(CALL, WHAT) SYS_CALL_COND(CALL, < 0, WHAT)
#define SYS_CALL_MMAP(CALL, WHAT) SYS_CALL_COND(CALL, == MAP_FAILED, WHAT)

#define FATAL_ERROR(...) {\
        std::cerr << "FATAL ERROR: " << __FUNCTION__ << ' ' << __FILE__ << ':' << __LINE__ << std::endl;\
        std::cerr << __VA_ARGS__ << std::endl;\
        exit(1);\
    }

#define CHECK(COND, WHAT) if(!(COND)) FATAL_ERROR("CHECK FAILED {"#COND <<"} " << WHAT)
