/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#include "g_all_guis.h"
#include <math.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define TMARGIN 2
#define BMARGIN 3

/* ------------ vsl gui-vertical  slider ----------------------- */

t_widgetbehavior vslider_widgetbehavior;
static t_class *vslider_class;

/* widget helper functions */

static void vslider_draw_update(t_gobj *client, t_glist *glist)
{
    t_vslider *x = (t_vslider *) client;
    if(glist_isvisible(glist))
    {
        int r = text_ypix(&x->x_gui.x_obj, glist) + x->x_gui.x_h -
                ((x->x_val + 50) / 100);
        int xpos = text_xpix(&x->x_gui.x_obj, glist);

        sys_vgui(".x%lx.c coords %lxKNOB %d %d %d %d\n", glist_getcanvas(glist),
            x, xpos + IEMGUI_ZOOM(x), r, xpos + x->x_gui.x_w - IEMGUI_ZOOM(x),
            r);
    }
}

static void vslider_draw_new(t_vslider *x, t_glist *glist)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x);
    int ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    int tmargin = TMARGIN * IEMGUI_ZOOM(x);
    int bmargin = BMARGIN * IEMGUI_ZOOM(x);
    int r = ypos + x->x_gui.x_h - ((x->x_val + 50) / 100);
    t_canvas *canvas = glist_getcanvas(glist);

    sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -fill #%06x -tags "
             "%lxBASE\n",
        canvas, xpos, ypos - tmargin, xpos + x->x_gui.x_w,
        ypos + x->x_gui.x_h + bmargin, IEMGUI_ZOOM(x), x->x_gui.x_bcol, x);
    if(!x->x_gui.x_fsf.x_snd_able)
    {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list "
                 "%lxOUT%d outlet]\n",
            canvas, xpos, ypos + x->x_gui.x_h + bmargin + IEMGUI_ZOOM(x) - ioh,
            xpos + iow, ypos + x->x_gui.x_h + bmargin, x, 0);
    }
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list "
                 "%lxIN%d inlet]\n",
            canvas, xpos, ypos - tmargin, xpos + iow,
            ypos - tmargin - IEMGUI_ZOOM(x) + ioh, x, 0);
    }
    sys_vgui(
        ".x%lx.c create line %d %d %d %d -width %d -fill #%06x -tags %lxKNOB\n",
        canvas, xpos + IEMGUI_ZOOM(x), r, xpos + x->x_gui.x_w - IEMGUI_ZOOM(x),
        r, 1 + 2 * IEMGUI_ZOOM(x), x->x_gui.x_fcol, x);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w \
             -font {{%s} -%d %s} -fill #%06x -tags [list %lxLABEL label text]\n",
        canvas, xpos + x->x_gui.x_ldx * IEMGUI_ZOOM(x),
        ypos + x->x_gui.x_ldy * IEMGUI_ZOOM(x),
        (strcmp(x->x_gui.x_lab->s_name, "empty") ? x->x_gui.x_lab->s_name : ""),
        x->x_gui.x_font, x->x_gui.x_fontsize * IEMGUI_ZOOM(x), sys_fontweight,
        x->x_gui.x_lcol, x);
}

static void vslider_draw_move(t_vslider *x, t_glist *glist)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x);
    int ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    int tmargin = TMARGIN * IEMGUI_ZOOM(x);
    int bmargin = BMARGIN * IEMGUI_ZOOM(x);
    int r = ypos + x->x_gui.x_h - ((x->x_val + 50) / 100);
    t_canvas *canvas = glist_getcanvas(glist);

    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n", canvas, x, xpos,
        ypos - tmargin, xpos + x->x_gui.x_w, ypos + x->x_gui.x_h + bmargin);
    if(!x->x_gui.x_fsf.x_snd_able)
    {
        sys_vgui(".x%lx.c coords %lxOUT%d %d %d %d %d\n", canvas, x, 0, xpos,
            ypos + x->x_gui.x_h + bmargin + IEMGUI_ZOOM(x) - ioh, xpos + iow,
            ypos + x->x_gui.x_h + bmargin);
    }
    if(!x->x_gui.x_fsf.x_rcv_able)
    {
        sys_vgui(".x%lx.c coords %lxIN%d %d %d %d %d\n", canvas, x, 0, xpos,
            ypos - tmargin, xpos + iow, ypos - tmargin - IEMGUI_ZOOM(x) + ioh);
    }
    sys_vgui(".x%lx.c coords %lxKNOB %d %d %d %d\n", canvas, x,
        xpos + IEMGUI_ZOOM(x), r, xpos + x->x_gui.x_w - IEMGUI_ZOOM(x), r);
    sys_vgui(".x%lx.c coords %lxLABEL %d %d\n", canvas, x,
        xpos + x->x_gui.x_ldx * IEMGUI_ZOOM(x),
        ypos + x->x_gui.x_ldy * IEMGUI_ZOOM(x));
}

