/*
    WidgetPDial.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2016 Will Godfrey
    Copyright 2017 Jesper Lloyd
    Coyright 2020, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.   See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original.

*/

#include "WidgetPDial.h"
#include "Misc/NumericFuncs.h"

#include <FL/fl_draw.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Group.H>
#include <FL/x.H>
#include <cairo.h>
#include <cairo-xlib.h>

using func::limit;

WidgetPDial::WidgetPDial(int x,int y, int w, int h, const char *label) : Fl_Dial(x,y,w,h,label)
{
    Fl_Group *save = Fl_Group::current();
    dyntip = new DynTooltip();
    Fl_Group::current(save);

    oldvalue = 0.0;
}

WidgetPDial::~WidgetPDial()
{
    delete dyntip;
}

void WidgetPDial::setValueType(ValueType type_)
{
    dyntip->setValueType(type_);
}

void WidgetPDial::setGraphicsType(ValueType type_)
{
    dyntip->setGraphicsType(type_);
}

void WidgetPDial::tooltip(const char * tip)
{
    if (tip)
        dyntip->setTooltipText(tip);
}

/*
  Override these Fl_Valuator methods to update
  the tooltip value when the widget value is changed.
*/
void WidgetPDial::value(double val)
{
    Fl_Valuator::value(val);
    dyntip->setValue(val);
    dyntip->setOnlyValue(true);
}

double WidgetPDial::value()
{
    return Fl_Valuator::value();
}

int WidgetPDial::handle(int event)
{

    double dragsize, v, min = minimum(), max = maximum();
    int my, mx;

    int res = 0;

    switch (event)
    {
    case FL_PUSH:
    case FL_DRAG: // done this way to suppress warnings
        if (event == FL_PUSH)
        {
            Fl::belowmouse(this); /* Ensures other widgets receive FL_RELEASE */
            do_callback();
            oldvalue = value();
        }
        my = -((Fl::event_y() - y()) * 2 - h());
        mx = ((Fl::event_x() - x()) * 2 - w());
        my = (my + mx);

        dragsize = 200.0;
        if (Fl::event_state(FL_CTRL) != 0)
            dragsize *= 10;
        else if (Fl::event_button() == 2)
            dragsize *= 3;
        if (Fl::event_button() != 3)
        {
            v = oldvalue + my / dragsize * (max - min);
            v = clamp(v);
            value(v);
            value_damage();
            if (this->when() != 0)
                do_callback();
        }
        res = 1;
        break;
    case FL_MOUSEWHEEL:
        if (!Fl::event_inside(this))
        {
            return 1;
        }
        my = - Fl::event_dy();
        dragsize = 25.0f;
        if (Fl::event_state(FL_CTRL) != 0)
            dragsize *= 10;
        value(limit(value() + my / dragsize * (max - min), min, max));
        value_damage();
        if (this->when() != 0)
            do_callback();
        res = 1;
        break;
    case FL_ENTER:
        res = 1;
        break;
    case FL_HIDE:
    case FL_UNFOCUS:
        break;
    case FL_LEAVE:
        res = 1;
        break;
    case FL_RELEASE:
        if (this->when() == 0)
            do_callback();
        res = 1;
        break;
    }

    dyntip->setValue(value());
    dyntip->tipHandle(event);
    return res;
}

void WidgetPDial::drawgradient(int cx,int cy,int sx,double m1,double m2)
{
    for (int i = (int)(m1 * sx); i < (int)(m2 * sx); ++i)
    {
        double tmp = 1.0 - powf(i * 1.0 / sx, 2.0);
        pdialcolor(140 + (int) (tmp * 90), 140 + (int)(tmp * 90), 140 + (int)(tmp * 100));
        fl_arc(cx + sx / 2 - i / 2, cy + sx / 2 - i / 2, i, i, 0, 360);
    }
}

