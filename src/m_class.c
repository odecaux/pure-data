/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#define PD_CLASS_DEF
#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include "g_canvas.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef _WIN32
#include <io.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf _snprintf
#endif

static t_symbol *class_loadsym; /* name under which an extern is invoked */
static void pd_defaultfloat(t_pd *x, t_float f);
static void pd_defaultlist(t_pd *x, t_symbol *s, int argc, t_atom *argv);
t_pd pd_objectmaker; /* factory for creating "object" boxes */
t_pd pd_canvasmaker; /* factory for creating canvases */

static t_symbol *class_extern_dir;

#ifdef PDINSTANCE
static t_class *class_list = 0;
PERTHREAD t_pdinstance *pd_this = NULL;
t_pdinstance **pd_instances;
int pd_ninstances;
#else
t_symbol s_pointer, s_float, s_symbol, s_bang, s_list, s_anything, s_signal,
    s__N, s__X, s_x, s_y, s_;
#endif
t_pdinstance pd_maininstance;

static t_symbol *dogensym(
    const char *s, t_symbol *oldsym, t_pdinstance *pdinstance);
void x_midi_newpdinstance(void);
void x_midi_freepdinstance(void);
void s_inter_newpdinstance(void);
void s_inter_free(t_instanceinter *inter);
void g_canvas_newpdinstance(void);
void g_canvas_freepdinstance(void);
void d_ugen_newpdinstance(void);
void d_ugen_freepdinstance(void);
void new_anything(void *dummy, t_symbol *s, int argc, t_atom *argv);

void s_stuff_newpdinstance(void)
{
    // STUFF = pd_this->pd_stuff
    STUFF = getbytes(sizeof(*STUFF));
    *STUFF = (t_instancestuff){.st_externlist = 0,
        .st_searchpath = 0,
        .st_staticpath = 0,
        .st_helppath = 0,
        .st_temppath = 0,
        .st_schedblocksize = DEFDACBLKSIZE,
        .st_blocksize = DEFDACBLKSIZE,
        .st_dacsr = DEFDACSAMPLERATE,
        .st_printhook = sys_printhook,
        .st_impdata = NULL};
}

void s_stuff_freepdinstance(void) { freebytes(STUFF, sizeof(*STUFF)); }

static t_pdinstance *pdinstance_init(t_pdinstance *pd)
{
    pd->pd_systime = 0;
    pd->pd_clock_setlist = 0;
    pd->pd_canvaslist = 0;
    pd->pd_templatelist = 0;
    pd->pd_symhash = getbytes(SYMTABHASHSIZE * sizeof(*pd->pd_symhash));
    for(int i = 0; i < SYMTABHASHSIZE; i++)
        pd->pd_symhash[i] = 0;
#ifdef PDINSTANCE
    dogensym("pointer", &pd->pd_s_pointer, pd);
    dogensym("float", &pd->pd_s_float, pd);
    dogensym("symbol", &pd->pd_s_symbol, pd);
    dogensym("bang", &pd->pd_s_bang, pd);
    dogensym("list", &pd->pd_s_list, pd);
    dogensym("anything", &pd->pd_s_anything, pd);
    dogensym("signal", &pd->pd_s_signal, pd);
    dogensym("#N", &pd->pd_s__N, pd);
    dogensym("#X", &pd->pd_s__X, pd);
    dogensym("x", &pd->pd_s_x, pd);
    dogensym("y", &pd->pd_s_y, pd);
    dogensym("", &pd->pd_s_, pd);
    pd_this = pd;
#else
    dogensym("pointer", &s_pointer, pd);
    dogensym("float", &s_float, pd);
    dogensym("symbol", &s_symbol, pd);
    dogensym("bang", &s_bang, pd);
    dogensym("list", &s_list, pd);
    dogensym("anything", &s_anything, pd);
    dogensym("signal", &s_signal, pd);
    dogensym("#N", &s__N, pd);
    dogensym("#X", &s__X, pd);
    dogensym("x", &s_x, pd);
    dogensym("y", &s_y, pd);
    dogensym("", &s_, pd);
#endif
    x_midi_newpdinstance();
    g_canvas_newpdinstance();
    d_ugen_newpdinstance();
    s_stuff_newpdinstance();
    return (pd);
}

static void class_addmethodtolist(t_class *c, t_methodentry **method_array,
    int num_methods, t_gotfn fn, t_symbol *method_name, unsigned char *args,
    t_pdinstance *pdinstance)
{
    for(int i = 0; i < num_methods; i++)
    {
        if(method_name && (*method_array)[i].me_name == method_name)
        {
            char nbuf[80];
            snprintf(nbuf, 80, "%s_aliased", method_name->s_name);
            nbuf[79] = 0;
            (*method_array)[i].me_name = dogensym(nbuf, 0, pdinstance);
            if(c == pd_objectmaker)
                logpost(NULL, PD_VERBOSE,
                    "warning: class '%s' overwritten; old one renamed '%s'",
                    method_name->s_name, nbuf);
            else
                logpost(NULL, PD_VERBOSE,
                    "warning: old method '%s' for class '%s' renamed '%s'",
                    method_name->s_name, c->c_name->s_name, nbuf);
        }
    }
    *method_array =
        t_resizebytes(*method_array, num_methods * sizeof(**method_array),
            (num_methods + 1) * sizeof(**method_array));

    t_methodentry *method = (*method_array) + num_methods;
    method->me_name = method_name;
    method->me_fun = (t_gotfn) fn;
    memcpy(method->me_arg, args, MAXPDARG + 1);
}

