
/*******************************************************************************/
/* Copyright (C) 2008 Jonathan Moore Liles                                     */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/

/* debug.h
 *
 * 11/21/2003 - Jonathan Moore Liles
 *
 * Debuging support.
 *
 * Disable by defining the preprocessor variable NDEBUG prior to inclusion.
 *
 * The following macros sould be defined as string literals
 *
 * 	name			value
 *
 * 	__MODULE__		Name of module. eg. "libfoo"
 *
 * 	__FILE__		Name of file. eg. "foo.c"
 *
 *	__FUNCTION__		Name of enclosing function. eg. "bar"
 *
 * 	(inteter literal)
 * 	__LINE__		Number of enclosing line.
 *
 *
 * __FILE__, and __LINE__ are automatically defined by standard CPP
 * implementations. __FUNCTION__ is more or less unique to GNU, and isn't
 * strictly a preprocessor macro, but rather a reserved word in the compiler.
 * There is a sed script available with this toolset that is able to fake
 * __FUNCTION__ (among other things) with an extra preprocesessing step.
 *
 * __MODULE__ is nonstandard and should be defined the enclosing program(s).
 * Autoconf defines PACKAGE as the module name, and these routines will use its
 * value instead if __MODULE__ is undefined.
 *
 * The following routines are provided (as macros) and take the same arguments
 * as printf():
 *
 * MESSAGE( const char *format, ... )
 * WARNING( const char *format, ... )
 * ASSERTION( const char *format, ... )
 *
 * Calling MESSAGE or WARNING prints the message to stderr along with module,
 * file and line information, as well as appropriate emphasis. Calling
 * ASSERTION will do the same, and then call abort() to end the program. It is
 * unwise to supply any of these marcros with arguments that produce side
 * effects. As, doing so will most likely result in Heisenbugs; program
 * behavior that changes when debugging is disabled.
 *
 */


#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef __MODULE__
#ifdef PACKAGE
#define __MODULE__ PACKAGE
#else
#define __MODULE__ NULL
#endif
#endif

#ifndef __GNUC__
	#define __FUNCTION__ NULL
#endif

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

typedef enum {
	W_MESSAGE = 0,
	W_WARNING,
	W_ASSERTION
} warning_t;

void
warnf ( warning_t level,
	   const char *module,
	   const char *file,
        const char *function, size_t line, const char *fmt, ... );


#ifndef NDEBUG
#define DMESSAGE( fmt, args... ) warnf( W_MESSAGE, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args )
#define DWARNING( fmt, args... ) warnf( W_WARNING, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args )
#define ASSERT( pred, fmt, args... ) do { if ( ! (pred) ) { warnf( W_ASSERTION, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args ); abort(); } } while ( 0 )
#else
#define DMESSAGE( fmt, args... )
#define DWARNING( fmt, args... )
#define ASSERT( pred, fmt, args... )
#endif

/* these are always defined */
#define MESSAGE( fmt, args... ) warnf( W_MESSAGE, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args )
#define WARNING( fmt, args... ) warnf( W_WARNING, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args )
#define ASSERTION( fmt, args... ) ( warnf( W_ASSERTION, __MODULE__, __FILE__, __FUNCTION__, __LINE__, fmt, ## args ), abort() )

#endif
