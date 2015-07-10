// $Id$ -*- C++ -*-
//======================================================================
//
// Copyright 2006-2008 by Wilson Snyder <wsnyder@wsnyder.org>.  This
// program is free software; you can redistribute it and/or modify it under
// the terms of either the GNU Lesser General Public License or the Perl
// Artistic License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//======================================================================
///
/// \file
/// \brief Vregs: Common defines used by all _defs.h files
///
/// AUTHOR:  Wilson Snyder
///
/// Note this file must be valid for C++, MSVC++, C, and assembler.
///
//======================================================================

#ifndef _VREGSDEFS_H_
#define _VREGSDEFS_H_

//======================================================================
// Defines

// Are we running in an assembler?
#ifndef VREGS_ASSEMBLER
# if (!defined(__cplusplus) && !defined(__STDC__) && !defined(_MSC_VER) && !defined(__GNUC__)) \
    || defined(__ASSEMBLY__) || defined(__ASSEMBLER__)
#  define VREGS_ASSEMBLER 1		///< Defined if running in a assembler, not C or C++
# endif
#endif

#ifndef VREGS_ULL
# if defined(_MSC_VER)
#  define VREGS_ULL(c) (c##ui64)	///< Add appropriate suffix to 64-bit constant
# elif defined(VREGS_ASSEMBLER)
#  define VREGS_ULL(c) (c)		///< Add appropriate suffix to 64-bit constant
# else  // Generic C/C++
#  define VREGS_ULL(c) (c##ULL)		///< Add appropriate suffix to 64-bit constant
# endif
#endif

//======================================================================

#endif //guard