#ifdef PDINSTANCE
EXTERN void pd_setinstance(t_pdinstance *pd) { pd_this = pd; }

static void pdinstance_renumber(void)
{
    for(int i = 0; i < pd_ninstances; i++)
        pd_instances[i]->pd_instanceno = i;
}

extern void text_template_init(void);
extern void garray_init(void);

#define grow(array, old_size, newsize)     \
    (array) = (typeof(array)) resizebytes( \
        (array), (old_size) * sizeof(array *), (newsize) * sizeof(array *))

EXTERN t_pdinstance *pdinstance_new(void)
{
    t_pdinstance *new_instance =
        (t_pdinstance *) getbytes(sizeof(t_pdinstance));
    pd_this = new_instance;
    s_inter_newpdinstance();
    pdinstance_init(new_instance);
    sys_lock();
    pd_globallock();
    grow(pd_instances, pd_ninstances, pd_ninstances + 1);
    pd_instances[pd_ninstances] = new_instance;
    for(t_class *class = class_list; class; class = class->c_next)
    {
        grow(class->c_methods, pd_ninstances, pd_ninstances + 1);
        class->c_methods[pd_ninstances] = t_getbytes(0);
        for(int i = 0; i < class->c_nmethod; i++)
        {
            class_addmethodtolist(class, &class->c_methods[pd_ninstances], i,
                class->c_methods[0][i].me_fun,
                dogensym(
                    class->c_methods[0][i].me_name->s_name, 0, new_instance),
                class->c_methods[0][i].me_arg, new_instance);
        }
    }
    pd_ninstances++;
    pdinstance_renumber();
    pd_bind(&glob_pdobject, gensym("pd"));
    text_template_init();
    garray_init();
    pd_globalunlock();
    sys_unlock();
    return (new_instance);
}

EXTERN void pdinstance_free(t_pdinstance *pd)
{
    t_canvas *canvas;
    pd_setinstance(pd);
    sys_lock();
    pd_globallock();

    int instanceno = pd->pd_instanceno;
    t_instanceinter *inter = pd->pd_inter;
    canvas_suspend_dsp();

    while(pd->pd_canvaslist)
        pd_free((t_pd *) pd->pd_canvaslist);
    while(pd->pd_templatelist)
        pd_free((t_pd *) pd->pd_templatelist);

    for(t_class *class = class_list; class; class = class->c_next)
    {
        if(class->c_methods[instanceno])
        {
            freebytes(class->c_methods[instanceno],
                class->c_nmethod * sizeof(**class->c_methods));
        }
        class->c_methods[instanceno] = NULL;
        for(int i = instanceno; i < pd_ninstances - 1; i++)
        {
            class->c_methods[i] = class->c_methods[i + 1];
        }
        class->c_methods = (t_methodentry **) t_resizebytes(class->c_methods,
            pd_ninstances * sizeof(*class->c_methods),
            (pd_ninstances - 1) * sizeof(*class->c_methods));
    }
    for(int i = 0; i < SYMTABHASHSIZE; i++)
    {
        while((t_symbol *s = pd->pd_symhash[i]))
        {
            pd->pd_symhash[i] = s->s_next;
            if(s != &pd->pd_s_pointer && s != &pd->pd_s_float &&
                s != &pd->pd_s_symbol && s != &pd->pd_s_bang &&
                s != &pd->pd_s_list && s != &pd->pd_s_anything &&
                s != &pd->pd_s_signal && s != &pd->pd_s__N &&
                s != &pd->pd_s__X && s != &pd->pd_s_x && s != &pd->pd_s_y &&
                s != &pd->pd_s_)
            {
                freebytes((void *) s->s_name, strlen(s->s_name) + 1);
                freebytes(s, sizeof(*s));
            }
        }
    }
    freebytes(pd->pd_symhash, SYMTABHASHSIZE * sizeof(*pd->pd_symhash));
    x_midi_freepdinstance();
    g_canvas_freepdinstance();
    d_ugen_freepdinstance();
    s_stuff_freepdinstance();

    for(int i = instanceno; i < pd_ninstances - 1; i++)
        pd_instances[i] = pd_instances[i + 1];

    pd_instances = (t_pdinstance **) resizebytes(pd_instances,
        pd_ninstances * sizeof(*pd_instances),
        (pd_ninstances - 1) * sizeof(*pd_instances));
    pd_ninstances--;

    pdinstance_renumber();
    pd_globalunlock();
    sys_unlock();
    pd_setinstance(&pd_maininstance);
    s_inter_free(inter); /* must happen after sys_unlock() */
}

#endif /* PDINSTANCE */

/* this bootstraps the class management system (pd_objectmaker, pd_canvasmaker)
 * it has been moved from the bottom of the file up here, before the class_new()
 * undefine
 */
void mess_init(void)
{
    if(pd_objectmaker) return;
#ifdef PDINSTANCE
    pd_this = &pd_maininstance;
#endif
    s_inter_newpdinstance();
    sys_lock();
    pd_globallock();
    pdinstance_init(&pd_maininstance);
    class_extern_dir = &s_;
    pd_objectmaker = class_new(
        gensym("objectmaker"), 0, 0, sizeof(t_pd), CLASS_DEFAULT, A_NULL);
    pd_canvasmaker = class_new(
        gensym("canvasmaker"), 0, 0, sizeof(t_pd), CLASS_DEFAULT, A_NULL);
    class_addanything(pd_objectmaker, (t_method) new_anything);
    pd_globalunlock();
    sys_unlock();
}

