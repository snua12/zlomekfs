#!/bin/bash

# generate_swig_def.sh
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of syplog
#
# syplog is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# syplog is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with syplog; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

if [ -z "$PROJECT_ROOT"]; then
  if [ -d "../src" ]; then
    INCLUDE_DIR="`pwd`/../src"
  else
    INCLUDE_DIR="/usr/include/syplog"
  fi
else
  INCLUDE_DIR="$(PROJECT_ROOT)/src"
fi

echo '//
//	ipc.i	WJ103
//

%module pysyplog


// This tells SWIG to treat char ** as a special case
%typemap(in) char ** {
  /* Check if is a list */
  if (PyList_Check($input)) {
    int size = PyList_Size($input);
    int i = 0;
    $1 = (char **) malloc((size+1)*sizeof(char *));
    for (i = 0; i < size; i++) {
      PyObject *o = PyList_GetItem($input,i);
      if (PyString_Check(o))
	$1[i] = PyString_AsString(PyList_GetItem($input,i));
      else {
	PyErr_SetString(PyExc_TypeError,"list must contain strings");
	free($1);
	return NULL;
      }
    }
    $1[i] = 0;
  } else {
    PyErr_SetString(PyExc_TypeError,"not a list");
    return NULL;
  }
}

// This cleans up the char ** array we mallocd before the function call
%typemap(freearg) char ** {
  free((char *) $1);
}

%include "stdint.i"

%{

' > $1

find ${INCLUDE_DIR} -iname '*.h' | while read file; do echo '#include "'$file'"'; done >> $1

echo '
%}

// Produce constants and helper functions for structures and unions
' >>$1

find ${INCLUDE_DIR} -iname '*.h' | while read file; do echo '%include "'$file'"'; done >> $1

echo '

%inline
%{
%}

// EOB' >> $1
