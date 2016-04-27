/*
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301, USA.
 */
#ifndef _DNS_SD_PROXY
#define _DNS_SD_PROXY


#ifndef _MSC_VER
#include <stdint.h>
#endif

/* The following is a subset of Apple's dns_sd.h file
 * http://www.opensource.apple.com/source/mDNSResponder/mDNSResponder-333.10/mDNSShared/dns_sd.h
 *
 * Copyright (c) 2003-2004, Apple Computer, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#  if defined(_WIN32) && !defined(EFI32) && !defined(EFI64)
#    define DNSSD_API __stdcall
#  else
#    define DNSSD_API
#  endif

#  define kDNSServiceInterfaceIndexAny 0
#  define kDNSServiceMaxDomainName 1009

typedef gint32 DNSServiceErrorType;
typedef guint32 DNSServiceFlags;
typedef struct _DNSServiceRef_t *DNSServiceRef;
typedef struct _DNSRecordRef_t *DNSRecordRef;
typedef guint32 DNSServiceProtocol;

typedef union _TXTRecordRef_t {
	gchar PrivateData[16];
	gchar *ForceNaturalAlignment;
} TXTRecordRef;

typedef void (DNSSD_API *DNSServiceBrowseReply)();
typedef void (DNSSD_API *DNSServiceGetAddrInfoReply)();
typedef void (DNSSD_API *DNSServiceQueryRecordReply)(
	DNSServiceRef sdRef, DNSServiceFlags flags, guint32 interfaceIndex,
	DNSServiceErrorType errorCode, const gchar *fullname,
	guint16 rrtype, guint16 rrclass, guint16 rdlen, const void *rdata,
	guint32 ttl, void *context);
typedef void (DNSSD_API *DNSServiceRegisterReply)();
typedef void (DNSSD_API *DNSServiceResolveReply)(
	DNSServiceRef sdRef, DNSServiceFlags flags, guint32 interfaceIndex,
	DNSServiceErrorType errorCode, const gchar *fullname,
	const gchar *hosttarget, guint16 port, guint16 txtLen,
	const guchar *txtRecord, void *context);

enum {
	kDNSServiceErr_NoError = 0,
	kDNSServiceErr_Unknown = -65537,
};

enum {
	kDNSServiceFlagsAdd = 0x2,
	kDNSServiceFlagsLongLivedQuery = 0x100,
};

enum {
	kDNSServiceType_NULL = 10,
	kDNSServiceType_TXT = 16,
};

enum {
	kDNSServiceClass_IN = 1,
};

enum {
	kDNSServiceProtocol_IPv4 = 0x01,
};

/* End dns_sd.h subset */

gboolean dns_sd_available(void);

DNSServiceErrorType _wpurple_DNSServiceAddRecord(DNSServiceRef sdRef, DNSRecordRef *RecordRef, DNSServiceFlags flags,
	uint16_t rrtype, uint16_t rdlen, const void *rdata, uint32_t ttl);
#define DNSServiceAddRecord(sdRef, RecordRef, flags, rrtype, rdlen, rdata, ttl) \
	_wpurple_DNSServiceAddRecord(sdRef, RecordRef, flags, rrtype, rdlen, rdata, ttl)

DNSServiceErrorType _wpurple_DNSServiceBrowse(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	const char *regtype, const char *domain, DNSServiceBrowseReply callBack, void *context);
#define DNSServiceBrowse(sdRef, flags, interfaceIndex, regtype, domain, callBack, context) \
	_wpurple_DNSServiceBrowse(sdRef, flags, interfaceIndex, regtype, domain, callBack, context)

int _wpurple_DNSServiceConstructFullName(char *fullName, const char *service, const char *regtype, const char *domain);
#define DNSServiceConstructFullName(fullName, service, regtype, domain) \
	_wpurple_DNSServiceConstructFullName(fullName, service, regtype, domain)

DNSServiceErrorType _wpurple_DNSServiceGetAddrInfo(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	DNSServiceProtocol protocol, const char *hostname, DNSServiceGetAddrInfoReply callBack, void *context);
#define DNSServiceGetAddrInfo(sdRef, flags, interfaceIndex, protocol, hostname, callBack, context) \
	_wpurple_DNSServiceGetAddrInfo(sdRef, flags, interfaceIndex, protocol, hostname, callBack, context)

