#pragma once

#include <stdint.h>

#include "octaryn_shared_abi_types.h"

#ifndef OCTARYN_ABI_EXPORT
#if defined(_WIN32)
#if defined(OCTARYN_ABI_BUILD)
#define OCTARYN_ABI_EXPORT __declspec(dllexport)
#else
#define OCTARYN_ABI_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define OCTARYN_ABI_EXPORT __attribute__((visibility("default")))
#else
#define OCTARYN_ABI_EXPORT
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OCTARYN_HOST_ABI_VERSION 1u
#define OCTARYN_HOST_MIN_WORKER_THREADS 2u

typedef enum octaryn_thread_role {
    OCTARYN_THREAD_ROLE_MAIN = 1,
    OCTARYN_THREAD_ROLE_COORDINATOR = 2,
    OCTARYN_THREAD_ROLE_WORKER_POOL = 3
} octaryn_thread_role;

OCTARYN_ABI_EXPORT uint32_t OCTARYN_ABI_CALL octaryn_host_abi_version(void);
OCTARYN_ABI_EXPORT uint32_t OCTARYN_ABI_CALL octaryn_host_min_worker_threads(void);

#ifdef __cplusplus
}
#endif
