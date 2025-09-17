/*
    ScaleTrackedWindow.cpp - extension of FL_Window that tracks dimension changes and updates tooltip text size.

    Copyright 2025 Jesper Lloyd

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License (version 2 or
    later) for more details.

    You should have received a copy of the GNU General Public License along with
    yoshimi; if not, write to the Free Software Foundation, Inc., 51 Franklin
    Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "ScaleTrackedWindow.h"

#include <FL/Fl_Tooltip.H>

ScaleTrackedWindow::ScaleTrackedWindow(int x, int y, int w, int h, const char *l) : Fl_Double_Window(x,y,w,h,l) {
    defaultH=h;
    defaultW=w;
    _dScale = 1;
}

ScaleTrackedWindow::ScaleTrackedWindow(int w, int h, const char *l) : Fl_Double_Window(w,h,l) {
    defaultH=h;
    defaultW=w;
    _dScale = 1;
}

void ScaleTrackedWindow::resize(int x, int y, int w, int h)
{
    Fl_Double_Window::resize(x, y, w, h);
    _dScale = (float) w / (float) defaultW;
    Fl_Tooltip::size(tooltipSize());
}

void ScaleTrackedWindow::reset(int defaultW, int defaultH) {
    this->defaultW = defaultW;
    this->defaultH = defaultH;
    _dScale = (float) w() / (float) defaultW;
    Fl_Tooltip::size(tooltipSize());
}


int ScaleTrackedWindow::handle(int ev) {
    int result = Fl_Double_Window::handle(ev);
    if (ev == FL_FOCUS || ev == FL_ENTER) {
        Fl_Tooltip::size(tooltipSize());
        result = 1;
    }
    return result;
}


int ScaleTrackedWindow::tooltipSize() const {
    float result = _dScale * 10;
    if (result < 12)
    {
        result = 12;
    }
    return int(result);
}

float ScaleTrackedWindow::dScale() const {
    return _dScale;
}
