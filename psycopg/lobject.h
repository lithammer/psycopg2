/* lobject.h - definition for the psycopg lobject type
 *
 * Copyright (C) 2006 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PSYCOPG_LOBJECT_H
#define PSYCOPG_LOBJECT_H 1

#include <Python.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>

#include "psycopg/connection.h"

#ifdef __cplusplus
extern "C" {
#endif

extern PyTypeObject lobjectType;

typedef struct {
    PyObject HEAD;

    connectionObject *conn; /* connection owning the lobject */

    int closed;              /* 1 if the lobject is closed */
    int mode;                /* numeric mode, tells if lobject was opened */
    char *smode;             /* string mode if lobject was opened */

    long int mark;           /* copied from conn->mark */

    Oid oid;                 /* the oid for this lobject */
    int fd;                  /* the file descriptor for file-like ops */
} lobjectObject;
    
/* functions exported from lobject_int.c */

extern int lobject_open(lobjectObject *self, connectionObject *conn,
                         Oid oid, int mode, Oid new_oid, char *new_file);
extern int lobject_unlink(lobjectObject *self);
extern int lobject_export(lobjectObject *self, char *filename);

extern size_t lobject_read(lobjectObject *self, char *buf, size_t len);
extern size_t lobject_write(lobjectObject *self, char *buf, size_t len);
extern int lobject_seek(lobjectObject *self, int pos, int whence);
extern int lobject_tell(lobjectObject *self);
extern void lobject_close(lobjectObject *self);

/* exception-raising macros */

#define EXC_IF_LOBJ_CLOSED(self) \
if ((self)->closed || ((self)->conn && (self)->conn->closed)) { \
    PyErr_SetString(InterfaceError, "lobject already closed");  \
    return NULL; }

#define EXC_IF_LOBJ_LEVEL0(self) \
if (self->conn->isolation_level == 0) {                             \
    psyco_set_error(ProgrammingError, (PyObject*)self,              \
        "can't use a lobject outside of transactions", NULL, NULL); \
    return NULL;                                                    \
}
#define EXC_IF_LOBJ_UNMARKED(self) \
if (self->conn->mark != self->mark) {                  \
    psyco_set_error(ProgrammingError, (PyObject*)self, \
        "lobject isn't valid anymore", NULL, NULL);    \
    return NULL;                                       \
}

#ifdef __cplusplus
}
#endif

#endif /* !defined(PSYCOPG_LOBJECT_H) */