static void pd_defaultanything(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    pd_error(x, "%s: no method for '%s'", (*x)->c_name->s_name, s->s_name);
}

static void pd_defaultbang(t_pd *x)
{
    if(*(*x)->c_listmethod != pd_defaultlist)
        (*(*x)->c_listmethod)(x, 0, 0, 0);
    else
        (*(*x)->c_anymethod)(x, &s_bang, 0, 0);
}

/* am empty list calls the 'bang' method unless it's the default
bang method -- that might turn around and call our 'list' method
which could be an infinite recorsion.  Fall through to calling our
'anything' method.  That had better not turn around and call us with
an empty list.  */
void pd_emptylist(t_pd *x)
{
    if(*(*x)->c_bangmethod != pd_defaultbang)
        (*(*x)->c_bangmethod)(x);
    else
        (*(*x)->c_anymethod)(x, &s_bang, 0, 0);
}

static void pd_defaultpointer(t_pd *x, t_gpointer *gp)
{
    if(*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETPOINTER(&at, gp);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETPOINTER(&at, gp);
        (*(*x)->c_anymethod)(x, &s_pointer, 1, &at);
    }
}

static void pd_defaultfloat(t_pd *x, t_float f)
{
    if(*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETFLOAT(&at, f);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETFLOAT(&at, f);
        (*(*x)->c_anymethod)(x, &s_float, 1, &at);
    }
}

static void pd_defaultsymbol(t_pd *x, t_symbol *s)
{
    if(*(*x)->c_listmethod != pd_defaultlist)
    {
        t_atom at;
        SETSYMBOL(&at, s);
        (*(*x)->c_listmethod)(x, 0, 1, &at);
    }
    else
    {
        t_atom at;
        SETSYMBOL(&at, s);
        (*(*x)->c_anymethod)(x, &s_symbol, 1, &at);
    }
}

void obj_list(t_object *x, t_symbol *s, int argc, t_atom *argv);
static void class_nosavefn(t_gobj *z, t_binbuf *b);

/* handle "list" messages to Pds without explicit list methods defined. */
static void pd_defaultlist(t_pd *x, t_symbol *s, int argc, t_atom *argv)
{
    /* a list with no elements is handled by the 'bang' method if
    one exists. */
    if(argc == 0 && *(*x)->c_bangmethod != pd_defaultbang)
    {
        (*(*x)->c_bangmethod)(x);
        return;
    }
    /* a list with one element which is a number can be handled by a
    "float" method if any is defined; same for "symbol", "pointer". */
    if(argc == 1)
    {
        if(argv->a_type == A_FLOAT && *(*x)->c_floatmethod != pd_defaultfloat)
        {
            (*(*x)->c_floatmethod)(x, argv->a_w.w_float);
            return;
        }
        else if(argv->a_type == A_SYMBOL &&
                *(*x)->c_symbolmethod != pd_defaultsymbol)
        {
            (*(*x)->c_symbolmethod)(x, argv->a_w.w_symbol);
            return;
        }
        else if(argv->a_type == A_POINTER &&
                *(*x)->c_pointermethod != pd_defaultpointer)
        {
            (*(*x)->c_pointermethod)(x, argv->a_w.w_gpointer);
            return;
        }
    }
    /* Next try for an "anything" method */
    if((*x)->c_anymethod != pd_defaultanything)
        (*(*x)->c_anymethod)(x, &s_list, argc, argv);

    /* if the object is patchable (i.e., can have proper inlets)
        send it on to obj_list which will unpack the list into the inlets */
    else if((*x)->c_patchable)
        obj_list((t_object *) x, s, argc, argv);
    /* otherwise gove up and complain. */
    else
        pd_defaultanything(x, &s_list, argc, argv);
}

/* for now we assume that all "gobjs" are text unless explicitly
overridden later by calling class_setbehavior().  I'm not sure
how to deal with Pds that aren't gobjs; shouldn't there be a
way to check that at run time?  Perhaps the presence of a "newmethod"
should be our cue, or perhaps the "tiny" flag.  */

/* another matter.  This routine does two unrelated things: it creates
a Pd class, but also adds a "new" method to create an instance of it.
These are combined for historical reasons and for brevity in writing
objects.  To avoid adding a "new" method send a null function pointer.
To add additional ones, use class_addcreator below.  Some "classes", like
"select", are actually two classes of the same name, one for the single-
argument form, one for the multiple one; see select_setup() to find out
how this is handled.  */

extern void text_save(t_gobj *z, t_binbuf *b);

