#ifndef _APP_VERSION_H_
#define _APP_VERSION_H_

/*  values come from cmake/version.cmake
 * BUILD_VERSION related  values will be 'git describe',
 * alternatively user defined BUILD_VERSION.
 */

/* #undef ZEPHYR_VERSION_CODE */
/* #undef ZEPHYR_VERSION */

#define APPVERSION          0x10300
#define APP_VERSION_NUMBER  0x103
#define APP_VERSION_MAJOR   0
#define APP_VERSION_MINOR   1
#define APP_PATCHLEVEL      3
#define APP_VERSION_STRING  "0.1.3"

#define APP_BUILD_VERSION v0.2.0-3-g8533cd8dbbf7


#endif /* _APP_VERSION_H_ */
