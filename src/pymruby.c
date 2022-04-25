#include "pymruby.h"

#ifdef _WIN32
  #define _AMD64_
  #include <threadpoollegacyapiset.h>
  #pragma comment(lib,"ws2_32.lib")
#endif

// void mrb_exc_set(mrb_state *mrb, mrb_value exc);
// PyMODINIT_FUNC PyInit_pymruby(void);

static mrb_value timeout_error;
static int borked = 0;
static size_t mem = 0;

static void timeout(int sig) {
  borked = 1;
  printf("Timeout!\n");
}

#ifdef _WIN32
void CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired){
  if (lpParam){
    int param=*(int*)lpParam;
    // printf("Timeout. Parameter is %d.\n", param);
    timeout(param);
  }
}
#endif

//struct mrb_state *mrb, void*, size_t, void *ud
void *counting_alloc(struct mrb_state *mrb, void *p, size_t size, void *ud) {
  mem += size;
  return mrb_default_allocf(mrb, p, size, ud);
}

// void (*code_fetch_hook)(struct mrb_state* mrb, const struct mrb_irep *irep, const mrb_code *pc, mrb_value *regs);
void hook(struct mrb_state* mrb,const struct mrb_irep *irep,const mrb_code *pc, mrb_value *regs) {
  if (borked) {
    if (borked == 1) {
      borked++;
      // mrb_exc_set(mrb, timeout_error);
      // mrb_raise(mrb, mrb->eException_class, "Execution Timeout");
      mrb_raise(mrb, E_RUNTIME_ERROR, "Execution Timeout");
    }
    else if (borked % 20==0) {
      borked++;
      // if ((*pc & 0x7f) == OP_JMPIF) {
      //   *pc = OP_STOP;
      // }
      // mrb_raise(mrb, E_RUNTIME_ERROR, "Execution Timeout");
      mrb_raise(mrb, mrb->eException_class, "Execution Timeout");
    }
    else
    {
      borked++;
    }
    //printf("op %li\n", (uint32_t)(*pc) & 0x7f);
    //printf("OP_NOP %li\n", (uint32_t)OP_JMPIF);
  }

  if (mem > 1000000) {
    // mrb_exc_set(mrb, timeout_error);
    // mrb_raise(mrb, E_RUNTIME_ERROR, "Execution Timeout");
    mrb_raise(mrb, mrb->eException_class, "Reached Memory Limit");
  }
}

static PyObject *eval(PymrubyObject *self, PyObject *args) {
  char *code;

  if(!PyArg_ParseTuple(args, "s", &code))
    return Py_BuildValue("s", "Arg parse fail");

  struct mrb_parser_state *parser = mrb_parser_new(self->mrb);

  parser->s = code;
  parser->send = code + strlen(code);
  parser->lineno = 1;
  mrb_parser_parse(parser, self->ctx);

  if (parser->nerr > 0) {
    char error[1000];
    snprintf(error, 1000, "line %d: %s", parser->error_buffer[0].lineno, parser->error_buffer[0].message);
    return Py_BuildValue("s", error);
  }

  struct RProc *proc = mrb_generate_code(self->mrb, parser);

  int time_limit=1;
#ifdef __unix__
  signal(SIGALRM, timeout);
  alarm(time_limit);
#elif _WIN32
  HANDLE hTimer = NULL;
  int param=1;
  printf("CreateTimerQueueTimer\n");
  if (!CreateTimerQueueTimer( &hTimer, NULL, (WAITORTIMERCALLBACK)TimerRoutine, &param, time_limit*1000, 0, 0))
  {
    printf("CreateTimerQueueTimer failed\n");
    return Py_BuildValue("s", "CreateTimerQueueTimer failed");
  }
#endif

  mrb_value result = mrb_context_run(
    self->mrb,
    proc,
    mrb_top_self(self->mrb),
    self->stack_keep
  );

#ifdef __unix__
  signal(SIGALRM, SIG_IGN);
  alarm(0);
#elif _WIN32
  printf("DeleteTimerQueueTimer\n");
  DeleteTimerQueueTimer(NULL,hTimer,NULL);
#endif

  borked = 0;
  mem = 0;

  self->stack_keep = proc->body.irep->nlocals;

  char *buff;
  PyObject *ret;

  size_t slen;
  // exception?
  if (self->mrb->exc) {
    mrb_value exc = mrb_funcall(self->mrb, mrb_obj_value(self->mrb->exc), "inspect", 0);

    slen=RSTRING_LEN(exc);
    buff = malloc(slen);
    memcpy(buff, RSTRING_PTR(exc), slen);
    ret = Py_BuildValue("s#", buff, slen);

    self->mrb->exc = NULL;
  } else {
    enum mrb_vtype t = mrb_type(result);
    mrb_value val;

    switch(t) {
    case MRB_TT_STRING:
      slen=RSTRING_LEN(result);
      buff = malloc(slen);
      memcpy(buff, RSTRING_PTR(result), slen);
      ret = Py_BuildValue("s#", buff, slen);
      break;
    case MRB_TT_FIXNUM:
      ret = Py_BuildValue("n", mrb_fixnum(result));
      break;
    default:
      val = mrb_funcall(self->mrb, result, "inspect", 0);
      if (self->mrb->exc) {
        val = mrb_funcall(self->mrb, mrb_obj_value(self->mrb->exc), "inspect", 0);
      }
      if(!mrb_string_p(val)) {
        val = mrb_obj_as_string(self->mrb, result);
      }

      slen=RSTRING_LEN(val);
      buff = malloc(slen);
      memcpy(buff, RSTRING_PTR(val), slen);
      ret = Py_BuildValue("s#", buff, slen);

      break;
    }
  }

  mrb_gc_arena_restore(self->mrb, self->ai);
  mrb_parser_free(parser);

  return ret;
}

