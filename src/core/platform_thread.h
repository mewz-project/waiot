#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef void *(*waiot_thread_entry_t)(void *arg);

typedef struct
{
    void *handle;
    void *context;
    bool valid;
} waiot_thread_t;

typedef struct
{
    void *handle;
    bool initialized;
} waiot_mutex_t;

int waiot_mutex_init(waiot_mutex_t *mutex);
void waiot_mutex_destroy(waiot_mutex_t *mutex);
void waiot_mutex_lock(waiot_mutex_t *mutex);
void waiot_mutex_unlock(waiot_mutex_t *mutex);

int waiot_thread_create(waiot_thread_t *thread,
                        size_t stack_size,
                        waiot_thread_entry_t entry,
                        void *arg);
int waiot_thread_join(waiot_thread_t *thread);
bool waiot_thread_is_valid(const waiot_thread_t *thread);
void waiot_thread_invalidate(waiot_thread_t *thread);