static void vslider_draw_erase(t_vslider *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);

    sys_vgui(".x%lx.c delete %lxBASE\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxKNOB\n", canvas, x);
    sys_vgui(".x%lx.c delete %lxLABEL\n", canvas, x);
    if(!x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if(!x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

static void vslider_draw_config(t_vslider *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);

    sys_vgui(".x%lx.c itemconfigure %lxLABEL -font {{%s} -%d %s} -fill #%06x "
             "-text {%s} \n",
        canvas, x, x->x_gui.x_font, x->x_gui.x_fontsize * IEMGUI_ZOOM(x),
        sys_fontweight,
        (x->x_gui.x_fsf.x_selected ? IEM_GUI_COLOR_SELECTED : x->x_gui.x_lcol),
        (strcmp(x->x_gui.x_lab->s_name, "empty") ? x->x_gui.x_lab->s_name
                                                 : ""));
    sys_vgui(".x%lx.c itemconfigure %lxKNOB -fill #%06x\n", canvas, x,
        x->x_gui.x_fcol);
    sys_vgui(".x%lx.c itemconfigure %lxBASE -fill #%06x\n", canvas, x,
        x->x_gui.x_bcol);
}

static void vslider_draw_io(t_vslider *x, t_glist *glist, int old_snd_rcv_flags)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    int iow = IOWIDTH * IEMGUI_ZOOM(x);
    int ioh = IEM_GUI_IOHEIGHT * IEMGUI_ZOOM(x);
    int tmargin = TMARGIN * IEMGUI_ZOOM(x);
    int bmargin = BMARGIN * IEMGUI_ZOOM(x);
    t_canvas *canvas = glist_getcanvas(glist);

    if((old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && !x->x_gui.x_fsf.x_snd_able)
    {
        sys_vgui(
            ".x%lx.c create rectangle %d %d %d %d -fill black -tags %lxOUT%d\n",
            canvas, xpos, ypos + x->x_gui.x_h + bmargin + IEMGUI_ZOOM(x) - ioh,
            xpos + iow, ypos + x->x_gui.x_h + bmargin, x, 0);
        /* keep these above outlet */
        sys_vgui(".x%lx.c raise %lxKNOB %lxOUT%d\n", canvas, x, x, 0);
        sys_vgui(".x%lx.c raise %lxLABEL %lxKNOB\n", canvas, x, x);
    }
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_SND_FLAG) && x->x_gui.x_fsf.x_snd_able)
        sys_vgui(".x%lx.c delete %lxOUT%d\n", canvas, x, 0);
    if((old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && !x->x_gui.x_fsf.x_rcv_able)
    {
        sys_vgui(
            ".x%lx.c create rectangle %d %d %d %d -fill black -tags %lxIN%d\n",
            canvas, xpos, ypos - tmargin, xpos + iow,
            ypos - tmargin - IEMGUI_ZOOM(x) + ioh, x, 0);
        /* keep these above inlet */
        sys_vgui(".x%lx.c raise %lxKNOB %lxIN%d\n", canvas, x, x, 0);
        sys_vgui(".x%lx.c raise %lxLABEL %lxKNOB\n", canvas, x, x);
    }
    if(!(old_snd_rcv_flags & IEM_GUI_OLD_RCV_FLAG) && x->x_gui.x_fsf.x_rcv_able)
        sys_vgui(".x%lx.c delete %lxIN%d\n", canvas, x, 0);
}