static void Pymruby_dealloc(PymrubyObject *self) {
  mrbc_context_free(self->mrb, self->ctx);
  mrb_close(self->mrb);

  // (self->ob_base).ob_type->tp_free((PyObject*)self);
  PyObject_Free(self);
}


static int Pymruby_init(PymrubyObject *self, PyObject *args) {
  self->mrb = mrb_open();
#ifdef MRB_ENABLE_DEBUG_HOOK
  self->mrb->code_fetch_hook = hook; // define MRB_ENABLE_DEBUG_HOOK
  // self->mrb->debug_op_hook = hook;
  printf("Pymruby enable debug\n");
#endif
  self->mrb->allocf = counting_alloc;
  self->stack_keep = 0;
  self->ctx = mrbc_context_new(self->mrb);
  self->ctx->lineno = 1;
  self->ctx->capture_errors = TRUE;
  mrbc_filename(self->mrb, self->ctx, "lol");
  // mrb_value err_str;
  // err_str = mrb_str_new_lit(self->mrb, "timeout!");
  // err_str = mrb_str_new_cstr(self->mrb, "timeout!");
  // timeout_error = mrb_exc_new_str(self->mrb, mrb_exc_get(self->mrb, "RuntimeError"), err_str);
  // timeout_error = mrb_exc_new_str(self->mrb, mrb_exc_get(self->mrb, "RuntimeError"), err_str);
  self->ai = mrb_gc_arena_save(self->mrb);
  return 0;
}

static PyObject *Pymruby_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PymrubyObject *self;

  // self = (PymrubyObject*)type->tp_alloc(type, 0);
  self=PyObject_New(PymrubyObject,type);
  return (PyObject *)self;
}

static PyObject * pymruby_eval(PyObject *self, PyObject *args)
{
  PymrubyObject* mruby=(PymrubyObject*)Pymruby_new(&Pymruby_Type,NULL,NULL);
  if (mruby == NULL)
    Py_RETURN_NONE;
  if(Pymruby_init(mruby,NULL)<0)
  {
    Pymruby_dealloc(mruby);
    Py_RETURN_NONE;
  }
  PyObject *result = eval(mruby,args);
  Pymruby_dealloc(mruby);
  return result;
}

static int pymruby_exec(PyObject *m)
{
    Pymruby_Type.tp_base = &PyBaseObject_Type;

    if (PyType_Ready(&Pymruby_Type) < 0)
        goto fail;
    PyModule_AddObject(m, "Pymruby", (PyObject *)&Pymruby_Type);
    return 0;
 fail:
    Py_XDECREF(m);
    return -1;
}

// PyMODINIT_FUNC PyInit_pymruby(void) {
//   // Pymruby_Type.tp_new = PyType_GenericNew;
//   if(PyType_Ready(&Pymruby_Type) < 0)
//     return NULL;
//   // PyObject *mod = Py_InitModule3("pymruby", NULL, "Python MRuby Bindings");
//   PyObject *mod=PyModule_Create(&pymruby_module);
//   Py_INCREF(&Pymruby_Type);
//   if(PyModule_AddObject(mod, "Pymruby", (PyObject *)&Pymruby_Type) < 0)
//     return NULL;
//   return mod;
// }

PyMODINIT_FUNC PyInit_pymruby(void)
{
  return PyModuleDef_Init(&pymruby_module);
}
