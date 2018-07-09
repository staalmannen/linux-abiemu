#ifndef _ABI_UTIL_ERRNO_H
#define _ABI_UTIL_ERRNO_H

#ident "%W% %G%"

#include <asm/abi_machdep.h>
#include <abi/util/map.h>

/*
 * Translate the errno numbers from linux to current personality.
 * This should be removed and all other sources changed to call the
 * map function above directly.
 */
#define iABI_errors(errno) \
        (map_value(current_thread_info()->exec_domain->err_map, errno, 1))

#endif /* _ABI_UTIL_ERRNO_H */