static void vslider_draw_select(t_vslider *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);

    if(x->x_gui.x_fsf.x_selected)
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x,
            IEM_GUI_COLOR_SELECTED);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%06x\n", canvas, x,
            IEM_GUI_COLOR_SELECTED);
    }
    else
    {
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline #%06x\n", canvas, x,
            IEM_GUI_COLOR_NORMAL);
        sys_vgui(".x%lx.c itemconfigure %lxLABEL -fill #%06x\n", canvas, x,
            x->x_gui.x_lcol);
    }
}

void vslider_draw(t_vslider *x, t_glist *glist, int mode)
{
    if(mode == IEM_GUI_DRAW_MODE_UPDATE)
    {
        sys_queuegui(x, glist, vslider_draw_update);
    }
    else if(mode == IEM_GUI_DRAW_MODE_MOVE)
    {
        vslider_draw_move(x, glist);
    }
    else if(mode == IEM_GUI_DRAW_MODE_NEW)
    {
        vslider_draw_new(x, glist);
    }
    else if(mode == IEM_GUI_DRAW_MODE_SELECT)
    {
        vslider_draw_select(x, glist);
    }
    else if(mode == IEM_GUI_DRAW_MODE_ERASE)
    {
        vslider_draw_erase(x, glist);
    }
    else if(mode == IEM_GUI_DRAW_MODE_CONFIG)
    {
        vslider_draw_config(x, glist);
    }
    else if(mode >= IEM_GUI_DRAW_MODE_IO)
    {
        vslider_draw_io(x, glist, mode - IEM_GUI_DRAW_MODE_IO);
    }
}

/* ------------------------ vsl widgetbehaviour----------------------------- */

static void vslider_getrect(
    t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_vslider *x = (t_vslider *) z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist) - TMARGIN * glist_getzoom(glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_h + (TMARGIN + BMARGIN) * glist_getzoom(glist);
}

static void vslider_save(t_gobj *z, t_binbuf *b)
{
    t_vslider *x = (t_vslider *) z;
    t_symbol *bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiffiisssiiiisssii", gensym("#X"), gensym("obj"),
        (int) x->x_gui.x_obj.te_xpix, (int) x->x_gui.x_obj.te_ypix,
        gensym("vsl"), x->x_gui.x_w / IEMGUI_ZOOM(x),
        x->x_gui.x_h / IEMGUI_ZOOM(x), (t_float) x->x_min, (t_float) x->x_max,
        x->x_lin0_log1, iem_symargstoint(&x->x_gui.x_isa), srl[0], srl[1],
        srl[2], x->x_gui.x_ldx, x->x_gui.x_ldy,
        iem_fstyletoint(&x->x_gui.x_fsf), x->x_gui.x_fontsize, bflcol[0],
        bflcol[1], bflcol[2], x->x_gui.x_isa.x_loadinit ? x->x_val : 0,
        x->x_steady);
    binbuf_addv(b, ";");
}

void vslider_check_height(t_vslider *x, int h)
{
    if(h < IEM_SL_MINSIZE * IEMGUI_ZOOM(x)) h = IEM_SL_MINSIZE * IEMGUI_ZOOM(x);
    x->x_gui.x_h = h;
    if(x->x_val > (x->x_gui.x_h * 100 - 100))
    {
        x->x_pos = x->x_gui.x_h * 100 - 100;
        x->x_val = x->x_pos;
    }
    if(x->x_lin0_log1)
    {
        x->x_k = log(x->x_max / x->x_min) /
                 (double) (x->x_gui.x_h / IEMGUI_ZOOM(x) - 1);
    }
    else
    {
        x->x_k = (x->x_max - x->x_min) /
                 (double) (x->x_gui.x_h / IEMGUI_ZOOM(x) - 1);
    }
}

void vslider_check_minmax(t_vslider *x, double min, double max)
{
    if(x->x_lin0_log1)
    {
        if((min == 0.0) && (max == 0.0)) max = 1.0;
        if(max > 0.0)
        {
            if(min <= 0.0) min = 0.01 * max;
        }
        else
        {
            if(min > 0.0) max = 0.01 * min;
        }
    }
    x->x_min = min;
    x->x_max = max;
    if(x->x_lin0_log1)
    {
        x->x_k = log(x->x_max / x->x_min) /
                 (double) (x->x_gui.x_h / IEMGUI_ZOOM(x) - 1);
    }
    else
    {
        x->x_k = (x->x_max - x->x_min) /
                 (double) (x->x_gui.x_h / IEMGUI_ZOOM(x) - 1);
    }
}