void WidgetPDial::draw()
{
    int cx = x(), cy = y(), sx = w(), sy = h();
    double d = (sx>sy)?sy:sx; // d = the smallest side -2
    double dh = d/2;
    fl_color(170,170,170);
    fl_pie(cx - 2, cy - 2, d + 4, d + 4, 0, 360);
    double val = (value() - minimum()) / (maximum() - minimum());
    cairo_t *cr;
    cairo_surface_t* Xsurface = cairo_xlib_surface_create
        (fl_display, fl_window, fl_visual->visual,Fl_Window::current()->w(),
         Fl_Window::current()->h());
    cr = cairo_create (Xsurface);
    cairo_translate(cr,cx+dh,cy+dh);
    //relative lengths of the various parts:
    double rCint = 10.5/35;
    double rCout = 13.0/35;
    double rHand = 8.0/35;
    double rGear = 15.0/35;
    //drawing base dark circle
    if (active_r())
    {
        cairo_pattern_create_rgb(51.0/255,51.0/255,51.0/255);
    } else {
        cairo_set_source_rgb(cr,0.4,0.4,0.4);
    }
    cairo_arc(cr,0,0,dh,0,2*M_PI);
    cairo_fill(cr);
    cairo_pattern_t* pat;
    //drawing the inner circle:
    pat = cairo_pattern_create_linear(0.5*dh,0.5*dh,0,-0.5*dh);
    cairo_pattern_add_color_stop_rgba (pat, 0, 0.8*186.0/255, 0.8*198.0/255, 0.8*211.0/255, 1);
    cairo_pattern_add_color_stop_rgba (pat, 1, 231.0/255, 235.0/255, 239.0/255, 1);
    cairo_set_source (cr, pat);
    cairo_arc(cr,0,0,d*rCout,0,2*M_PI);
    cairo_fill(cr);
    //drawing the outer circle:
    pat = cairo_pattern_create_radial(2.0/35*d,6.0/35*d,2.0/35*d,0,0,d*rCint);
    cairo_pattern_add_color_stop_rgba (pat, 0, 231.0/255, 235.0/255, 239.0/255, 1);
    cairo_pattern_add_color_stop_rgba (pat, 1, 186.0/255, 198.0/255, 211.0/255, 1);
    cairo_set_source (cr, pat);
    cairo_arc(cr,0,0,d*rCint,0,2*M_PI);
    cairo_fill(cr);
    //drawing the "light" circle:
    int linewidth = int(2.0f * sx / 30);
    if (linewidth < 2)
        linewidth = 2;
    if (active_r())
    {
        cairo_set_source_rgb(cr,0,197.0/255,245.0/255); //light blue
    } else {
        cairo_set_source_rgb(cr,0.6,0.7,0.8);
    }
    cairo_set_line_width (cr, linewidth);
    cairo_new_sub_path(cr);
    cairo_arc(cr,0,0,d*rGear,0.75*M_PI,+val*1.5*M_PI+0.75*M_PI);
    cairo_stroke(cr);
    //drawing the hand:
    if (active_r())
    {
        cairo_set_source_rgb(cr,61.0/255,61.0/255,61.0/255);
    } else {
        cairo_set_source_rgb(cr,111.0/255,111.0/255,111.0/255);
    }
    cairo_rotate(cr,val*3/2*M_PI+0.25*M_PI);
    cairo_set_line_width (cr, linewidth);
    cairo_move_to(cr,0,0);
    cairo_line_to(cr,0,d*rHand);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke (cr);
    //freeing the resources
    cairo_pattern_destroy(pat);
    cairo_surface_destroy(Xsurface);  cairo_destroy(cr);
}

inline void WidgetPDial::pdialcolor(int r,int g,int b)
{
    if (active_r())
        fl_color(r, g, b);
    else
        fl_color(160 - (160 - r) / 3, 160 - (160 - b) / 3, 160 - (160 - b) / 3);
}
