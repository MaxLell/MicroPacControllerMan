#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

/* Firmware shim: STATIC keeps internal linkage on target (the upstream test
 * build redefines it to expose statics to unit tests). */
#define STATIC static

#endif /* TEST_SUPPORT_H */