static void vslider_properties(t_gobj *z, t_glist *owner)
{
    t_vslider *x = (t_vslider *) z;
    char buf[800];
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);

    sprintf(buf, "pdtk_iemgui_dialog %%s |vsl| \
            --------dimensions(pix)(pix):-------- %d %d width: %d %d height: \
            -----------output-range:----------- %g bottom: %g top: %d \
            %d lin log %d %d empty %d \
            %s %s \
            %s %d %d \
            %d %d \
            #%06x #%06x #%06x\n",
        x->x_gui.x_w / IEMGUI_ZOOM(x), IEM_GUI_MINSIZE,
        x->x_gui.x_h / IEMGUI_ZOOM(x), IEM_SL_MINSIZE, x->x_min, x->x_max,
        0, /*no_schedule*/
        x->x_lin0_log1, x->x_gui.x_isa.x_loadinit, x->x_steady,
        -1, /*no multi, but iem-characteristic*/
        srl[0]->s_name, srl[1]->s_name, srl[2]->s_name, x->x_gui.x_ldx,
        x->x_gui.x_ldy, x->x_gui.x_fsf.x_font_style, x->x_gui.x_fontsize,
        0xffffff & x->x_gui.x_bcol, 0xffffff & x->x_gui.x_fcol,
        0xffffff & x->x_gui.x_lcol);
    gfxstub_new(&x->x_gui.x_obj.ob_pd, x, buf);
}

/* compute numeric value (fval) from pixel location (val) and range */
static t_float vslider_getfval(t_vslider *x)
{
    t_float fval;
    int zoomval = (x->x_gui.x_fsf.x_finemoved)
                      ? x->x_val / IEMGUI_ZOOM(x)
                      : (x->x_val / (100 * IEMGUI_ZOOM(x))) * 100;

    if(x->x_lin0_log1)
    {
        fval = x->x_min * exp(x->x_k * (double) (zoomval) *0.01);
    }
    else
    {
        fval = (double) (zoomval) *0.01 * x->x_k + x->x_min;
    }
    if((fval < 1.0e-10) && (fval > -1.0e-10)) fval = 0.0;
    return (fval);
}

static void vslider_bang(t_vslider *x)
{
    double out;

    if(pd_compatibilitylevel < 46)
    {
        out = vslider_getfval(x);
    }
    else
    {
        out = x->x_fval;
    }
    outlet_float(x->x_gui.x_obj.ob_outlet, out);
    if(x->x_gui.x_fsf.x_snd_able && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, out);
}

static void vslider_dialog(t_vslider *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *srl[3];
    int w = (int) atom_getfloatarg(0, argc, argv);
    int h = (int) atom_getfloatarg(1, argc, argv) * IEMGUI_ZOOM(x);
    double min = (double) atom_getfloatarg(2, argc, argv);
    double max = (double) atom_getfloatarg(3, argc, argv);
    int lilo = (int) atom_getfloatarg(4, argc, argv);
    int steady = (int) atom_getfloatarg(17, argc, argv);
    int sr_flags;
    t_atom undo[18];

    iemgui_setdialogatoms(&x->x_gui, 18, undo);
    SETFLOAT(undo + 2, x->x_min);
    SETFLOAT(undo + 3, x->x_max);
    SETFLOAT(undo + 4, x->x_lin0_log1);
    SETFLOAT(undo + 17, x->x_steady);

    pd_undo_set_objectstate(
        x->x_gui.x_glist, (t_pd *) x, gensym("dialog"), 18, undo, argc, argv);

    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady)
    {
        x->x_steady = 1;
    }
    else
    {
        x->x_steady = 0;
    }
    sr_flags = iemgui_dialog(&x->x_gui, srl, argc, argv);
    x->x_gui.x_w = iemgui_clip_size(w) * IEMGUI_ZOOM(x);
    vslider_check_height(x, h);
    vslider_check_minmax(x, min, max);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO + sr_flags);
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text *) x);
}