t_class *class_new(t_symbol *class_name, t_newmethod newmethod,
    t_method freemethod, size_t size, int flags, t_atomtype type1, ...)
{
    va_list ap;
    t_atomtype vec[MAXPDARG + 1];
    t_atomtype *vp = vec;
    int count = 0;
    int typeflag = flags & CLASS_TYPEMASK;
    if(!typeflag) typeflag = CLASS_PATCHABLE;
    *vp = type1;

    va_start(ap, type1);
    while(*vp)
    {
        if(count == MAXPDARG)
        {
            if(class_name)
                pd_error(0,
                    "class %s: sorry: only %d args typechecked; use A_GIMME",
                    class_name->s_name, MAXPDARG);
            else
                pd_error(0,
                    "unnamed class: sorry: only %d args typechecked; use "
                    "A_GIMME",
                    MAXPDARG);
            break;
        }
        vp++;
        count++;
        *vp = va_arg(ap, t_atomtype);
    }
    va_end(ap);

    if(pd_objectmaker && newmethod)
    {
        /* add a "new" method by the name specified by the object */
        class_addmethod(pd_objectmaker, (t_method) newmethod, class_name,
            vec[0], vec[1], vec[2], vec[3], vec[4], vec[5]);
        if(class_name && class_loadsym &&
            !zgetfn(&pd_objectmaker, class_loadsym))
        {
            /* if we're loading an extern it might have been invoked by a
            longer file name; in this case, make this an admissible name
            too. */
            const char *loadstring = class_loadsym->s_name;
            size_t l1 = strlen(class_name->s_name);
            size_t l2 = strlen(loadstring);
            if(l2 > l1 && !strcmp(class_name->s_name, loadstring + (l2 - l1)))
                class_addmethod(pd_objectmaker, (t_method) newmethod,
                    class_loadsym, vec[0], vec[1], vec[2], vec[3], vec[4],
                    vec[5]);
        }
    }
    t_class *c = (t_class *) t_getbytes(sizeof(*c));
    c->c_name = c->c_helpname = class_name;
    c->c_size = size;
    c->c_nmethod = 0;
    c->c_freemethod = (t_method) freemethod;
    c->c_bangmethod = pd_defaultbang;
    c->c_pointermethod = pd_defaultpointer;
    c->c_floatmethod = pd_defaultfloat;
    c->c_symbolmethod = pd_defaultsymbol;
    c->c_listmethod = pd_defaultlist;
    c->c_anymethod = pd_defaultanything;
    c->c_wb = (typeflag == CLASS_PATCHABLE ? &text_widgetbehavior : 0);
    c->c_pwb = 0;
    c->c_firstin = ((flags & CLASS_NOINLET) == 0);
    c->c_patchable = (typeflag == CLASS_PATCHABLE);
    c->c_gobj = (typeflag >= CLASS_GOBJ);
    c->c_drawcommand = 0;
    c->c_floatsignalin = 0;
    c->c_externdir = class_extern_dir;
    c->c_savefn = (typeflag == CLASS_PATCHABLE ? text_save : class_nosavefn);
    c->c_classfreefn = 0;
#ifdef PDINSTANCE
    c->c_methods =
        (t_methodentry **) t_getbytes(pd_ninstances * sizeof(*c->c_methods));
    for(int i = 0; i < pd_ninstances; i++)
        c->c_methods[i] = t_getbytes(0);
    c->c_next = class_list;
    class_list = c;
#else
    c->c_methods = t_getbytes(0);
#endif
#if 0 /* enable this if you want to see a list of all classes */
    post("class: %s", c->c_name->s_name);
#endif
    return (c);
}

void class_free(t_class *c)
{
#ifdef PDINSTANCE
    if(class_list == c)
    {
        class_list = c->c_next;
    }
    else
    {
        t_class *prev = class_list;
        while(prev->c_next != c)
            prev = prev->c_next;
        prev->c_next = c->c_next;
    }
#endif
    if(c->c_classfreefn) c->c_classfreefn(c);
#ifdef PDINSTANCE
    for(int i = 0; i < pd_ninstances; i++)
    {
        if(c->c_methods[i])
            freebytes(c->c_methods[i], c->c_nmethod * sizeof(*c->c_methods[i]));
        c->c_methods[i] = NULL;
    }
    freebytes(c->c_methods, pd_ninstances * sizeof(*c->c_methods));
#else
    freebytes(c->c_methods, c->c_nmethod * sizeof(*c->c_methods));
#endif
    freebytes(c, sizeof(*c));
}

void class_setfreefn(t_class *c, t_classfreefn fn) { c->c_classfreefn = fn; }

#ifdef PDINSTANCE
t_class *class_getfirst(void) { return class_list; }
#endif

/* add a creation method, which is a function that returns a Pd object
suitable for putting in an object box.  We presume you've got a class it
can belong to, but this won't be used until the newmethod is actually
called back (and the new method explicitly takes care of this.) */

void class_addcreator(
    t_newmethod newmethod, t_symbol *class_name, t_atomtype type1, ...)
{
    va_list ap;
    t_atomtype vec[MAXPDARG + 1];
    t_atomtype *vp = vec;
    int count = 0;
    *vp = type1;

    va_start(ap, type1);
    while(*vp)
    {
        if(count == MAXPDARG)
        {
            if(class_name)
                pd_error(0,
                    "class %class_name: sorry: only %d creation args allowed",
                    class_name->s_name, MAXPDARG);
            else
                pd_error(0,
                    "unnamed class: sorry: only %d creation args allowed",
                    MAXPDARG);
            break;
        }
        vp++;
        count++;
        *vp = va_arg(ap, t_atomtype);
    }
    va_end(ap);
    class_addmethod(pd_objectmaker, (t_method) newmethod, class_name, vec[0],
        vec[1], vec[2], vec[3], vec[4], vec[5]);
}

