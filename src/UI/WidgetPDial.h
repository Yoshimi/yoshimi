/*
    WidgetPDial.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2016 Will Godfrey
    Copyright 2017 Jesper Lloyd
    Copyright 2018 Will Godfrey & others

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

#ifndef WidgetPDial_h
#define WidgetPDial_h

#include <FL/Fl.H>
#include <FL/Fl_Dial.H>
#include <FL/Fl_Menu_Window.H>
#include "UI/MiscGui.h"
#include "UI/DynamicTooltip.h"

/*
  Dial widget with custom drawing and input handling.
  Supports dynamic tooltips and adjustable default values.
*/
class WidgetPDial : public Fl_Dial {
 public:
  WidgetPDial(int x,int y, int w, int h, const char *label=0);
  ~WidgetPDial();

  void setValueType(ValueType type_);
  void setGraphicsType(ValueType type_);

  void tooltip(const char * c);
  void value(double v);
  double value();
  int handle(int event);
  void draw();

 private:

  void drawgradient(int cx,int cy,int sx,double m1,double m2);
  void pdialcolor(int r,int g,int b);

  double oldvalue;
  DynTooltip *dyntip;
  float home;
};
#endif
