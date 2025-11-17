#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/debug.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/stats.h"
#include "lwip/arch.h"
#include "arch/sys_arch.h"
#include <string.h>

void sys_init(void) {
}

u32_t sys_now(void) {
    extern unsigned long GetMillis(void);
    return (u32_t)GetMillis();
}

u32_t sys_jiffies(void) {
    extern unsigned long GetMillis(void);
    return (u32_t)GetMillis();
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    LWIP_ASSERT("sem != NULL", sem != NULL);
    LWIP_ASSERT("count invalid (not 0 or 1)", (count == 0) || (count == 1));
    *sem = count + 1;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem) {
    LWIP_ASSERT("sem != NULL", sem != NULL);
    *sem = 0;
}

void sys_sem_set_invalid(sys_sem_t *sem) {
    LWIP_ASSERT("sem != NULL", sem != NULL);
    *sem = 0;
}

void sys_sem_signal(sys_sem_t *sem) {
    LWIP_ASSERT("sem != NULL", sem != NULL);
    LWIP_ASSERT("*sem > 0", *sem > 0);
    (*sem)++;
    LWIP_ASSERT("*sem > 0", *sem > 0);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms) {
    LWIP_ASSERT("sem != NULL", sem != NULL);
    LWIP_ASSERT("*sem > 0", *sem > 0);
    
    if (*sem == 1) {
        if (timeout_ms == 0) {
            return SYS_ARCH_TIMEOUT;
        }
    }
    
    if (*sem > 1) {
        (*sem)--;
        return 1;
    }
    
    return SYS_ARCH_TIMEOUT;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    int mboxsize = size;
    LWIP_ASSERT("mbox != NULL", mbox != NULL);
    LWIP_ASSERT("size >= 0", size >= 0);
    
    if (size == 0) {
        mboxsize = TCPIP_MBOX_SIZE;
    }
    
    mbox->head = mbox->tail = 0;
    mbox->sem = mbox;
    mbox->q_mem = (void**)mem_malloc(sizeof(void*) * mboxsize);
    if (mbox->q_mem == NULL) {
        return ERR_MEM;
    }
    mbox->size = mboxsize;
    mbox->used = 0;
    
    memset(mbox->q_mem, 0, sizeof(void*) * mboxsize);
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    LWIP_ASSERT("mbox != NULL", mbox != NULL);
    LWIP_ASSERT("mbox->sem != NULL", mbox->sem != NULL);
    LWIP_ASSERT("mbox->q_mem != NULL", mbox->q_mem != NULL);
    
    mem_free(mbox->q_mem);
    mbox->sem = NULL;
    mbox->q_mem = NULL;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    LWIP_ASSERT("mbox != NULL", mbox != NULL);
    mbox->sem = NULL;
    mbox->q_mem = NULL;
}

void sys_mbox_post(sys_mbox_t *q, void *msg) {
    LWIP_ASSERT("q != SYS_MBOX_NULL", q != SYS_MBOX_NULL);
    LWIP_ASSERT("q->sem == q", q->sem == q);
    LWIP_ASSERT("q->q_mem != NULL", q->q_mem != NULL);
    LWIP_ASSERT("q->used < q->size", q->used < q->size);
    
    q->q_mem[q->head] = msg;
    q->head++;
    if (q->head >= (unsigned int)q->size) {
        q->head = 0;
    }
    q->used++;
}

err_t sys_mbox_trypost(sys_mbox_t *q, void *msg) {
    LWIP_ASSERT("q != SYS_MBOX_NULL", q != SYS_MBOX_NULL);
    LWIP_ASSERT("q->sem == q", q->sem == q);
    LWIP_ASSERT("q->q_mem != NULL", q->q_mem != NULL);
    
    if (q->used == q->size) {
        return ERR_MEM;
    }
    sys_mbox_post(q, msg);
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg) {
    return sys_mbox_trypost(q, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *q, void **msg, u32_t timeout_ms) {
    LWIP_ASSERT("q != SYS_MBOX_NULL", q != SYS_MBOX_NULL);
    LWIP_ASSERT("q->sem == q", q->sem == q);
    LWIP_ASSERT("q->q_mem != NULL", q->q_mem != NULL);
    
    if (q->used == 0) {
        if (timeout_ms == 0) {
            if (msg != NULL) {
                *msg = NULL;
            }
            return SYS_ARCH_TIMEOUT;
        }
    }
    
    if (q->used > 0) {
        sys_arch_mbox_tryfetch(q, msg);
        return 1;
    }
    
    if (msg != NULL) {
        *msg = NULL;
    }
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *q, void **msg) {
    LWIP_ASSERT("q != SYS_MBOX_NULL", q != SYS_MBOX_NULL);
    LWIP_ASSERT("q->sem == q", q->sem == q);
    LWIP_ASSERT("q->q_mem != NULL", q->q_mem != NULL);
    
    if (q->used == 0) {
        if (msg != NULL) {
            *msg = NULL;
        }
        return SYS_MBOX_EMPTY;
    }
    
    if (msg != NULL) {
        *msg = q->q_mem[q->tail];
    }
    
    q->tail++;
    if (q->tail >= (unsigned int)q->size) {
        q->tail = 0;
    }
    q->used--;
    
    return 0;
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio) {
    LWIP_UNUSED_ARG(name);
    LWIP_UNUSED_ARG(thread);
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(stacksize);
    LWIP_UNUSED_ARG(prio);
    return 0;
}

void sys_tcpip_thread(void *arg) {
}

