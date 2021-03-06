/*
 * NLize Ruby extended library
 *
 * Fair License
 * 
 * Copyright (c) 2008 arton
 *
 * Usage of the works is permitted provided that this
 * instrument is retained with the works, so that any entity
 * that uses the works is notified of this instrument.
 *
 * DISCLAIMER: THE WORKS ARE WITHOUT WARRANTY.
 */
#include "ruby.h"
#include "extconf.h"
#include <stdio.h>
#include <errno.h>
#if defined(_WIN32)
  #include <windows.h>
#elif defined(HAVE_SYS_MMAN_H)
  #include <sys/mman.h>
  #define PAGE_MASK 0xfffff000
#endif

#if defined(_MSC_VER) && _MSC_VER < 1300
  #define VSNPRINTF _vsnprintf
#else
  #define VSNPRINTF vsnprintf
#endif

#define NLIZE_VERSION "0.0.2"

static VALUE mNLize;
static ID nlize_translate;
static ID new_message;
/**
 * safe call object's method with args
 * args[0] --- self
 * args[1] --- id of the method
 * args[2] --- number of the arguments
 * args[3] --- start of the arguments
 * args[4] --- 2nd argument if exists
 */
static VALUE safe_funcall(VALUE args)
{
    VALUE* argp = (VALUE*)args;
    return rb_funcall2(*argp, *(argp + 1), *(argp + 2), argp + 3);
}

static int alt_vsnprintf(char* buffer, size_t count, const char* format, va_list argptr)
{
    static int reentry = 0;
    VALUE translated;
    const char* alt;
    char* sargs[3];
    reentry++;
#if defined(DEBUG)
    printf("hello %d '%s'\n", reentry, format);
    fflush(stdout);
#endif    
    if (reentry < 10)
    {
        if (strcmp(format, "%s") == 0)
        {
            char** pc = (char**)argptr;
#if defined(DEBUG)
            printf("format=%s, arg=%s\n", format, *pc);
#endif            
            translated = rb_funcall(mNLize, nlize_translate, 1, rb_str_new2(*pc));
        }
        else
        {
            translated = rb_funcall(mNLize, nlize_translate, 1, rb_str_new2(format));
        }
        alt = StringValueCStr(translated);
    }
    else
    {
        alt = format;
    }
    reentry--;
#if defined(DEBUG)
    printf("%s\n", alt);
    fflush(stdout);
#endif        
#if defined(_MSC_VER) && _MSC_VER >= 1300
    return _vsnprintf_l(buffer, count, alt, NULL, argptr);
#elif defined(HAVE_VSNPRINTF_L)
    return vsnprintf_l(buffer, count, NULL, alt, argptr);
#else
    return vsprintf(buffer, alt, argptr);
#endif
}

static VALUE name_err_mesg_new(VALUE obj, VALUE mesg, VALUE recv, VALUE method)
{
    VALUE val = rb_funcall(mNLize, nlize_translate, 1, mesg);
    return rb_funcall(obj, new_message, 3, val, recv, method);
}

static int (*altvsnprintf_p)(char*, size_t, const char*, va_list) = alt_vsnprintf;
static unsigned char* palt;

void Init_cnlize()
{
#if defined(_WIN32)
    DWORD old;
#endif
    unsigned char* org;
    int ret;
    mNLize = rb_const_get(rb_cObject, rb_intern("NLize"));
    rb_define_const(mNLize, "VERSION", rb_str_new2(NLIZE_VERSION));
    nlize_translate = rb_intern("translate");
    org = (unsigned char*)VSNPRINTF;
    palt = (unsigned char*)&altvsnprintf_p;
#if defined(_WIN32)
    VirtualProtect(org, 8, PAGE_EXECUTE_READWRITE, &old);
#else
    ret = mprotect((void*)((int)org & PAGE_MASK), 8, PROT_READ | PROT_WRITE | PROT_EXEC);
    if (ret) {
        /* ignore nlize */
        return;
    }
#endif    
    /* i386 only */
    *org = '\xff';
    *(org + 1) = '\x25';
    memcpy(org + 2, &palt, 4);
#if defined(_WIN32)   
    VirtualProtect(org, 8, old, &old);
#else
    ret = mprotect((void*)((int)org & PAGE_MASK), 8, PROT_READ | PROT_EXEC);    
    if (ret) {
        rb_fatal("can't reset memory config errno(%d)", errno);        
    }
#endif    
    new_message = rb_intern("_new_message");
    rb_alias(rb_singleton_class(rb_cNameErrorMesg), new_message, '!');
    rb_define_singleton_method(rb_cNameErrorMesg, "!", name_err_mesg_new, 3);
}
