/*
    DynamicTooltip.cpp

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2016 Will Godfrey
    Copyright 2017 Jesper Lloyd
    Copyright 2018 Will Godfrey and others

    Idea originally derived from work by Greg Ercolano
    (http://seriss.com/people/erco/fltk/)

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

    This file is a derivative of the ZynAddSubFX original.

*/

#include "DynamicTooltip.h"

#include <FL/fl_draw.H>
#include <FL/Fl_Tooltip.H>

#include <UI/MiscGui.h>

#define MAX_TEXT_WIDTH 280


// Whether or not a dynamic tooltip was shown recently
static bool _recent;

/* Delayed display of tooltip - callbackk*/
static void delayedShow(void* dyntip){
    if (DynTooltip* tip = (DynTooltip*) dyntip)
       tip->dynshow(0);
}

static void resetRecent(void*){
    _recent = false;
}


DynTooltip::DynTooltip():Fl_Menu_Window(1,1)
{

    tipText.clear();
    valueText.clear();

    valueType = VC_plainValue;
    graphicsType = VC_plainValue;
    onlyValue = false;

    positioned = false;
    yoffs = 20;
    xoffs = 0;

    tipTextW = tipTextH = 0;
    valTextW = valTextH = 0;
    graphW = graphH = 0;

    set_override(); // place window on top
    end();
    hide();
}

DynTooltip::~DynTooltip(){
    Fl::remove_timeout(delayedShow);
    Fl::remove_timeout(resetRecent);
}

/*
   Overrides standard hide/show from Fl_Widget
   to update flags and set tooltip position
*/
void DynTooltip::hide()
{
    positioned = false;
    Fl_Menu_Window::hide();
}

void DynTooltip::dynshow(float timeout)
{
    if (timeout <= 0){
        Fl::remove_timeout(delayedShow, this);
        _recent = true;
        reposition();
        update();
        Fl_Menu_Window::show();
    } else {
        Fl::add_timeout(timeout, delayedShow, this);
    }
}

/*
  Sets the value to be formatted and shown in the tooltip
*/
void DynTooltip::setValue(float val)
{
    if (val != currentValue)
    {
        currentValue = val;
        if (positioned)
            update();
    }
}

/*
  Calling with true causes only the formatted value
  and the associated graphics to be shown in the tooltip.
*/
void DynTooltip::setOnlyValue(bool onlyval)
{
    if (onlyValue != onlyval)
    {
        onlyValue = onlyval;
        if (positioned)
            update();
    }
}

/*
  Sets the description of the dynamic value.
  Calling setOnlyValue(false) will prevent this from being
  displayed.
*/
void DynTooltip::setTooltipText(const string& tt_text)
{
    tipText = tt_text;
    tipTextW = MAX_TEXT_WIDTH;
    tipTextH = 0;

    /* Calculate & set dimensions of the tooltip text */
    fl_font(Fl_Tooltip::font(), Fl_Tooltip::size());
    fl_measure(tipText.c_str(), tipTextW, tipTextH, 0);

    if (positioned)
        update();
}

/*
  Set the type of the formatted value
*/
void DynTooltip::setValueType(ValueType vt)
{
    valueType = vt;
    if (positioned)
        update();
}

/*
  Set the graphics used alongside the formatted value, if any.
  Note: The graphicstype should probably always be the same as
  the valuetype, and this field may be removed in the future.
*/
void DynTooltip::setGraphicsType(ValueType gvt)
{
    graphicsType = gvt;
    custom_graph_dimensions(graphicsType, graphW, graphH);
    if (positioned)
        update();
}

/*
  Set the position of the tooltip relative to the position
  of the mouse when the tooltip is shown.
*/
void DynTooltip::setOffset(int x, int y)
{
    xoffs = x;
    yoffs = y;
}

/*
  Change the position of the tooltip unless it is already visible.
*/
inline void DynTooltip::reposition()
{
    if (!positioned)
    {
        position(Fl::event_x_root() + xoffs, Fl::event_y_root() + yoffs);
        positioned = true;
    }
}

/* For readability - trust the optimizer */
inline int max(int a, int b)
{
    return a >= b ? a : b;
}

/*
  Update the size parameters and message strings.
*/
void DynTooltip::update()
{

    /* Update formatted value */
    valueText = convert_value(valueType, currentValue);

    /* Calculate size bounds for the formatted value string */
    valTextW = MAX_TEXT_WIDTH;
    valTextH = 0;

    fl_font(Fl_Tooltip::font(), Fl_Tooltip::size());
    fl_measure(valueText.c_str(), valTextW, valTextH, 0);

    int _w = max(valTextW, graphW);
    int _h = valTextH + graphH;

    if (!onlyValue)
    {
        _w = max(_w, tipTextW);
        _h += tipTextH;
    }

    /* Add standard tooltip margins and set size*/
    _w += 6;//Fl_Tooltip::margin_width() * 2;
    _h += 6;//Fl_Tooltip::margin_height() * 2;

    size(_w, _h);
    redraw();
}


/*
  Use static style parameters for regular tooltips to draw the custom ones
*/
void DynTooltip::draw()
{

    const int mw = 3;//Fl_Tooltip::margin_width();
    const int mh = 3;//Fl_Tooltip::margin_height();

    int x = mw, y = mh;
    int _w = w() - mw * 2;

    draw_box(FL_BORDER_BOX, 0, 0, w(), h(), Fl_Tooltip::color());
    fl_color(Fl_Tooltip::textcolor());
    fl_font(Fl_Tooltip::font(), Fl_Tooltip::size());

    /* Draw tooltip text */
    if (!onlyValue)
    {
        fl_draw(tipText.c_str(), x, y, _w, tipTextH,
                Fl_Align((tipTextW < valTextW || tipTextW < graphW ?
                          FL_ALIGN_CENTER : FL_ALIGN_LEFT)| FL_ALIGN_WRAP));
        y += tipTextH;
    }

    /* Draw formatted tooltip value */
    fl_draw(valueText.c_str(), x, y, _w, valTextH,
            Fl_Align(FL_ALIGN_CENTER | FL_ALIGN_WRAP));

    /* Draw additional graphics */
    if (graphicsType != VC_plainValue)
        custom_graphics(graphicsType, currentValue, w(), h() - mh);
}

/*
  Standard tooltip behaviour
*/
void DynTooltip::tipHandle(int event)
{
    switch(event)
    {
    case FL_ENTER:
        Fl::remove_timeout(resetRecent);
        setOnlyValue(false);
        dynshow(_recent ? Fl_Tooltip::hoverdelay() : Fl_Tooltip::delay());
        break;
    case FL_PUSH:
    case FL_DRAG:
    case FL_MOUSEWHEEL:
        Fl::remove_timeout(delayedShow);
        Fl::remove_timeout(resetRecent);
        setOnlyValue(true);
        dynshow(0);
        break;
    case FL_LEAVE:
    case FL_RELEASE:
    case FL_HIDE:
        Fl::remove_timeout(delayedShow);
        Fl::add_timeout(Fl_Tooltip::hoverdelay(),resetRecent);
        hide();
        break;
    }
}