static void vslider_motion(
    t_vslider *x, t_floatarg dx, t_floatarg dy, t_floatarg up)
{
    int old = x->x_val;
    if(up != 0) return;

    if(x->x_gui.x_fsf.x_finemoved)
    {
        x->x_pos -= (int) dy;
    }
    else
    {
        x->x_pos -= 100 * (int) dy;
    }
    x->x_val = x->x_pos;
    if(x->x_val > (100 * x->x_gui.x_h - 100))
    {
        x->x_val = 100 * x->x_gui.x_h - 100;
        x->x_pos += 50;
        x->x_pos -= x->x_pos % 100;
    }
    if(x->x_val < 0)
    {
        x->x_val = 0;
        x->x_pos -= 50;
        x->x_pos -= x->x_pos % 100;
    }
    x->x_fval = vslider_getfval(x);
    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        vslider_bang(x);
    }
}

static void vslider_click(t_vslider *x, t_floatarg xpos, t_floatarg ypos,
    t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    if(!x->x_steady)
    {
        x->x_val =
            (int) (100.0 *
                   (x->x_gui.x_h +
                       text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist) - ypos));
    }
    if(x->x_val > (100 * x->x_gui.x_h - 100))
        x->x_val = 100 * x->x_gui.x_h - 100;
    if(x->x_val < 0) x->x_val = 0;
    x->x_fval = vslider_getfval(x);
    x->x_pos = x->x_val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    vslider_bang(x);
    glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
        (t_glistmotionfn) vslider_motion, 0, xpos, ypos);
}

static int vslider_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix,
    int shift, int alt, int dbl, int doit)
{
    t_vslider *x = (t_vslider *) z;

    if(doit)
    {
        vslider_click(x, (t_floatarg) xpix, (t_floatarg) ypix,
            (t_floatarg) shift, 0, (t_floatarg) alt);
        if(shift)
        {
            x->x_gui.x_fsf.x_finemoved = 1;
        }
        else
        {
            x->x_gui.x_fsf.x_finemoved = 0;
        }
    }
    return (1);
}

static void vslider_set(t_vslider *x, t_floatarg f)
{
    int old = x->x_val;
    double g;

    x->x_fval = f;
    if(x->x_min > x->x_max)
    {
        if(f > x->x_min) f = x->x_min;
        if(f < x->x_max) f = x->x_max;
    }
    else
    {
        if(f > x->x_max) f = x->x_max;
        if(f < x->x_min) f = x->x_min;
    }
    if(x->x_lin0_log1)
    {
        g = log(f / x->x_min) / x->x_k;
    }
    else
    {
        g = (f - x->x_min) / x->x_k;
    }
    x->x_val = IEMGUI_ZOOM(x) * (int) (100.0 * g + 0.49999);
    x->x_pos = x->x_val;
    if(x->x_val != old)
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void vslider_float(t_vslider *x, t_floatarg f)
{
    vslider_set(x, f);
    if(x->x_gui.x_fsf.x_put_in2out) vslider_bang(x);
}

static void vslider_size(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    x->x_gui.x_w =
        iemgui_clip_size((int) atom_getfloatarg(0, ac, av)) * IEMGUI_ZOOM(x);
    if(ac > 1)
    {
        vslider_check_height(
            x, (int) atom_getfloatarg(1, ac, av) * IEMGUI_ZOOM(x));
    }
    iemgui_size((void *) x, &x->x_gui);
}

static void vslider_delta(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_delta((void *) x, &x->x_gui, s, ac, av);
}

static void vslider_pos(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_pos((void *) x, &x->x_gui, s, ac, av);
}

static void vslider_range(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    vslider_check_minmax(x, (double) atom_getfloatarg(0, ac, av),
        (double) atom_getfloatarg(1, ac, av));
}

static void vslider_color(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_color((void *) x, &x->x_gui, s, ac, av);
}

static void vslider_send(t_vslider *x, t_symbol *s)
{
    iemgui_send(x, &x->x_gui, s);
}

static void vslider_receive(t_vslider *x, t_symbol *s)
{
    iemgui_receive(x, &x->x_gui, s);
}

static void vslider_label(t_vslider *x, t_symbol *s)
{
    iemgui_label((void *) x, &x->x_gui, s);
}

static void vslider_label_pos(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_pos((void *) x, &x->x_gui, s, ac, av);
}

static void vslider_label_font(t_vslider *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_font((void *) x, &x->x_gui, s, ac, av);
}

static void vslider_log(t_vslider *x)
{
    x->x_lin0_log1 = 1;
    vslider_check_minmax(x, x->x_min, x->x_max);
}

static void vslider_lin(t_vslider *x)
{
    x->x_lin0_log1 = 0;
    x->x_k =
        (x->x_max - x->x_min) / (double) (x->x_gui.x_h / IEMGUI_ZOOM(x) - 1);
}

static void vslider_init(t_vslider *x, t_floatarg f)
{
    x->x_gui.x_isa.x_loadinit = (f == 0.0) ? 0 : 1;
}

static void vslider_steady(t_vslider *x, t_floatarg f)
{
    x->x_steady = (f == 0.0) ? 0 : 1;
}

static void vslider_zoom(t_vslider *x, t_floatarg f)
{
    /* scale current pixel value */
    x->x_val = (IEMGUI_ZOOM(x) == 2 ? (x->x_val) / 2 : (x->x_val) * 2);
    x->x_pos = x->x_val;
    iemgui_zoom(&x->x_gui, f);
}

static void vslider_loadbang(t_vslider *x, t_floatarg action)
{
    if(action == LB_LOAD && x->x_gui.x_isa.x_loadinit)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        vslider_bang(x);
    }
}

