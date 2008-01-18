

echo '//
//	ipc.i	WJ103
//

%module zfsd_status


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

#include  "zfsd/dbus-service-descriptors.h"
#include  "zfsd_status.h"

%}

// Produce constants and helper functions for structures and unions
' >>$1


echo '
%include "zfsd/dbus-service-descriptors.h"
%include "zfsd_status.h"
%inline
%{
%}

// EOB' >> $1
