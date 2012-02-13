/* ! \file \brief Functions for dokan support. */

#ifndef DOKAN_IFACE_H
#define DOKAN_IFACE_H

/* ! Is ZFS mounted? */
extern bool mounted;

extern void kernel_unmount(void);
extern bool kernel_start(void);
extern void kernel_cleanup(void);


#endif
