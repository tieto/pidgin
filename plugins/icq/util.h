/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif

#include "icqtypes.h"
#include "icq.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE !FALSE
#endif

void hex_dump(char *data, long size);

WORD Chars_2_Word(unsigned char *buf);
DWORD Chars_2_DW(unsigned char *buf);
void DW_2_Chars(unsigned char *buf, DWORD num);
void Word_2_Chars(unsigned char *buf, WORD num);

const char *icq_ConvertStatus2Str(unsigned long status);

#endif /* _UTIL_H_ */
