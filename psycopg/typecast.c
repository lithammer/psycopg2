/* typecast.c - basic utility functions related to typecasting
 *
 * Copyright (C) 2003 Federico Di Gregorio <fog@debian.org>
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

#include <Python.h>
#include <structmember.h>

#define PSYCOPG_MODULE
#include "psycopg/config.h"
#include "psycopg/psycopg.h"
#include "psycopg/python.h"
#include "psycopg/typecast.h"
#include "psycopg/cursor.h"

/* usefull function used by some typecasters */

static char *
skip_until_space(char *s)
{
    while (*s && *s != ' ') s++;
    return s;
}


/** include casting objects **/
#include "psycopg/typecast_basic.c"

#ifdef HAVE_MXDATETIME
#include "psycopg/typecast_mxdatetime.c"
#endif

#ifdef HAVE_PYDATETIME
#include "psycopg/typecast_datetime.c"
#endif

#include "psycopg/typecast_builtins.c"

/* a list of initializers, used to make the typecasters accessible anyway */
#ifdef HAVE_PYDATETIME
typecastObject_initlist typecast_pydatetime[] = {
    {"PYDATETIME", typecast_DATETIME_types, typecast_PYDATETIME_cast},
    {"PYTIME", typecast_TIME_types, typecast_PYTIME_cast},
    {"PYDATE", typecast_DATE_types, typecast_PYDATE_cast},
    {"PYINTERVAL", typecast_INTERVAL_types, typecast_PYINTERVAL_cast},    
    {NULL, NULL, NULL}
};
#endif

/* a list of initializers, used to make the typecasters accessible anyway */
#ifdef HAVE_MXDATETIME
typecastObject_initlist typecast_mxdatetime[] = {
    {"MXDATETIME", typecast_DATETIME_types, typecast_MXDATE_cast},
    {"MXTIME", typecast_TIME_types, typecast_MXTIME_cast},
    {"MXDATE", typecast_DATE_types, typecast_MXDATE_cast},
    {"MXINTERVAL", typecast_INTERVAL_types, typecast_MXINTERVAL_cast},    
    {NULL, NULL, NULL}
};
#endif


/** the type dictionary and associated functions **/

PyObject *psyco_types;
PyObject *psyco_default_cast;
PyObject *psyco_binary_types;
PyObject *psyco_default_binary_cast;

static long int typecast_default_DEFAULT[] = {0};
static typecastObject_initlist typecast_default = {
    "DEFAULT", typecast_default_DEFAULT, typecast_STRING_cast};


/* typecast_init - initialize the dictionary and create default types */

int
typecast_init(PyObject *dict)
{
    int i;
    
    /* create type dictionary and put it in module namespace */
    psyco_types = PyDict_New();
    psyco_binary_types = PyDict_New();
    
    if (!psyco_types || !psyco_binary_types) {
        Py_XDECREF(psyco_types);
        Py_XDECREF(psyco_binary_types);
        return -1;
    }                         

    PyDict_SetItemString(dict, "string_types", psyco_types);
    PyDict_SetItemString(dict, "binary_types", psyco_binary_types);
    
    /* insert the cast types into the 'types' dictionary and register them in
       the module dictionary */
    for (i = 0; typecast_builtins[i].name != NULL; i++) {
        typecastObject *t;

        Dprintf("typecast_init: initializing %s", typecast_builtins[i].name);

        t = (typecastObject *)typecast_from_c(&(typecast_builtins[i]));
        if (t == NULL) return -1;
        if (typecast_add((PyObject *)t, 0) != 0) return -1;

        PyDict_SetItem(dict, t->name, (PyObject *)t);

        /* export binary object */
        if (typecast_builtins[i].values == typecast_BINARY_types) {
            psyco_default_binary_cast = (PyObject *)t;
        }
    }
    
    /* create and save a default cast object (but does not register it) */
    psyco_default_cast = typecast_from_c(&typecast_default);

    /* register the date/time typecasters with their original names */
#ifdef HAVE_MXDATETIME
    for (i = 0; typecast_mxdatetime[i].name != NULL; i++) {
        typecastObject *t;
        Dprintf("typecast_init: initializing %s", typecast_mxdatetime[i].name);
        t = (typecastObject *)typecast_from_c(&(typecast_mxdatetime[i]));
        if (t == NULL) return -1;
        PyDict_SetItem(dict, t->name, (PyObject *)t);
    }
#endif
#ifdef HAVE_PYDATETIME
    for (i = 0; typecast_pydatetime[i].name != NULL; i++) {
        typecastObject *t;
        Dprintf("typecast_init: initializing %s", typecast_pydatetime[i].name);
        t = (typecastObject *)typecast_from_c(&(typecast_pydatetime[i]));
        if (t == NULL) return -1;
        PyDict_SetItem(dict, t->name, (PyObject *)t);
    }
#endif
    
    return 0;
}