typedef struct
{
    char class_name[512];
    char method_name[512];
} Method;

static Method methods[1000];
static int method_idx = 0;

void class_addmethod(
    t_class *class, t_method fn, t_symbol *method_name, t_atomtype arg1, ...)
{
    {
        strcpy(methods[method_idx].class_name, class->c_name->s_name);
        strcpy(methods[method_idx].method_name, method_name->s_name);
        method_idx++;
    }

    va_list ap;
    t_atomtype argtype = arg1;
    if(!class) return;
    va_start(ap, arg1);
    /* "signal" method specifies that we take audio signals but
    that we don't want automatic float to signal conversion.  This
    is obsolete; you should now use the CLASS_MAINSIGNALIN macro. */
    if(method_name == &s_signal)
    {
        if(class->c_floatsignalin)
            post("warning: signal method overrides class_mainsignalin");
        class->c_floatsignalin = -1;
    }
    /* check for special cases.  "Pointer" is missing here so that
    pd_objectmaker's pointer method can be typechecked differently.  */
    if(method_name == &s_bang)
    {
        if(argtype) goto phooey;
        class_addbang(class, fn);
    }
    else if(method_name == &s_float)
    {
        if(argtype != A_FLOAT || va_arg(ap, t_atomtype)) goto phooey;
        class_doaddfloat(class, fn);
    }
    else if(method_name == &s_symbol)
    {
        if(argtype != A_SYMBOL || va_arg(ap, t_atomtype)) goto phooey;
        class_addsymbol(class, fn);
    }
    else if(method_name == &s_list)
    {
        if(argtype != A_GIMME) goto phooey;
        class_addlist(class, fn);
    }
    else if(method_name == &s_anything)
    {
        if(argtype != A_GIMME) goto phooey;
        class_addanything(class, fn);
    }
    else
    {
        unsigned char argvec[MAXPDARG + 1];
        int nargs = 0;
        while(argtype != A_NULL && nargs < MAXPDARG)
        {
            argvec[nargs++] = argtype;
            argtype = va_arg(ap, t_atomtype);
        }
        if(argtype != A_NULL)
            pd_error(0,
                "%s_%s: only 5 arguments are typecheckable; use A_GIMME",
                (class->c_name) ? (class->c_name->s_name) : "<anon>",
                method_name ? (method_name->s_name) : "<nomethod>");
        argvec[nargs] = 0;
#ifdef PDINSTANCE
        for(int i = 0; i < pd_ninstances; i++)
        {
            class_addmethodtolist(class, &class->c_methods[i], class->c_nmethod,
                (t_gotfn) fn,
                method_name ? dogensym(method_name->s_name, 0, pd_instances[i])
                            : 0,
                argvec, pd_instances[i]);
        }
#else
        class_addmethodtolist(class, &class->c_methods, class->c_nmethod,
            (t_gotfn) fn, method_name, argvec, &pd_maininstance);
#endif
        class->c_nmethod++;
    }
    goto done;
phooey:
    bug("class_addmethod: %s_%s: bad argument types\n",
        (class->c_name) ? (class->c_name->s_name) : "<anon>",
        method_name ? (method_name->s_name) : "<nomethod>");
done:
    va_end(ap);
    return;
}

/* Instead of these, see the "class_addfloat", etc.,  macros in m_pd.h */
void class_addbang(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_bangmethod = (t_bangmethod) fn;
}

void class_addpointer(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_pointermethod = (t_pointermethod) fn;
}

void class_doaddfloat(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_floatmethod = (t_floatmethod) fn;
}

void class_addsymbol(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_symbolmethod = (t_symbolmethod) fn;
}

void class_addlist(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_listmethod = (t_listmethod) fn;
}

void class_addanything(t_class *class, t_method fn)
{
    if(!class) return;
    class->c_anymethod = (t_anymethod) fn;
}

void class_setwidget(t_class *class, const t_widgetbehavior *w)
{
    if(!class) return;
    class->c_wb = w;
}

void class_setparentwidget(t_class *class, const t_parentwidgetbehavior *pw)
{
    if(!class) return;
    class->c_pwb = pw;
}

const char *class_getname(const t_class *class)
{
    if(!class) return 0;
    return (class->c_name->s_name);
}

const char *class_gethelpname(const t_class *class)
{
    if(!class) return 0;
    return (class->c_helpname->s_name);
}

void class_sethelpsymbol(t_class *class, t_symbol *s)
{
    if(!class) return;
    class->c_helpname = s;
}

const t_parentwidgetbehavior *pd_getparentwidget(t_pd *widget)
{
    return ((*widget)->c_pwb);
}

void class_setdrawcommand(t_class *class)
{
    if(!class) return;
    class->c_drawcommand = 1;
}

int class_isdrawcommand(const t_class *class)
{
    if(!class) return 0;
    return (class->c_drawcommand);
}

static void pd_floatforsignal(t_pd *x, t_float value)
{
    int offset = (*x)->c_floatsignalin;
    if(offset > 0)
        *(t_float *) (((char *) x) + offset) = value;
    else
        pd_error(
            x, "%s: float unexpected for signal input", (*x)->c_name->s_name);
}

void class_domainsignalin(t_class *class, int onset)
{
    if(!class) return;
    if(onset <= 0)
    {
        onset = -1;
    }
    else
    {
        if(class->c_floatmethod != pd_defaultfloat)
            post(
                "warning: %s: float method overwritten", class->c_name->s_name);
        class->c_floatmethod = (t_floatmethod) pd_floatforsignal;
    }
    class->c_floatsignalin = onset;
}