DNSServiceErrorType _wpurple_DNSServiceProcessResult(DNSServiceRef sdRef);
#define DNSServiceProcessResult(sdRef) \
	_wpurple_DNSServiceProcessResult(sdRef);

DNSServiceErrorType _wpurple_DNSServiceQueryRecord(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	const char *fullname, uint16_t rrtype, uint16_t rrclass, DNSServiceQueryRecordReply callBack, void *context);
#define DNSServiceQueryRecord(sdRef, flags, interfaceIndex, fullname, rrtype, rrclass, callBack, context) \
	_wpurple_DNSServiceQueryRecord(sdRef, flags, interfaceIndex, fullname, rrtype, rrclass, callBack, context)

void _wpurple_DNSServiceRefDeallocate(DNSServiceRef sdRef);
#define DNSServiceRefDeallocate(sdRef) \
	_wpurple_DNSServiceRefDeallocate(sdRef)

int _wpurple_DNSServiceRefSockFD(DNSServiceRef sdRef);
#define DNSServiceRefSockFD(sdRef) \
	_wpurple_DNSServiceRefSockFD(sdRef)

DNSServiceErrorType _wpurple_DNSServiceRegister(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
	const char *name, const char *regtype, const char *domain, const char *host, uint16_t port, uint16_t txtLen,
	const void *txtRecord, DNSServiceRegisterReply callBack, void *context);
#define DNSServiceRegister(sdRef, flags, interfaceIndex, name, regtype, domain, host, port, txtLen, txtRecord, callBack, context) \
	_wpurple_DNSServiceRegister(sdRef, flags, interfaceIndex, name, regtype, domain, host, port, txtLen, txtRecord, callBack, context)

DNSServiceErrorType _wpurple_DNSServiceResolve(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *name,
	const char *regtype, const char *domain, DNSServiceResolveReply callBack, void *context);
#define DNSServiceResolve(sdRef, flags, interfaceIndex, name, regtype, domain, callBack, context) \
	_wpurple_DNSServiceResolve(sdRef, flags, interfaceIndex, name, regtype, domain, callBack, context)

DNSServiceErrorType _wpurple_DNSServiceRemoveRecord(DNSServiceRef sdRef, DNSRecordRef RecordRef, DNSServiceFlags flags);
#define DNSServiceRemoveRecord(sdRef, RecordRef, flags) \
	_wpurple_DNSServiceRemoveRecord(sdRef, RecordRef, flags)

DNSServiceErrorType _wpurple_DNSServiceUpdateRecord(DNSServiceRef sdRef, DNSRecordRef RecordRef, DNSServiceFlags flags,
	uint16_t rdlen, const void *rdata, uint32_t ttl);
#define DNSServiceUpdateRecord(sdRef, RecordRef, flags, rdlen, rdata, ttl) \
	_wpurple_DNSServiceUpdateRecord(sdRef, RecordRef, flags, rdlen, rdata, ttl)

void _wpurple_TXTRecordCreate(TXTRecordRef *txtRecord, uint16_t bufferLen, void *buffer);
#define TXTRecordCreate(txtRecord, bufferLen, buffer) \
	_wpurple_TXTRecordCreate(txtRecord, bufferLen, buffer)

void _wpurple_TXTRecordDeallocate(TXTRecordRef *txtRecord);
#define TXTRecordDeallocate(txtRecord) \
	_wpurple_TXTRecordDeallocate(txtRecord)

const void * _wpurple_TXTRecordGetBytesPtr(const TXTRecordRef *txtRecord);
#define TXTRecordGetBytesPtr(txtRecord) \
	_wpurple_TXTRecordGetBytesPtr(txtRecord)

uint16_t _wpurple_TXTRecordGetLength(const TXTRecordRef *txtRecord);
#define TXTRecordGetLength(txtRecord) \
	_wpurple_TXTRecordGetLength(txtRecord)

const void * _wpurple_TXTRecordGetValuePtr(uint16_t txtLen, const void *txtRecord, const char *key, uint8_t *valueLen);
#define TXTRecordGetValuePtr(txtLen, txtRecord, key, valueLen) \
	_wpurple_TXTRecordGetValuePtr(txtLen, txtRecord, key, valueLen)

DNSServiceErrorType _wpurple_TXTRecordSetValue(TXTRecordRef *txtRecord, const char *key, uint8_t valueSize, const void *value);
#define TXTRecordSetValue(txtRecord, key, valueSize, value) \
	_wpurple_TXTRecordSetValue(txtRecord, key, valueSize, value)

#endif
