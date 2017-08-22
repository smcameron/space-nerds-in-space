/*
	Copyright (C) 2014 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
	*/

/*
 * Link this in to make all calls to setlocale do setlocale(LC_ALL, "C");
 * regardless of what parameters are passed. We do this so that the scanf()
 * family behaves sanely.  Otherwise we would have to ship localized
 * variants of our own data files (e.g. wavefront obj files for 3d models)
 * to use commas (',') in floating point numbers rather than the
 * usual periods ('.') to represent the decimal point for some locales,
 * and write some conversion utility to modify the output of e.g. blender
 * to "fix" the files for these locales, which would be completely insane.
 * Instead, we suppress this locale insanity by forcing the locale to
 * always be 'C'.
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <locale.h>

typedef char *(*setlocale_prototype)(int category, const char *locale);

static setlocale_prototype real_setlocale = NULL;

static char *the_only_locale = "C";

char *setlocale(__attribute__((unused)) int category,
		__attribute__((unused)) const char *locale)
{
	char *msg;

	/* printf("setlocale intercepted, using locale of 'C'\n"); */
	if (!real_setlocale) {

		/* To explain the cast of the lvalue "real_setlocale" used to store
		 * the (void *) return value of dlsym() below, here is a quote from the
		 * dlopen(3) man page:
		 *
		 *	cosine = (double (*)(double)) dlsym(handle, "cos");
		 *
		 * [...]
		 *
		 *	According to the ISO C standard, casting between function
		 *	pointers and 'void *', as done above, produces undefined results.
		 *	POSIX.1-2003 and POSIX.1-2008 accepted this state of affairs and
		 *	proposed the following workaround:
		 *
		 *	*(void **) (&cosine) = dlsym(handle, "cos");
		 *
		 *	This (clumsy) cast conforms with the ISO C standard and will
		 *	avoid any compiler warnings.
		 *
		 *	The 2013 Technical Corrigendum to POSIX.1-2008 (a.k.a.
		 *	POSIX.1-2013) improved matters by requiring that conforming
		 *	implementations support casting 'void *' to a function pointer.
		 *	Nevertheless, some compilers (e.g., gcc with the '-pedantic'
		 *	option) may complain about the cast used in this program.
		 *
		 * Previously, I had used the following to avoid GCC diagnostic warnings:
		 *
		 *	#pragma GCC diagnostic push
		 *	#pragma GCC diagnostic ignored "-Wpedantic"
		 *	real_setlocale = (setlocale_prototype) dlsym(RTLD_NEXT, "setlocale");
		 *	#pragma GCC diagnostic ignored "-Wpedantic"
		 *
		 */
		*(void **) &real_setlocale = dlsym(RTLD_NEXT, "setlocale");
		msg = dlerror();
		if (msg) {
			fprintf(stderr, "Failed to override setlocale(): %s\n", msg);
			fflush(stderr);
			real_setlocale = NULL;
			return the_only_locale;
		}
	}
	/* C is the locale, and the locale shall be C */
	return real_setlocale(LC_ALL, "C");
}