static void *vslider_new(t_symbol *s, int argc, t_atom *argv)
{
    t_vslider *x = (t_vslider *) pd_new(vslider_class);
    int w = IEM_GUI_DEFAULTSIZE;
    int h = IEM_SL_DEFAULTSIZE;
    int lilo = 0;
    int f = 0;
    int ldx = 0;
    int ldy = -9;
    int fs = 10;
    int steady = 1;
    double min = 0.0;
    double max = (double) (IEM_SL_DEFAULTSIZE - 1);
    char str[144];
    float v = 0;

    iem_inttosymargs(&x->x_gui.x_isa, 0);
    iem_inttofstyle(&x->x_gui.x_fsf, 0);

    x->x_gui.x_bcol = 0xFCFCFC;
    x->x_gui.x_fcol = 0x00;
    x->x_gui.x_lcol = 0x00;

    if(((argc == 17) || (argc == 18)) && IS_A_FLOAT(argv, 0) &&
        IS_A_FLOAT(argv, 1) && IS_A_FLOAT(argv, 2) && IS_A_FLOAT(argv, 3) &&
        IS_A_FLOAT(argv, 4) && IS_A_FLOAT(argv, 5) &&
        (IS_A_SYMBOL(argv, 6) || IS_A_FLOAT(argv, 6)) &&
        (IS_A_SYMBOL(argv, 7) || IS_A_FLOAT(argv, 7)) &&
        (IS_A_SYMBOL(argv, 8) || IS_A_FLOAT(argv, 8)) && IS_A_FLOAT(argv, 9) &&
        IS_A_FLOAT(argv, 10) && IS_A_FLOAT(argv, 11) && IS_A_FLOAT(argv, 12) &&
        IS_A_FLOAT(argv, 16))
    {
        w = (int) atom_getfloatarg(0, argc, argv);
        h = (int) atom_getfloatarg(1, argc, argv);
        min = (double) atom_getfloatarg(2, argc, argv);
        max = (double) atom_getfloatarg(3, argc, argv);
        lilo = (int) atom_getfloatarg(4, argc, argv);
        iem_inttosymargs(&x->x_gui.x_isa, atom_getfloatarg(5, argc, argv));
        iemgui_new_getnames(&x->x_gui, 6, argv);
        ldx = (int) atom_getfloatarg(9, argc, argv);
        ldy = (int) atom_getfloatarg(10, argc, argv);
        iem_inttofstyle(&x->x_gui.x_fsf, atom_getfloatarg(11, argc, argv));
        fs = (int) atom_getfloatarg(12, argc, argv);
        iemgui_all_loadcolors(&x->x_gui, argv + 13, argv + 14, argv + 15);
        v = atom_getfloatarg(16, argc, argv);
    }
    else
        iemgui_new_getnames(&x->x_gui, 6, 0);
    if((argc == 18) && IS_A_FLOAT(argv, 17))
        steady = (int) atom_getfloatarg(17, argc, argv);
    x->x_gui.x_draw = (t_iemfunptr) vslider_draw;
    x->x_gui.x_fsf.x_snd_able = 1;
    x->x_gui.x_fsf.x_rcv_able = 1;
    x->x_gui.x_glist = (t_glist *) canvas_getcurrent();
    if(x->x_gui.x_isa.x_loadinit)
    {
        x->x_val = v;
    }
    else
    {
        x->x_val = 0;
    }
    x->x_pos = x->x_val;
    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady != 0) steady = 1;
    x->x_steady = steady;
    if(!strcmp(x->x_gui.x_snd->s_name, "empty")) x->x_gui.x_fsf.x_snd_able = 0;
    if(!strcmp(x->x_gui.x_rcv->s_name, "empty")) x->x_gui.x_fsf.x_rcv_able = 0;
    if(x->x_gui.x_fsf.x_font_style == 1)
    {
        strcpy(x->x_gui.x_font, "helvetica");
    }
    else if(x->x_gui.x_fsf.x_font_style == 2)
    {
        strcpy(x->x_gui.x_font, "times");
    }
    else
    {
        x->x_gui.x_fsf.x_font_style = 0;
        strcpy(x->x_gui.x_font, sys_font);
    }
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if(fs < 4) fs = 4;
    x->x_gui.x_fontsize = fs;
    x->x_gui.x_w = iemgui_clip_size(w);
    vslider_check_height(x, h);
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    iemgui_newzoom(&x->x_gui);
    vslider_check_minmax(x, min, max);
    outlet_new(&x->x_gui.x_obj, &s_float);
    x->x_fval = vslider_getfval(x);
    return (x);
}

