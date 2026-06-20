#include "platform_thread.h"

#include "cmsis_os2.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
    waiot_thread_entry_t entry;
    void *arg;
    osSemaphoreId_t done;
} waiot_freertos_thread_context_t;

static void waiot_freertos_thread_trampoline(void *arg)
{
    waiot_freertos_thread_context_t *ctx =
        (waiot_freertos_thread_context_t *)arg;
    if (ctx && ctx->entry)
    {
        ctx->entry(ctx->arg);
    }
    if (ctx && ctx->done)
    {
        osSemaphoreRelease(ctx->done);
    }
    osThreadExit();
}

int waiot_mutex_init(waiot_mutex_t *mutex)
{
    if (!mutex)
    {
        return -1;
    }

    osMutexId_t handle = osMutexNew(NULL);
    if (!handle)
    {
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
    osMutexDelete((osMutexId_t)mutex->handle);
    mutex->handle = NULL;
    mutex->initialized = false;
}

void waiot_mutex_lock(waiot_mutex_t *mutex)
{
    if (mutex && mutex->initialized)
    {
        osMutexAcquire((osMutexId_t)mutex->handle, osWaitForever);
    }
}

void waiot_mutex_unlock(waiot_mutex_t *mutex)
{
    if (mutex && mutex->initialized)
    {
        osMutexRelease((osMutexId_t)mutex->handle);
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

    waiot_freertos_thread_context_t *ctx =
        (waiot_freertos_thread_context_t *)malloc(sizeof(*ctx));
    if (!ctx)
    {
        return -1;
    }

    ctx->entry = entry;
    ctx->arg = arg;
    ctx->done = osSemaphoreNew(1, 0, NULL);
    if (!ctx->done)
    {
        free(ctx);
        return -1;
    }

    osThreadAttr_t attr = {
        .name = "waiot_wamr",
        .stack_size = (uint32_t)stack_size,
    };

    osThreadId_t handle = osThreadNew(waiot_freertos_thread_trampoline,
                                      ctx, &attr);
    if (!handle)
    {
        osSemaphoreDelete(ctx->done);
        free(ctx);
        return -1;
    }

    thread->handle = handle;
    thread->context = ctx;
    thread->valid = true;
    return 0;
}

int waiot_thread_join(waiot_thread_t *thread)
{
    if (!waiot_thread_is_valid(thread))
    {
        return -1;
    }

    waiot_freertos_thread_context_t *ctx =
        (waiot_freertos_thread_context_t *)thread->context;
    if (!ctx || !ctx->done)
    {
        waiot_thread_invalidate(thread);
        return -1;
    }

    osStatus_t status = osSemaphoreAcquire(ctx->done, osWaitForever);
    osSemaphoreDelete(ctx->done);
    free(ctx);
    waiot_thread_invalidate(thread);
    return (status == osOK) ? 0 : -1;
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