void class_set_extern_dir(t_symbol *path) { class_extern_dir = path; }

const char *class_gethelpdir(const t_class *class)
{
    if(!class) return 0;
    return (class->c_externdir->s_name);
}

static void class_nosavefn(t_gobj *z, t_binbuf *b)
{
    bug("save function called but not defined");
}

void class_setsavefn(t_class *class, t_savefn f)
{
    if(!class) return;
    class->c_savefn = f;
}

t_savefn class_getsavefn(const t_class *class)
{
    if(!class) return 0;
    return (class->c_savefn);
}

void class_setpropertiesfn(t_class *class, t_propertiesfn f)
{
    if(!class) return;
    class->c_propertiesfn = f;
}

t_propertiesfn class_getpropertiesfn(const t_class *class)
{
    if(!class) return 0;
    return (class->c_propertiesfn);
}

/* ---------------- the symbol table ------------------------ */

static t_symbol *dogensym(
    const char *name, t_symbol *oldsym, t_pdinstance *pdinstance)
{
    int length = 0;
    unsigned int hash = 5381;
    for(const char *character = name; *character != 0; character++)
    {
        hash = ((hash << 5) + hash) + *character;
        length++;
    }

    t_symbol **symhashloc =
        pdinstance->pd_symhash + (hash & (SYMTABHASHSIZE - 1));
    t_symbol *sym2;
    while((sym2 = *symhashloc))
    {
        // already in the cache
        if(!strcmp(sym2->s_name, name)) return (sym2);
        symhashloc = &sym2->s_next;
    }

    t_symbol *new_sym;
    if(oldsym)
        new_sym = oldsym;
    else
        new_sym = (t_symbol *) t_getbytes(sizeof(*new_sym));

    *new_sym =
        (t_symbol){.s_name = t_getbytes(length + 1), .s_thing = 0, .s_next = 0};
    strcpy(new_sym->s_name, name);
    *symhashloc = new_sym;
    return (new_sym);
}

t_symbol *gensym(const char *name) { return (dogensym(name, 0, pd_this)); }

static t_symbol *addfileextent(t_symbol *file_name)
{
    const char *str = file_name->s_name;
    int ln = (int) strlen(str);
    if(!strcmp(str + ln - 3, ".pd")) return (file_name);
    char new_file_name[MAXPDSTRING];
    strcpy(new_file_name, str);
    strcpy(new_file_name + ln, ".pd");
    return (gensym(new_file_name));
}

#define MAXOBJDEPTH 1000
static int tryingalready;

void canvas_popabstraction(t_canvas *x);

t_symbol *pathsearch(t_symbol *s, char *ext);
int pd_setloadingabstraction(t_symbol *sym);

/* this routine is called when a new "object" is requested whose class Pd
doesn't know.  Pd tries to load it as an extern, then as an abstraction. */
void new_anything(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    int fd;
    char dirbuf[MAXPDSTRING];
    char classslashclass[MAXPDSTRING];
    char *nameptr;
    if(tryingalready > MAXOBJDEPTH)
    {
        pd_error(0, "maximum object loading depth %d reached", MAXOBJDEPTH);
        return;
    }
    if(s == &s_anything)
    {
        pd_error(0, "object name \"%s\" not allowed", s->s_name);
        return;
    }
    pd_this->pd_newest = 0;
    class_loadsym = s;
    pd_globallock();
    if(sys_load_lib(canvas_getcurrent(), s->s_name))
    {
        tryingalready++;
        typedmess(dummy, s, argc, argv);
        tryingalready--;
        return;
    }
    class_loadsym = 0;
    pd_globalunlock();
}

/* This is externally available, but note that it might later disappear; the
whole "newest" thing is a hack which needs to be redesigned. */
t_pd *pd_newest(void) { return (pd_this->pd_newest); }

/* horribly, we need prototypes for each of the artificial function
calls in typedmess(), to keep the compiler quiet. */
typedef t_pd *(*t_newgimme)(t_symbol *s, int argc, t_atom *argv);
typedef void (*t_messgimme)(t_pd *x, t_symbol *s, int argc, t_atom *argv);

typedef t_pd *(*t_fun0)(
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun1)(t_int i1, t_floatarg d1, t_floatarg d2, t_floatarg d3,
    t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun2)(t_int i1, t_int i2, t_floatarg d1, t_floatarg d2,
    t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun3)(t_int i1, t_int i2, t_int i3, t_floatarg d1,
    t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun4)(t_int i1, t_int i2, t_int i3, t_int i4, t_floatarg d1,
    t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun5)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4, t_floatarg d5);
typedef t_pd *(*t_fun6)(t_int i1, t_int i2, t_int i3, t_int i4, t_int i5,
    t_int i6, t_floatarg d1, t_floatarg d2, t_floatarg d3, t_floatarg d4,
    t_floatarg d5);