static void vslider_free(t_vslider *x)
{
    if(x->x_gui.x_fsf.x_rcv_able)
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

void g_vslider_setup(void)
{
    vslider_class = class_new(gensym("vsl"), (t_newmethod) vslider_new,
        (t_method) vslider_free, sizeof(t_vslider), 0, A_GIMME, 0);
    class_addcreator((t_newmethod) vslider_new, gensym("vslider"), A_GIMME, 0);
    class_addbang(vslider_class, vslider_bang);
    class_addfloat(vslider_class, vslider_float);
    class_addmethod(vslider_class, (t_method) vslider_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(vslider_class, (t_method) vslider_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_dialog, gensym("dialog"), A_GIMME, 0);
    class_addmethod(vslider_class, (t_method) vslider_loadbang,
        gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_size, gensym("size"), A_GIMME, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_range, gensym("range"), A_GIMME, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_color, gensym("color"), A_GIMME, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(vslider_class, (t_method) vslider_receive,
        gensym("receive"), A_DEFSYM, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(vslider_class, (t_method) vslider_label_pos,
        gensym("label_pos"), A_GIMME, 0);
    class_addmethod(vslider_class, (t_method) vslider_label_font,
        gensym("label_font"), A_GIMME, 0);
    class_addmethod(vslider_class, (t_method) vslider_log, gensym("log"), 0);
    class_addmethod(vslider_class, (t_method) vslider_lin, gensym("lin"), 0);
    class_addmethod(
        vslider_class, (t_method) vslider_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_steady, gensym("steady"), A_FLOAT, 0);
    class_addmethod(
        vslider_class, (t_method) vslider_zoom, gensym("zoom"), A_CANT, 0);
    vslider_widgetbehavior.w_getrectfn = vslider_getrect;
    vslider_widgetbehavior.w_displacefn = iemgui_displace;
    vslider_widgetbehavior.w_selectfn = iemgui_select;
    vslider_widgetbehavior.w_activatefn = NULL;
    vslider_widgetbehavior.w_deletefn = iemgui_delete;
    vslider_widgetbehavior.w_visfn = iemgui_vis;
    vslider_widgetbehavior.w_clickfn = vslider_newclick;
    class_setwidget(vslider_class, &vslider_widgetbehavior);
    class_sethelpsymbol(vslider_class, gensym("vslider"));
    class_setsavefn(vslider_class, vslider_save);
    class_setpropertiesfn(vslider_class, vslider_properties);
}
