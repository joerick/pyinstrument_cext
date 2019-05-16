#include <Python.h>
#include <structmember.h>
#include <frameobject.h>

////////////////////////////
// Version/Platform shims //
////////////////////////////

/* Python 2 shim */
#if PY_MAJOR_VERSION < 3

#define PyUnicode_InternFromString PyString_InternFromString

#endif

/*
These timer functions are mostly stolen from timemodule.c
*/

#if defined(MS_WINDOWS) && !defined(__BORLANDC__)
#include <windows.h>

/* use QueryPerformanceCounter on Windows */

static double
floatclock(void)
{
    static LARGE_INTEGER ctrStart;
    static double divisor = 0.0;
    LARGE_INTEGER now;
    double diff;

    if (divisor == 0.0) {
        LARGE_INTEGER freq;
        QueryPerformanceCounter(&ctrStart);
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
            /* Unlikely to happen - this works on all intel
               machines at least!  Revert to clock() */
            return ((double)clock()) / CLOCKS_PER_SEC;
        }
        divisor = (double)freq.QuadPart;
    }
    QueryPerformanceCounter(&now);
    diff = (double)(now.QuadPart - ctrStart.QuadPart);
    return diff / divisor;
}

#else  /* !MS_WINDOWS */

#include <sys/time.h>

static double
floatclock(void)
{
    struct timeval t;
    gettimeofday(&t, (struct timezone *)NULL);

    return (double)t.tv_sec + t.tv_usec*0.000001;
}

#endif  /* MS_WINDOWS */

///////////////////
// ProfilerState //
///////////////////

typedef struct profiler_state {
    PyObject_HEAD
    PyObject *target;
    double interval;
    double last_invocation;
} ProfilerState;

static void ProfilerState_SetTarget(ProfilerState *self, PyObject *target) {
    PyObject *tmp = self->target;
    Py_XINCREF(target);
    self->target = target;
    Py_XDECREF(tmp);
}

static void ProfilerState_Dealloc(ProfilerState *self) {
    ProfilerState_SetTarget(self, NULL);
    Py_TYPE(self)->tp_free(self);
}

static PyTypeObject ProfilerState_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pyinstrument_cext.ProfilerState",        /* tp_name */
    sizeof(ProfilerState),                    /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)ProfilerState_Dealloc,        /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_reserved */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    0,                                        /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    0,                                        /* tp_init */
    PyType_GenericAlloc,                      /* tp_alloc */
    PyType_GenericNew,                        /* tp_new */
    PyObject_Del,                             /* tp_free */
};

static ProfilerState *ProfilerState_New(void) {
    ProfilerState *op = PyObject_New(ProfilerState, &ProfilerState_Type);
    op->target = NULL;
    op->interval = 0.0;
    op->last_invocation = 0.0;
    return op;
}

////////////////////////
// Internal functions //
////////////////////////

static PyObject *whatstrings[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static int
trace_init(void)
{
    static char *whatnames[7] = {"call", "exception", "line", "return",
                                 "c_call", "c_exception", "c_return"};
    PyObject *name;
    int i;
    for (i = 0; i < 7; ++i) {
        if (whatstrings[i] == NULL) {
            name = PyUnicode_InternFromString(whatnames[i]);
            if (name == NULL)
                return -1;
            whatstrings[i] = name;
        }
    }
    return 0;
}

static PyObject *
call_target(ProfilerState *pState, PyFrameObject *frame, int what, PyObject *arg)
{
    PyObject *args;
    PyObject *whatstr;
    PyObject *result;

    if (arg == NULL)
        arg = Py_None;

    args = PyTuple_New(3);
    if (args == NULL)
        return NULL;

    PyFrame_FastToLocals(frame);

    Py_INCREF(frame);
    whatstr = whatstrings[what];
    Py_INCREF(whatstr);
    if (arg == NULL)
        arg = Py_None;
    Py_INCREF(arg);
    PyTuple_SET_ITEM(args, 0, (PyObject *)frame);
    PyTuple_SET_ITEM(args, 1, whatstr);
    PyTuple_SET_ITEM(args, 2, arg);

    /* call the Python-level target function */
    result = PyEval_CallObject(pState->target, args);

    PyFrame_LocalsToFast(frame, 1);
    if (result == NULL)
        PyTraceBack_Here(frame);

    /* cleanup */
    Py_DECREF(args);
    return result;
}

//////////////////////
// Public functions //
//////////////////////

/**
 * The profile function. Passed to PyEval_SetProfile, and called with 
 * function frames as the program executes by Python 
 */
static int
profile(PyObject *op, PyFrameObject *frame, int what, PyObject *arg)
{
    double now = floatclock();
    ProfilerState *pState = (ProfilerState *)op;
    PyObject *result;

    if (now < pState->last_invocation + pState->interval) {
        return 0;
    } else {
        pState->last_invocation = now;
    }

    result = call_target(pState, frame, what, arg);

    if (result == NULL) {
        PyEval_SetProfile(NULL, NULL);
        return -1;
    }

    Py_DECREF(result);
    return 0;
}

/**
 * The 'setprofile' function. This is the public API that can be called
 * from Python code.
 */
static PyObject *
setstatprofile(PyObject *m, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"target", "interval", NULL};
    ProfilerState *pState = NULL;
    double interval = 0.0;
    PyObject *target = NULL;
    
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O|d", kwlist, &target, &interval))
        return NULL;

    if (target == Py_None) 
        target = NULL;

    if (target) {
        if (!PyCallable_Check(target)) {
            PyErr_SetString(PyExc_TypeError, "target must be callable");
            return NULL;
        }

        if (trace_init() == -1)
            return NULL;
        
        pState = ProfilerState_New();
        ProfilerState_SetTarget(pState, target);

        // default interval is 1 ms
        pState->interval = (interval > 0) ? interval : 0.001;

        // initialise the last invocation to avoid immediate callback
        pState->last_invocation = floatclock();

        PyEval_SetProfile(profile, (PyObject *)pState);
        Py_DECREF(pState);
    } else {
        PyEval_SetProfile(NULL, NULL);
    }

    Py_RETURN_NONE;
}

///////////////////////////
// Module initialization //
///////////////////////////

#if PY_MAJOR_VERSION >= 3
    #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
    #define MOD_DEF(m, name, doc, methods, module_state_size) \
        static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, module_state_size, methods, }; \
        m = PyModule_Create(&moduledef);
    #define MOD_RETURN(m) return m;
#else
    #define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
    #define MOD_DEF(m, name, doc, methods, module_state_size) \
        m = Py_InitModule3(name, methods, doc);
    #define MOD_RETURN(m) return;
#endif

static PyMethodDef module_methods[] = {
    {"setstatprofile", (PyCFunction)setstatprofile, METH_VARARGS | METH_KEYWORDS, 
     "Sets the statistal profiler callback. The function in the same manner as setprofile, but "
     "instead of being called every on every call and return, the function is called every "
     "<interval> seconds with the current stack."},
    {NULL}  /* Sentinel */
};

MOD_INIT(pyinstrument_cext)
{
    PyObject* m;

    MOD_DEF(m, 
            "pyinstrument_cext", 
            "Module that implements the backend to a statistical profiler", 
            module_methods,
            0)

    MOD_RETURN(m)
}