void pd_typedmess(t_pd *x, t_symbol *name, int argc, t_atom *argv)
{
    t_class *class = *x;

    /* check for messages that are handled by fixed slots in the class
    structure. */
    if(name == &s_float)
    {
        if(!argc)
            (*class->c_floatmethod)(x, 0.);
        else if(argv->a_type == A_FLOAT)
            (*class->c_floatmethod)(x, argv->a_w.w_float);
        else
            goto badarg;
        return;
    }
    if(name == &s_bang)
    {
        (*class->c_bangmethod)(x);
        return;
    }
    if(name == &s_list)
    {
        (*class->c_listmethod)(x, name, argc, argv);
        return;
    }
    if(name == &s_symbol)
    {
        if(argc && argv->a_type == A_SYMBOL)
            (*class->c_symbolmethod)(x, argv->a_w.w_symbol);
        else
            (*class->c_symbolmethod)(x, &s_);
        return;
    }
    /* pd_objectmaker doesn't require
    an actual pointer value */
    if(name == &s_pointer && x != &pd_objectmaker)
    {
        if(argc && argv->a_type == A_POINTER)
            (*class->c_pointermethod)(x, argv->a_w.w_gpointer);
        else
            goto badarg;
        return;
    }
#ifdef PDINSTANCE
    t_methodentry *methods = class->c_methods[pd_this->pd_instanceno];
#else
    t_methodentry *methods = class->c_methods;
#endif

    int narg = 0;
    for(int i = 0; i < class->c_nmethod; i++)
    {
        t_methodentry *method = methods + i;
        if(method->me_name == name)
        {
            unsigned char *wanted_arg_types = method->me_arg;
            if(wanted_arg_types[0] == A_GIMME)
            {
                if(x == &pd_objectmaker)
                {
                    pd_this->pd_newest =
                        (*((t_newgimme) (method->me_fun)))(name, argc, argv);
                }
                else
                {
                    (*((t_messgimme) (method->me_fun)))(x, name, argc, argv);
                }
                return;
            }

            if(argc > MAXPDARG) argc = MAXPDARG;

            t_floatarg float_args[MAXPDARG + 1];
            int float_arg_idx = 0;
            t_int ptr_args[MAXPDARG + 1];
            int ptr_arg_idx = 0;

            if(x != &pd_objectmaker)
            {
                ptr_args[ptr_arg_idx++] = (t_int) x;
                narg++;
            }

            int arg_idx = 0;
            int wanted_type_idx = 0;
            unsigned char wanted_type;

            // split between pointer and float arguments
            while((wanted_type = wanted_arg_types[wanted_type_idx++]))
            {
                switch(wanted_type)
                {
                    case A_POINTER:
                    {
                        if(arg_idx == argc) goto badarg;
                        t_int *ptr_arg = &ptr_args[ptr_arg_idx++];
                        t_atom arg = argv[arg_idx++];
                        if(arg.a_type != A_POINTER) goto badarg;
                        *ptr_arg = (t_int) (arg.a_w.w_gpointer);
                        narg++;
                    }
                    break;
                    case A_FLOAT:
                    {
                        if(arg_idx == argc) goto badarg; /* falls through */
                    }
                    case A_DEFFLOAT:
                    {
                        t_floatarg *float_arg = &float_args[float_arg_idx++];
                        if(arg_idx == argc)
                        {
                            *float_arg = 0;
                        }
                        else
                        {
                            t_atom arg = argv[arg_idx++];
                            if(arg.a_type != A_FLOAT) goto badarg;
                            *float_arg = arg.a_w.w_float;
                        }
                        narg++;
                    }
                    break;
                    case A_SYMBOL:
                    {
                        if(arg_idx == argc) goto badarg; /* falls through */
                    }
                    case A_DEFSYM:
                    {
                        t_int *ptr_arg = &ptr_args[ptr_arg_idx++];
                        if(arg_idx == argc)
                        {
                            *ptr_arg = (t_int) (&s_);
                        }
                        else
                        {
                            t_atom arg = argv[arg_idx++];
                            if(arg.a_type == A_SYMBOL)
                                *ptr_arg = (t_int) (arg.a_w.w_symbol);
                            /* if it's an unfilled "dollar" argument it appears
                            as zero here; cheat and bash it to the null
                            symbol.  Unfortunately, this lets real zeros
                            pass as symbols too, which seems wrong... */
                            else if(x == &pd_objectmaker &&
                                    arg.a_type == A_FLOAT &&
                                    arg.a_w.w_float == 0)
                                *ptr_arg = (t_int) (&s_);
                            else
                                goto badarg;
                        }
                        narg++;
                    }
                    break;
                    default:
                    {
                        goto badarg;
                    }
                    break;
                }
            }

            t_pd *bonzo;
            t_int p_0 = ptr_args[0], p_1 = ptr_args[1], p_2 = ptr_args[2],
                  p_3 = ptr_args[3], p_4 = ptr_args[4], p_5 = ptr_args[5];
            t_floatarg f_0 = float_args[0], f_1 = float_args[1],
                       f_2 = float_args[2], f_3 = float_args[3],
                       f_4 = float_args[4];
            switch(narg)
            {
                case 0:
                    bonzo =
                        (*(t_fun0) (method->me_fun))(f_0, f_1, f_2, f_3, f_4);
                    break;
                case 1:
                    bonzo = (*(t_fun1) (method->me_fun))(
                        p_0, f_0, f_1, f_2, f_3, f_4);
                    break;
                case 2:
                    bonzo = (*(t_fun2) (method->me_fun))(
                        p_0, p_1, f_0, f_1, f_2, f_3, f_4);
                    break;
                case 3:
                    bonzo = (*(t_fun3) (method->me_fun))(
                        p_0, p_1, p_2, f_0, f_1, f_2, f_3, f_4);
                    break;
                case 4:
                    bonzo = (*(t_fun4) (method->me_fun))(
                        p_0, p_1, p_2, p_3, f_0, f_1, f_2, f_3, f_4);
                    break;
                case 5:
                    bonzo = (*(t_fun5) (method->me_fun))(
                        p_0, p_1, p_2, p_3, p_4, f_0, f_1, f_2, f_3, f_4);
                    break;
                case 6:
                    bonzo = (*(t_fun6) (method->me_fun))(
                        p_0, p_1, p_2, p_3, p_4, p_5, f_0, f_1, f_2, f_3, f_4);
                    break;
                default:
                    bonzo = 0;
            }
            if(x == &pd_objectmaker) pd_this->pd_newest = bonzo;
            return;
        }
    }
    (*class->c_anymethod)(x, name, argc, argv);
    return;
badarg:
    pd_error(x, "bad arguments for message '%s' to object '%s'", name->s_name,
        class->c_name->s_name);
}

