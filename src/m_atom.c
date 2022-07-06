/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"

#include <stdio.h>
#include <string.h>

/* convenience routines for checking and getting values of
    atoms.  There's no "pointer" version since there's nothing
    safe to return if there's an error. */

t_float atom_getfloat(const t_atom *a)
{
    if(a->a_type == A_FLOAT)
    {
        return (a->a_w.w_float);
    }
    else
    {
        return (0);
    }
}

t_int atom_getint(const t_atom *a) { return (atom_getfloat(a)); }

t_symbol *atom_getsymbol(
    const t_atom *a) /* LATER think about this more carefully */
{
    if(a->a_type == A_SYMBOL)
    {
        return (a->a_w.w_symbol);
    }
    else
    {
        return (&s_float);
    }
}

t_symbol *atom_gensym(const t_atom *a) /* this works  better for graph labels */
{
    if(a->a_type == A_SYMBOL) return (a->a_w.w_symbol);
    char buf[30];
    if(a->a_type == A_FLOAT)
    {
        snprintf(buf, 30, "%g", a->a_w.w_float);
    }
    else
    {
        strcpy(buf, "???");
    }
    return (gensym(buf));
}

t_float atom_getfloatarg(int which, int argc, const t_atom *argv)
{
    if(argc <= which) return (0);
    t_atom *a = argv + which;
    if(a->a_type == A_FLOAT)
    {
        return (a->a_w.w_float);
    }
    else
    {
        return (0);
    }
}

t_int atom_getintarg(int which, int argc, const t_atom *argv)
{
    return (atom_getfloatarg(which, argc, argv));
}

t_symbol *atom_getsymbolarg(int which, int argc, const t_atom *argv)
{
    if(argc <= which) return (&s_);
    t_atom *a = argv + which;
    if(a->a_type == A_SYMBOL)
    {
        return (a->a_w.w_symbol);
    }
    else
    {
        return (&s_);
    }
}

/* convert an atom into a string, in the reverse sense of binbuf_text (q.v.)
 * special attention is paid to symbols containing the special characters
 * ';', ',', '$', and '\'; these are quoted with a preceding '\', except that
 * the '$' only gets quoted if followed by a digit.
 */
void atom_string(const t_atom *a, char *buf, unsigned int bufsize)
{
    switch(a->a_type)
    {
        case A_SEMI:
            strcpy(buf, ";");
            break;
        case A_COMMA:
            strcpy(buf, ",");
            break;
        case A_POINTER:
            strcpy(buf, "(pointer)");
            break;
        case A_FLOAT:
        {
            char tbuf[30];
            sprintf(tbuf, "%g", a->a_w.w_float);
            if(strlen(tbuf) < bufsize - 1)
            {
                strcpy(buf, tbuf);
            }
            else if(a->a_w.w_float < 0)
            {
                strcpy(buf, "-");
            }
            else
            {
                strcpy(buf, "+");
            }
        }
        break;
        case A_SYMBOL:
        case A_DOLLSYM:
        {
            const char *sp = a->a_w.w_symbol->s_name;
            unsigned int len = 0;
            int quote = 0;

            while(*sp != '\0')
            {
                int is_punctuation =
                    *sp == ';' || *sp == ',' || *sp == '\\' || *sp == ' ';
                int is_dollar_sub = a->a_type == A_SYMBOL && *sp == '$' &&
                                    sp[1] >= '0' && sp[1] <= '9';
                if(is_punctuation || is_dollar_sub) quote = 1;
                *sp++;
                len++;
            }
            if(quote)
            {
                char *write_cursor = buf;
                char *max_buffer_end = buf + (bufsize - 2);
                sp = a->a_w.w_symbol->s_name;

                while(write_cursor < max_buffer_end && *sp)
                {
                    int is_punctuation =
                        *sp == ';' || *sp == ',' || *sp == '\\' || *sp == ' ';
                    int is_dollar_sub = a->a_type == A_SYMBOL && *sp == '$' &&
                                        sp[1] >= '0' && sp[1] <= '9';
                    if(is_punctuation || is_dollar_sub)
                    {
                        *write_cursor++ =
                            '\\'; // write a backlash before these, acts as a
                    }
                    // quote I guess ?
                    *write_cursor++ = *sp++;
                }
                if(*sp) // same as case below : (len >= bufsize-1)
                    *write_cursor++ = '*';
                *write_cursor = 0;
                /* post("quote %s -> %s", a->a_w.w_symbol->s_name, buf); */
            }
            else
            {
                if(len < bufsize - 1)
                {
                    strcpy(buf, a->a_w.w_symbol->s_name);
                }
                else
                {
                    strncpy(buf, a->a_w.w_symbol->s_name, bufsize - 2);
                    strcpy(buf + (bufsize - 2), "*");
                }
            }
        }
        break;
        case A_DOLLAR:
            sprintf(buf, "$%d", a->a_w.w_index);
            break;
        default:
            bug("atom_string");
    }
}
