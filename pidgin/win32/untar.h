/*
 *  untar.h
 *
 *  Author: Herman Bloggs <hermanator12002@yahoo.com>
 *  Date: April, 2003
 *  Description: untar.c header
 */
#ifndef _UNTAR_H_
#define _UNTAR_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _untar_opt {
	UNTAR_LISTING =      (1 << 0),
	UNTAR_QUIET =        (1 << 1),
	UNTAR_VERBOSE =      (1 << 2),
	UNTAR_FORCE =        (1 << 3),
	UNTAR_ABSPATH =      (1 << 4),
	UNTAR_CONVERT =      (1 << 5)
} untar_opt;

int untar(const char *filename, const char *destdir, untar_opt options);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
