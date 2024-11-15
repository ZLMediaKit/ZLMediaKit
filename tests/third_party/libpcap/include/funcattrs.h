/* -*- Mode: c; tab-width: 8; indent-tabs-mode: 1; c-basic-offset: 8; -*- */
/*
 * Copyright (c) 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lib_pcap_funcattrs_h
#define lib_pcap_funcattrs_h

#include <pcap/compiler-tests.h>

/*
 * Attributes to apply to functions and their arguments, using various
 * compiler-specific extensions.
 */

/*
 * PCAP_API_DEF must be used when defining *data* exported from
 * libpcap.  It can be used when defining *functions* exported
 * from libpcap, but it doesn't have to be used there.  It
 * should not be used in declarations in headers.
 *
 * PCAP_API must be used when *declaring* data or functions
 * exported from libpcap; PCAP_API_DEF won't work on all platforms.
 */

#if defined(_WIN32)
  /*
   * For Windows:
   *
   *    when building libpcap:
   *
   *       if we're building it as a DLL, we have to declare API
   *       functions with __declspec(dllexport);
   *
   *       if we're building it as a static library, we don't want
   *       to do so.
   *
   *    when using libpcap:
   *
   *       if we're using the DLL, calls to its functions are a
   *       little more efficient if they're declared with
   *       __declspec(dllimport);
   *
   *       if we're not using the dll, we don't want to declare
   *       them that way.
   *
   * So:
   *
   *    if pcap_EXPORTS is defined, we define PCAP_API_DEF as
   *     __declspec(dllexport);
   *
   *    if PCAP_DLL is defined, we define PCAP_API_DEF as
   *    __declspec(dllimport);
   *
   *    otherwise, we define PCAP_API_DEF as nothing.
   */
  #if defined(pcap_EXPORTS)
    /*
     * We're compiling libpcap as a DLL, so we should export functions
     * in our API.
     */
    #define PCAP_API_DEF	__declspec(dllexport)
  #elif defined(PCAP_DLL)
    /*
     * We're using libpcap as a DLL, so the calls will be a little more
     * efficient if we explicitly import the functions.
     */
    #define PCAP_API_DEF	__declspec(dllimport)
  #else
    /*
     * Either we're building libpcap as a static library, or we're using
     * it as a static library, or we don't know for certain that we're
     * using it as a dynamic library, so neither import nor export the
     * functions explicitly.
     */
    #define PCAP_API_DEF
  #endif
#elif defined(MSDOS)
  /* XXX - does this need special treatment? */
  #define PCAP_API_DEF
#else /* UN*X */
  #ifdef pcap_EXPORTS
    /*
     * We're compiling libpcap as a (dynamic) shared library, so we should
     * export functions in our API.  The compiler might be configured not
     * to export functions from a shared library by default, so we might
     * have to explicitly mark functions as exported.
     */
    #if PCAP_IS_AT_LEAST_GNUC_VERSION(3,4) \
        || PCAP_IS_AT_LEAST_XL_C_VERSION(12,0)
      /*
       * GCC 3.4 and later, or some compiler asserting compatibility with
       * GCC 3.4 and later, or XL C 13.0 and later, so we have
       * __attribute__((visibility()).
       */
      #define PCAP_API_DEF	__attribute__((visibility("default")))
    #elif PCAP_IS_AT_LEAST_SUNC_VERSION(5,5)
      /*
       * Sun C 5.5 and later, so we have __global.
       * (Sun C 5.9 and later also have __attribute__((visibility()),
       * but there's no reason to prefer it with Sun C.)
       */
      #define PCAP_API_DEF	__global
    #else
      /*
       * We don't have anything to say.
       */
      #define PCAP_API_DEF
    #endif
  #else
    /*
     * We're not building libpcap.
     */
    #define PCAP_API_DEF
  #endif
#endif /* _WIN32/MSDOS/UN*X */

#define PCAP_API	PCAP_API_DEF extern

/*
 * Definitions to 1) indicate what version of libpcap first had a given
 * API and 2) allow upstream providers whose build environments allow
 * APIs to be designated as "first available in this release" to do so
 * by appropriately defining them.
 *
 * On macOS, Apple can tweak this to make various APIs "weakly exported
 * symbols" to make it easier for software that's distributed in binary
 * form and that uses libpcap to run on multiple macOS versions and use
 * new APIs when available.  (Yes, such third-party software exists -
 * Wireshark provides binary packages for macOS, for example.  tcpdump
 * doesn't count, as that's provided by Apple, so each release can
 * come with a version compiled to use the APIs present in that release.)
 *
 * We don't tweak it that way ourselves because, if you're building
 * and installing libpcap on macOS yourself, the APIs will be available
 * no matter what OS version you're installing it on.
 *
 * For other platforms, we don't define them, leaving it up to
 * others to do so based on their OS versions, if appropriate.
 *
 * We start with libpcap 0.4, as that was the last LBL release, and
 * I've never seen earlier releases.
 */