/* typecast_add - add a type object to the dictionary */
int
typecast_add(PyObject *obj, int binary)
{
    PyObject *val;
    int len, i;

    typecastObject *type = (typecastObject *)obj;
    
    Dprintf("typecast_add: object at %p, values refcnt = %d",
            obj, type->values->ob_refcnt);

    len = PyTuple_Size(type->values);
    for (i = 0; i < len; i++) {
        val = PyTuple_GetItem(type->values, i);
        Dprintf("typecast_add:     adding val: %ld", PyInt_AsLong(val));
        if (binary) {
            PyDict_SetItem(psyco_binary_types, val, obj);
        }
        else {
            PyDict_SetItem(psyco_types, val, obj);
        }
    }

    return 0;
}


/** typecast type **/

#define OFFSETOF(x) offsetof(typecastObject, x)

static struct memberlist typecastObject_memberlist[] = {
    {"name", T_OBJECT, OFFSETOF(name), RO},
    {"values", T_OBJECT, OFFSETOF(values), RO},
    {NULL}
};

/* numeric methods */

static PyObject *
typecast_new(PyObject *name, PyObject *values, PyObject *cast);
     
static int
typecast_coerce(PyObject **pv, PyObject **pw)
{    
    if (PyObject_TypeCheck(*pv, &typecastType)) {
        if (PyInt_Check(*pw)) {
            PyObject *coer, *args;
            args = PyTuple_New(1);
            Py_INCREF(*pw);
            PyTuple_SET_ITEM(args, 0, *pw);
            coer = typecast_new(NULL, args, NULL);
            *pw = coer;
            Py_DECREF(args);
            Py_INCREF(*pv);
            return 0;
        }
        else if (PyObject_TypeCheck(*pw, &typecastType)){
            Py_INCREF(*pv);
            Py_INCREF(*pw);
            return 0;
        }
    }
    PyErr_SetString(PyExc_TypeError, "psycopg type coercion failed");
    return -1;
}

static PyNumberMethods typecastObject_as_number = {
    0, 	/*nb_add*/
    0,	/*nb_subtract*/
    0,	/*nb_multiply*/
    0,	/*nb_divide*/
    0,	/*nb_remainder*/
    0,	/*nb_divmod*/
    0,	/*nb_power*/
    0,	/*nb_negative*/
    0,	/*nb_positive*/
    0,	/*nb_absolute*/
    0,  /*nb_nonzero*/
    0,	/*nb_invert*/
    0,	/*nb_lshift*/
    0,	/*nb_rshift*/
    0,	/*nb_and*/
    0,	/*nb_xor*/
    0,	/*nb_or*/
    typecast_coerce, /*nb_coerce*/
    0,	/*nb_int*/
    0,	/*nb_long*/
    0,	/*nb_float*/
    0,	/*nb_oct*/
    0,	/*nb_hex*/
};


/* object methods */

static int
typecast_cmp(typecastObject *self, typecastObject *v)
{
    int res;

    if (PyObject_Length(v->values) > 1 && PyObject_Length(self->values) == 1) {
        /* calls itself exchanging the args */
        return typecast_cmp(v, self);
    }
    res = PySequence_Contains(self->values, PyTuple_GET_ITEM(v->values, 0));
    
    if (res < 0) return res;
    else return res == 1 ? 0 : 1;
}

static struct PyMethodDef typecastObject_methods[] = {
    {"__cmp__", (PyCFunction)typecast_cmp, METH_VARARGS, NULL},
    {NULL, NULL}
};

/** FIXME: typecast should become a new-style type sometime in the future, but
    right now this is not important and we keep going with old class */

