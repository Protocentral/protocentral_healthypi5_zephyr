#ifndef _KERNEL_VERSION_H_
#define _KERNEL_VERSION_H_

/*  values come from cmake/version.cmake
 * BUILD_VERSION related  values will be 'git describe',
 * alternatively user defined BUILD_VERSION.
 */

#define ZEPHYR_VERSION_CODE 197888
#define ZEPHYR_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define KERNELVERSION          0x3050000
#define KERNEL_VERSION_NUMBER  0x30500
#define KERNEL_VERSION_MAJOR   3
#define KERNEL_VERSION_MINOR   5
#define KERNEL_PATCHLEVEL      0
#define KERNEL_VERSION_STRING  "3.5.0"

#define BUILD_VERSION zephyr-v3.5.0


#endif /* _KERNEL_VERSION_H_ */