#ifdef __APPLE__
/*
 * Apple - insert #include <os/availability.h> here, and replace the two
 * #defines below with:
 *
 *   #define PCAP_API_AVAILABLE	API_AVAILABLE
 *
 * and adjust availabilities as necessary, including adding information
 * about operating systems other than macOS.
 */
#define PCAP_API_AVAILABLE(...)
#define PCAP_AVAILABLE_0_4	PCAP_API_AVAILABLE(macos(10.0))
#define PCAP_AVAILABLE_0_5	PCAP_API_AVAILABLE(macos(10.0))
#define PCAP_AVAILABLE_0_6	PCAP_API_AVAILABLE(macos(10.1))
#define PCAP_AVAILABLE_0_7	PCAP_API_AVAILABLE(macos(10.4))
#define PCAP_AVAILABLE_0_8	PCAP_API_AVAILABLE(macos(10.4))
#define PCAP_AVAILABLE_0_9	PCAP_API_AVAILABLE(macos(10.5))
#define PCAP_AVAILABLE_1_0	PCAP_API_AVAILABLE(macos(10.6))
/* #define PCAP_AVAILABLE_1_1	no routines added to the API */
#define PCAP_AVAILABLE_1_2	PCAP_API_AVAILABLE(macos(10.9))
/* #define PCAP_AVAILABLE_1_3	no routines added to the API */
/* #define PCAP_AVAILABLE_1_4	no routines added to the API */
#define PCAP_AVAILABLE_1_5	PCAP_API_AVAILABLE(macos(10.10))
/* #define PCAP_AVAILABLE_1_6	no routines added to the API */
#define PCAP_AVAILABLE_1_7	PCAP_API_AVAILABLE(macos(10.12))
#define PCAP_AVAILABLE_1_8	PCAP_API_AVAILABLE(macos(10.13))
#define PCAP_AVAILABLE_1_9	PCAP_API_AVAILABLE(macos(10.13))
/*
 * The remote capture APIs are, in 1.9 and 1.10, usually only
 * available in the library if the library was built with
 * remote capture enabled.
 *
 * However, macOS Sonoma provides stub versions of those routine,
 * which return an error.  This means that we need a separate
 * availability indicator macro for those routines, so that
 * progras built on macOS Sonoma that attempt to use weak
 * importing and availability tests to use those routines
 * if they're available will get those routines weakly imported,
 * so that if they're run on releases prior to Sonoma, they
 * won't get an error from dyld about those routines being
 * missing in libpcap.  (If they don't use run-time availability
 * tests, they will, instead, get crashes if they call one of
 * those routines, as the addresses of those routines will be
 * set to 0 by dyld, meaning the program will dereference a
 * null pointer and crash when trying to call them.)
 *
 * (Not that it's useful to use those routines *anyway*, as they're
 * stubs that always fail.  The stubs were necessary in order to
 * support weak exporting at all.)
 */
#define PCAP_AVAILABLE_1_9_REMOTE	PCAP_API_AVAILABLE(macos(14.0))
#define PCAP_AVAILABLE_1_10	PCAP_API_AVAILABLE(macos(12.1))
#define PCAP_AVAILABLE_1_10_REMOTE	PCAP_API_AVAILABLE(macos(14.0))
#define PCAP_AVAILABLE_1_11	/* not released yet, so not in macOS yet */
#else /* __APPLE__ */
#define PCAP_AVAILABLE_0_4
#define PCAP_AVAILABLE_0_5
#define PCAP_AVAILABLE_0_6
#define PCAP_AVAILABLE_0_7
#define PCAP_AVAILABLE_0_8
#define PCAP_AVAILABLE_0_9
#define PCAP_AVAILABLE_1_0
/* #define PCAP_AVAILABLE_1_1	no routines added to the API */
#define PCAP_AVAILABLE_1_2
/* #define PCAP_AVAILABLE_1_3	no routines added to the API */
/* #define PCAP_AVAILABLE_1_4	no routines added to the API */
#define PCAP_AVAILABLE_1_5
/* #define PCAP_AVAILABLE_1_6	no routines added to the API */
#define PCAP_AVAILABLE_1_7
#define PCAP_AVAILABLE_1_8
#define PCAP_AVAILABLE_1_9
#define PCAP_AVAILABLE_1_9_REMOTE
#define PCAP_AVAILABLE_1_10
#define PCAP_AVAILABLE_1_10_REMOTE
#define PCAP_AVAILABLE_1_11
#endif /* __APPLE__ */

