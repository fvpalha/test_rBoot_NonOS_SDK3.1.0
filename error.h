#ifndef _ERROR_H_
#define _ERROR_H_
/* Obviously, some of these don't make sense b/c we aren't linux.
 I like using these numbers though becuase they are common */
#define MAX_NUM_ERRORS 22

#define EPERM       1  /* Operation not permitted */
#define EIO         2  /* I/O error */
#define ENXIO       3  /* No such device or address */
#define E2BIG       4  /* Argument list too long */
#define EAGAIN      5  /* Try again */
#define ENOMEM      6  /* Out of memory */
#define EACCES      7  /* Permission denied */
#define EFAULT      8  /* Bad address */
#define ENOTBLK     9  /* Block device required */
#define EBUSY       10  /* Device or resource busy */
#define ENODEV      11  /* No such device */
#define EINVAL      12  /* Invalid argument */
#define ENOSPC      13  /* No space left on device */
#define EROFS       14  /* Read-only file system */
#define EDOM        15  /* Math argument out of domain of func */
#define ERANGE      16  /* Math result not representable */
#define ENOSYS      17  /* Function not implemented */
#define ENODATA     18  /* No data available */
#define ETIME       19  /* Timer expired */
#define EPROTO      20  /* Protocol error */
#define EMSGSIZE    21  /* Message too long */

#endif