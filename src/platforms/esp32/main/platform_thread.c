#include "platform_thread.h"

#include <pthread.h>
#include <stdlib.h>

int waiot_mutex_init(waiot_mutex_t *mutex)
{
    if (!mutex)
    {
        return -1;
    }

    pthread_mutex_t *handle = (pthread_mutex_t *)malloc(sizeof(*handle));
    if (!handle)
    {
        return -1;
    }
    if (pthread_mutex_init(handle, NULL) != 0)
    {
        free(handle);
        return -1;
    }

    mutex->handle = handle;
    mutex->initialized = true;
    return 0;
}

void waiot_mutex_destroy(waiot_mutex_t *mutex)
{
    if (!mutex || !mutex->initialized)
    {
        return;
    }
    pthread_mutex_destroy((pthread_mutex_t *)mutex->handle);
    free(mutex->handle);
    mutex->handle = NULL;
    mutex->initialized = false;
}

void waiot_mutex_lock(waiot_mutex_t *mutex)
{
    if (mutex && mutex->initialized)
    {
        pthread_mutex_lock((pthread_mutex_t *)mutex->handle);
    }
}

void waiot_mutex_unlock(waiot_mutex_t *mutex)
{
    if (mutex && mutex->initialized)
    {
        pthread_mutex_unlock((pthread_mutex_t *)mutex->handle);
    }
}

int waiot_thread_create(waiot_thread_t *thread,
                        size_t stack_size,
                        waiot_thread_entry_t entry,
                        void *arg)
{
    if (!thread || !entry)
    {
        return -1;
    }

    pthread_t *handle = (pthread_t *)malloc(sizeof(*handle));
    if (!handle)
    {
        return -1;
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
    {
        free(handle);
        return -1;
    }
    if (stack_size > 0)
    {
        pthread_attr_setstacksize(&attr, stack_size);
    }

    int res = pthread_create(handle, &attr, entry, arg);
    pthread_attr_destroy(&attr);
    if (res != 0)
    {
        free(handle);
        return res;
    }

    thread->handle = handle;
    thread->context = NULL;
    thread->valid = true;
    return 0;
}

int waiot_thread_join(waiot_thread_t *thread)
{
    if (!waiot_thread_is_valid(thread))
    {
        return -1;
    }

    int res = pthread_join(*(pthread_t *)thread->handle, NULL);
    free(thread->handle);
    waiot_thread_invalidate(thread);
    return res;
}

bool waiot_thread_is_valid(const waiot_thread_t *thread)
{
    return thread && thread->valid && thread->handle;
}

void waiot_thread_invalidate(waiot_thread_t *thread)
{
    if (!thread)
    {
        return;
    }
    thread->handle = NULL;
    thread->context = NULL;
    thread->valid = false;
}