/*
 * PCAP_NORETURN, before a function declaration, means "this function
 * never returns".  (It must go before the function declaration, e.g.
 * "extern PCAP_NORETURN func(...)" rather than after the function
 * declaration, as the MSVC version has to go before the declaration.)
 *
 * PCAP_NORETURN_DEF, before a function *definition*, means "this
 * function never returns"; it would be used only for static functions
 * that are defined before any use, and thus have no declaration.
 * (MSVC doesn't support that; I guess the "decl" in "__declspec"
 * means "declaration", and __declspec doesn't work with definitions.)
 */
#if __has_attribute(noreturn) \
    || PCAP_IS_AT_LEAST_GNUC_VERSION(2,5) \
    || PCAP_IS_AT_LEAST_SUNC_VERSION(5,9) \
    || PCAP_IS_AT_LEAST_XL_C_VERSION(7,0) \
    || PCAP_IS_AT_LEAST_HP_C_VERSION(6,10) \
    || __TINYC__
  /*
   * Compiler with support for __attribute((noreturn)), or GCC 2.5 and
   * later, or some compiler asserting compatibility with GCC 2.5 and
   * later, or Solaris Studio 12 (Sun C 5.9) and later, or IBM XL C 7.0
   * and later (do any earlier versions of XL C support this?), or HP aCC
   * A.06.10 and later, or current TinyCC.
   */
  #define PCAP_NORETURN __attribute((noreturn))
  #define PCAP_NORETURN_DEF __attribute((noreturn))
#elif defined(_MSC_VER)
  /*
   * MSVC.
   */
  #define PCAP_NORETURN __declspec(noreturn)
  #define PCAP_NORETURN_DEF
#else
  #define PCAP_NORETURN
  #define PCAP_NORETURN_DEF
#endif

/*
 * PCAP_PRINTFLIKE(x,y), after a function declaration, means "this function
 * does printf-style formatting, with the xth argument being the format
 * string and the yth argument being the first argument for the format
 * string".
 */
#if __has_attribute(__format__) \
    || PCAP_IS_AT_LEAST_GNUC_VERSION(2,3) \
    || PCAP_IS_AT_LEAST_XL_C_VERSION(7,0) \
    || PCAP_IS_AT_LEAST_HP_C_VERSION(6,10)
  /*
   * Compiler with support for it, or GCC 2.3 and later, or some compiler
   * asserting compatibility with GCC 2.3 and later, or IBM XL C 7.0
   * and later (do any earlier versions of XL C support this?),
   * or HP aCC A.06.10 and later.
   */
  #define PCAP_PRINTFLIKE(x,y) __attribute__((__format__(__printf__,x,y)))
#else
  #define PCAP_PRINTFLIKE(x,y)
#endif

/*
 * PCAP_DEPRECATED(func, msg), after a function declaration, marks the
 * function as deprecated.
 *
 * The argument is a string giving the warning message to use if the
 * compiler supports that.
 */
#if __has_attribute(deprecated) \
    || PCAP_IS_AT_LEAST_GNUC_VERSION(4,5) \
    || PCAP_IS_AT_LEAST_SUNC_VERSION(5,13)
  /*
   * Compiler that supports __has_attribute and __attribute__((deprecated)),
   * or GCC 4.5 and later, or Sun/Oracle C 12.4 (Sun C 5.13) and later.
   *
   * Those support __attribute__((deprecated(msg))) (we assume, perhaps
   * incorrectly, that anything that supports __has_attribute() is
   * recent enough to support __attribute__((deprecated(msg)))).
   */
  #define PCAP_DEPRECATED(msg)	__attribute__((deprecated(msg)))
#elif PCAP_IS_AT_LEAST_GNUC_VERSION(3,1)
  /*
   * GCC 3.1 through 4.4.
   *
   * Those support __attribute__((deprecated)) but not
   * __attribute__((deprecated(msg))).
   */
  #define PCAP_DEPRECATED(msg)	__attribute__((deprecated))
#elif defined(_MSC_VER) && !defined(BUILDING_PCAP)
  /*
   * MSVC, and we're not building libpcap itself; it's VS 2015
   * and later, so we have __declspec(deprecated(...)).
   *
   * If we *are* building libpcap, we don't want this, as it'll warn
   * us even if we *define* the function.
   */
  #define PCAP_DEPRECATED(msg)	_declspec(deprecated(msg))
#else
  #define PCAP_DEPRECATED(msg)
#endif

/*
 * For flagging arguments as format strings in MSVC.
 */
#ifdef _MSC_VER
 #include <sal.h>
 #define PCAP_FORMAT_STRING(p) _Printf_format_string_ p
#else
 #define PCAP_FORMAT_STRING(p) p
#endif

#endif /* lib_pcap_funcattrs_h */
