/* sc1_gdb.h - SiCortex 1 gdb remote stub

   Copyright (c) 2005, SiCortex, Inc.  All rights reserved.

   Copyright (c) 2003-2005, Joel Martin

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

*/

#ifndef _SC1_GDB_H_
#define _SC1_GDB_H_    0

#if !defined (_WIN32)
#include <sys/poll.h>
#include "simple_socket.h"

/* 
 * SIMH data 
 */

#define GDB_POLL_INTVL 10000
#define BUFMAX 8192

extern UNIT gdb_unit[];
extern DEVICE gdb_dev;
extern struct simple_socket_priv gdb_socket_priv;

t_stat gdb_stub(int cpu);

#endif
#endif