static PyObject *
typecast_getattr(typecastObject *self, char *name)
{
    PyObject *rv;
	
    rv = PyMember_Get((char *)self, typecastObject_memberlist, name);
    if (rv) return rv;
    PyErr_Clear();
    return Py_FindMethod(typecastObject_methods, (PyObject *)self, name);
}

static void
typecast_destroy(typecastObject *self)
{
    PyObject *name, *cast, *values;

    values = self->values;
    name = self->name;
    cast = self->pcast;

    PyObject_Del(self);

    Py_XDECREF(name);
    Py_XDECREF(values);
    Py_XDECREF(cast);

    Dprintf("typecast_destroy: object at %p destroyed", self);
}

static PyObject *
typecast_call(PyObject *obj, PyObject *args, PyObject *kwargs)
{   
    PyObject *string, *cursor, *res;
    
    typecastObject *self = (typecastObject *)obj;
    
    if (!PyArg_ParseTuple(args, "OO", &string, &cursor)) {
        return NULL;
    }

    if (self->ccast) {
        Dprintf("typecast_call: calling C cast function");
        res = self->ccast(string, cursor);
    }
    else if (self->pcast) {
        Dprintf("typecast_call: calling python callable");
        Py_INCREF(string);
        res = PyObject_CallFunction(self->pcast, "OO", string, cursor);
    }
    else {
        Py_INCREF(Py_None);
        res = Py_None;
    }

    Dprintf("typecast_call: string argument has refcnt = %d",
            string->ob_refcnt);
    return res;
}


PyTypeObject typecastType = {
    PyObject_HEAD_INIT(NULL)

    0, /*ob_size*/
    "psycopg.type", /*tp_name*/
    sizeof(typecastObject), /*tp_basicsize*/
    0,    /*tp_itemsize*/

    /* methods */
    (destructor)typecast_destroy, /*tp_dealloc*/
    0,    /*tp_print*/
    (getattrfunc)typecast_getattr, /*tp_getattr*/
    0,    /*tp_setattr*/
    (cmpfunc)typecast_cmp, /*tp_compare*/
    0,    /*tp_repr*/
    &typecastObject_as_number, /*tp_as_number*/
    0,    /*tp_as_sequence*/
    0,    /*tp_as_mapping*/
    0,    /*tp_hash*/
    typecast_call, /*tp_call*/
    0,    /*tp_str*/
    
    /* Space for future expansion */
    0L,0L,0L,0L,
    "psycopg type-casting object"  /* Documentation string */
};

static PyObject *
typecast_new(PyObject *name, PyObject *values, PyObject *cast)
{
    typecastObject *obj;

    obj = PyObject_NEW(typecastObject, &typecastType);
    if (obj == NULL) return NULL;

    Py_INCREF(values);
    obj->values = values;
    
    if (name) {
        Py_INCREF(name);
        obj->name = name;
    }
    else {
        Py_INCREF(Py_None);
        obj->name = Py_None;
    }

    obj->pcast = NULL;
    obj->ccast = NULL;
    
    if (cast && cast != Py_None) {
        Py_INCREF(cast);
        obj->pcast = cast;
    }
    
    Dprintf("typecast_new: typecast object created at %p", obj);
    
    return (PyObject *)obj;
}

PyObject *
typecast_from_python(PyObject *self, PyObject *args, PyObject *keywds)
{
    PyObject *v, *name, *cast = NULL;

    static char *kwlist[] = {"values", "name", "castobj", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!|O!O", kwlist, 
                                     &PyTuple_Type, &v,
                                     &PyString_Type, &name,
                                     &cast)) {
        return NULL;
    }
    
    return typecast_new(name, v, cast);
}

PyObject *
typecast_from_c(typecastObject_initlist *type)
{
    PyObject *tuple;
    typecastObject *obj;
    int i, len = 0;

    while (type->values[len] != 0) len++;
    
    tuple = PyTuple_New(len);
    if (!tuple) return NULL;
    
    for (i = 0; i < len ; i++) {
        PyTuple_SET_ITEM(tuple, i, PyInt_FromLong(type->values[i]));
    }
    
    obj = (typecastObject *)
        typecast_new(PyString_FromString(type->name), tuple, NULL);
    
    if (obj) {
        obj->ccast = type->cast;
        obj->pcast = NULL;
    }
    return (PyObject *)obj;
}

