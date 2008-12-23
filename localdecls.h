/*
 * File: localdecls.h
 *
 * Copyright 1999-2004 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */

#ifndef __LOCALDECLS_H__
#define __LOCALDECLS_H__

/* take care of broken ld loading */
#if defined(__GLIBC__) && !defined(__UCLIBC__)

# if __GLIBC__ <= 2 && __GLIBC_MINOR__ <= 2
#  define BROKEN_RTLD_NEXT
#  define LIBC 5
# endif

# if !defined(BROKEN_RTLD_NEXT)
#  if defined(__mips__)
#   define BROKEN_RTLD_NEXT
#  endif
# endif

#else

#if 0
# if defined(__FreeBSD__)
#  define BROKEN_RTLD_NEXT
# endif
#endif

#endif

#define GLIBC_MINOR __GLIBC_MINOR__

#ifdef PATH_MAX
# define SB_PATH_MAX PATH_MAX * 2
# if (SB_PATH_MAX >= INT_MAX) || (SB_PATH_MAX < PATH_MAX)
#  undef SB_PATH_MAX
#  define SB_PATH_MAX PATH_MAX + 25
#  if (SB_PATH_MAX >= INT_MAX) || (SB_PATH_MAX < PATH_MAX)
#   error SB_PATH_MAX too big!
#  endif
# endif
#else
# error PATH_MAX not defined!
#endif

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

/* If the system is old and does not support *at funcs, then define
 * it ourself.  Shouldn't matter based on how we use it.
 */
#ifndef AT_FDCWD
# define AT_FDCWD -100
#endif

#if !HAVE_DLVSYM
# define dlvsym(_lib, _sym, _ver) dlsym(_lib, _sym)
#endif

#if HAVE_DLVSYM
/* Taken from glibc */
# define symbol_version(_real, _name, _version) \
	__asm__ (".symver " #_real "," #_name "@" #_version)
# define default_symbol_version(_real, _name, _version) \
	__asm__ (".symver " #_real "," #_name "@@" #_version)
#endif

/* Taken from glibc */
# define strong_alias(_name, _aliasname) \
	extern __typeof (_name) _aliasname __attribute__ ((alias (#_name)));
# define weak_alias(_name, _aliasname) \
	extern __typeof (_name) _aliasname __attribute__ ((weak, alias (#_name)));


#endif