/* convenience routine giving a stdarg interface to typedmess().  Only
ten args supported; it seems unlikely anyone will need more since
longer messages are likely to be programmatically generated anyway. */
void pd_vmess(t_pd *x, t_symbol *sel, const char *fmt, ...)
{
    t_atom arg[10];
    t_atom *at = arg;
    int nargs = 0;
    const char *fp = fmt;

    va_list ap;
    va_start(ap, fmt);
    while(1)
    {
        if(nargs >= 10)
        {
            pd_error(x, "pd_vmess: only 10 allowed");
            break;
        }
        switch(*fp++)
        {
            case 'f':
                SETFLOAT(at, va_arg(ap, double));
                break;
            case 's':
                SETSYMBOL(at, va_arg(ap, t_symbol *));
                break;
            case 'i':
                SETFLOAT(at, va_arg(ap, t_int));
                break;
            case 'p':
                SETPOINTER(at, va_arg(ap, t_gpointer *));
                break;
            default:
                goto done;
        }
        at++;
        nargs++;
    }
done:
    va_end(ap);
    typedmess(x, sel, nargs, arg);
}

void pd_forwardmess(t_pd *x, int argc, t_atom *argv)
{
    if(argc)
    {
        t_atomtype t = argv->a_type;
        if(t == A_SYMBOL)
            pd_typedmess(x, argv->a_w.w_symbol, argc - 1, argv + 1);
        else if(t == A_POINTER)
        {
            if(argc == 1)
                pd_pointer(x, argv->a_w.w_gpointer);
            else
                pd_list(x, &s_list, argc, argv);
        }
        else if(t == A_FLOAT)
        {
            if(argc == 1)
                pd_float(x, argv->a_w.w_float);
            else
                pd_list(x, &s_list, argc, argv);
        }
        else
            bug("pd_forwardmess");
    }
}

void nullfn(void) {}

t_gotfn getfn(const t_pd *x, t_symbol *name)
{
    const t_class *class = *x;
    t_methodentry *method_list;
#ifdef PDINSTANCE
    method_list = class->c_methods[pd_this->pd_instanceno];
#else
    method_list = class->c_methods;
#endif

    for(int i = 0; i < class->c_nmethod; i++)
    {
        t_methodentry *method = &method_list[i];
        if(method->me_name == name) return (method->me_fun);
    }
    pd_error(x, "%s: no method for message '%s'", class->c_name->s_name,
        name->s_name);
    return ((t_gotfn) nullfn);
}

t_gotfn zgetfn(const t_pd *x, t_symbol *name)
{
    const t_class *class = *x;
    t_methodentry *method_list;
#ifdef PDINSTANCE
    method_list = class->c_methods[pd_this->pd_instanceno];
#else
    method_list = class->c_methods;
#endif

    for(int i = 0; i < class->c_nmethod; i++)
    {
        t_methodentry *method = &method_list[i];
        if(method->me_name == name) return (method->me_fun);
    }
    return (0);
}

void c_extern(t_externclass *cls, t_newmethod newroutine, t_method freeroutine,
    t_symbol *name, size_t size, int tiny, t_atomtype arg1, ...)
{
    bug("'c_extern' not implemented.");
}

void c_addmess(t_method fn, t_symbol *sel, t_atomtype arg1, ...)
{
    bug("'c_addmess' not implemented.");
}

/* provide 'class_new' fallbacks, in case a double-precision Pd attempts to
 * load a single-precision external, or vice versa
 */
#ifdef class_new
#undef class_new
#endif
t_class *
#if PD_FLOATSIZE == 32
class_new64
#else
class_new
#endif
    (t_symbol *s, t_newmethod newmethod, t_method freemethod, size_t size,
        int flags, t_atomtype type1, ...)
{
    const int ext_floatsize =
#if PD_FLOATSIZE == 32
        64
#else
        32
#endif
        ;
    static int loglevel = 0;
    if(s)
    {
        logpost(0, loglevel,
            "refusing to load %dbit-float object '%s' into %dbit-float Pd",
            ext_floatsize, s->s_name, PD_FLOATSIZE);
        loglevel = 3;
    }
    else
        logpost(0, 3,
            "refusing to load unnamed %dbit-float object into %dbit-float Pd",
            ext_floatsize, PD_FLOATSIZE);

    return 0;
}
