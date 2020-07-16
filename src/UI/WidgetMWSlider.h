/*
    WidgetMWSlider.h - Mousewheel controllable Fl_Slider widgets

    Idea developed from ZynAddSubFX Pdial
    Copyright 2016 Rob Couto & Will Godfrey
    Copyright 2017 Jesper Lloyd

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

#ifndef WidgetMWSlider_h
#define WidgetMWSlider_h

#include <FL/Fl_Value_Slider.H>
#include "UI/DynamicTooltip.h"

class mwheel_val_slider : public Fl_Value_Slider {
 public:
  mwheel_val_slider(int x, int y, int w, int h, const char *l=0) ;
  ~mwheel_val_slider();

  void useCustomTip(bool);

  /* DynTipped methods */
  void setValueType(ValueType vt);
  void setGraphicsType(ValueType vt);

  /* Overridden widget methods */
  int handle(int ev);
  void tooltip(const char* tip);
  int value(double);
  double value();

 protected:
  /* Shared handle behaviour */
  int _handle(int result, int event);

  int reverse;
  bool customTip;
  DynTooltip *dyntip;
 private:
  string tipText;
};

class mwheel_val_slider_rev : public mwheel_val_slider {
 public:
  mwheel_val_slider_rev(int x, int y, int w, int h, const char *l=0) ;
};

/*
  The intuitive inheritance is reversed for convenience,
  since only the drawing and handling calls differ in the base class.
*/
class mwheel_slider : public mwheel_val_slider {
 public:
  mwheel_slider(int x, int y, int w, int h, const char *l=0) ;
  void draw();
  int handle(int);
};

class mwheel_slider_rev : public mwheel_slider {
 public:
  mwheel_slider_rev(int x, int y, int w, int h, const char *l=0) ;
};

#endif
