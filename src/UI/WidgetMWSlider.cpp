/*
    WidgetMWSlider.cpp - Mousewheel controllable Fl_Slider widgets

    Idea developed from ZynAddSubFX Pdial
    Copyright 2016 Rob Couto & Will Godfrey
    Copyright 2017 Jesper Lloyd
    Copyright 2019 Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

    This file is a derivative of the ZynAddSubFX original

*/

#include "WidgetMWSlider.h"

#include <cmath>

/*
  Fl_Value_Slider supplying additional mousewheel control with variable precision.
*/
mwheel_val_slider::mwheel_val_slider(int x, int y, int w, int h, const char *l)
    : Fl_Value_Slider (x,y,w,h,l)
{
    Fl_Group *save = Fl_Group::current();
    dyntip = new DynTooltip();
    Fl_Group::current(save);

    customTip = false;
    tipText.clear();
    reverse = 1;
}

mwheel_val_slider::~mwheel_val_slider()
{
    delete dyntip;
}

/* Support for the dynamic tooltip interface */

void mwheel_val_slider::setValueType(ValueType vt)
{
    dyntip->setValueType(vt);
}

void mwheel_val_slider::setGraphicsType(ValueType vt)
{
    dyntip->setGraphicsType(vt);
}

void mwheel_val_slider::useCustomTip(bool enabled)
{
    if (!enabled)
    {
        dyntip->hide();
    }
    customTip = enabled;
    if (!tipText.empty())
        tooltip(tipText.c_str());
}

int mwheel_val_slider::value(double val)
{
    dyntip->setValue(val);
    dyntip->setOnlyValue(true);
    return Fl_Valuator::value(val);
}

double mwheel_val_slider::value()
{
    return Fl_Valuator::value();
}

void mwheel_val_slider::tooltip(const char* tip)
{
     if (tip)
     {
         tipText = string(tip);
         dyntip->setTooltipText(tipText);
     }
    /* Call base class with empty string to prevent
       potential parent group tooltip from showing */
    if (customTip)
    {
        Fl_Widget::tooltip("");
    } else {
        Fl_Widget::tooltip(tip);
    }
}

/*
  Helper function for quick reimplementations under different super class calls
*/
int mwheel_val_slider::_handle(int res, int event)
{

    switch(event)
    {

    case FL_MOUSEWHEEL: {
        if (!Fl::event_inside(this))
        {
            return 1;
        }
        double range = std::abs(maximum() - minimum());
        int step_size = (reverse * Fl::event_dy() > 0) ? 1 : -1;

        if (Fl::event_state(FL_CTRL) != 0)
        {
            step_size *= step();
            if (range > 256) // Scale stepping for large ranges
                step_size *= 50;
        } else {
            step_size *= range / 20;
        }

        value(clamp(increment(value(), step_size)));
        do_callback();
        res = 1;
        break;
    }
    case FL_PUSH:
        Fl::belowmouse(this);
        do_callback();
        res = 1;
        break;
    }

    if (customTip)
    {
        dyntip->setValue(value());
        dyntip->tipHandle(event);
    }

    return res;
}

int mwheel_val_slider::handle(int event)
{
    return _handle(Fl_Value_Slider::handle(event), event);
}

mwheel_val_slider_rev::mwheel_val_slider_rev(int x, int y, int w, int h, const char *l)
    : mwheel_val_slider (x,y,w,h,l)
{
    reverse = -1;
}

/*
   Derived classes - uses standard Fl_Slider drawing and handling
*/
mwheel_slider::mwheel_slider(int x, int y, int w, int h, const char *l)
    : mwheel_val_slider (x,y,w,h,l)
{
    reverse = 1;
}

void mwheel_slider::draw()
{
    Fl_Slider::draw();
}

int mwheel_slider::handle(int event)
{
    return _handle(Fl_Slider::handle(event), event);
}

mwheel_slider_rev::mwheel_slider_rev(int x, int y, int w, int h, const char *l)
    : mwheel_slider (x,y,w,h,l)
{
    reverse = -1;
}
