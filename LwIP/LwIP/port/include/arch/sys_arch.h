#ifndef SYS_ARCH_H
#define SYS_ARCH_H

#include <stdint.h>
#include <stddef.h>
#include "lwip/arch.h"
#include "lwip/opt.h"

typedef int sys_sem_t;
#define sys_sem_valid(sema) ((sema) > 0)
#define sys_sem_valid_val(sema) ((sema) > 0)

struct lwip_mbox {
  void* sem;
  void** q_mem;
  unsigned int head, tail;
  int size;
  int used;
};
typedef struct lwip_mbox sys_mbox_t;
#define SYS_MBOX_NULL NULL
#define sys_mbox_valid(mbox) ((mbox != NULL) && ((mbox)->sem != NULL) && ((mbox)->sem != (void*)-1))
#define sys_mbox_valid_val(mbox) (((mbox).sem != NULL) && ((mbox).sem != (void*)-1))

typedef u32_t sys_thread_t;

#define SYS_ARCH_TIMEOUT 0xffffffffUL
#define SYS_MBOX_EMPTY SYS_ARCH_TIMEOUT
#define SYS_ARCH_DECL_PROTECT(lev)
#define SYS_ARCH_PROTECT(lev)
#define SYS_ARCH_UNPROTECT(lev)

#endif

