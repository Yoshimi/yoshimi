/*
    DynamicTooltip.h

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul
    Copyright 2009-2010, Alan Calvert
    Copyright 2016 Will Godfrey
    Copyright 2017 Jesper Lloyd

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

    This file is a derivative of the ZynAddSubFX original, modified March 2017
*/

#ifndef DynamicTooltip_h
#define DynamicTooltip_h

#include <FL/Fl_Menu_Window.H>
#include "UI/MiscGui.h"

/*
  Tooltip window used for dynamic, formatted messages
  for valuator widgets (dials, sliders, etc.)
*/
class DynTooltip : private Fl_Menu_Window {

 public:
  DynTooltip();

  void setValue(float);
  void setValueType(ValueType vt);
  void setGraphicsType(ValueType gv_);
  void setTooltipText(string tt_text);
  void setOnlyValue(bool onlyval);

  void hide();
  void show();

  void setOffset(int x, int y);
  void draw();

 private:
  void reposition();
  void update();

  float currentValue;

  string tipText;
  string valueText;

  ValueType valueType;
  ValueType graphicsType;
  bool onlyValue;

  bool positioned;
  int tipTextW, tipTextH;
  int valTextW, valTextH;
  int graphW, graphH;

  /* relative tooltip position */
  int xoffs, yoffs;
};

/*
  Interface to allow for shared behaviour when handling
  events of dynamic tooltips.
*/
class DynTipped {
 public:

  /* Set whether tooltip is visible or not */
  virtual void tipShow(bool) = 0;

  /* Set whether or not to show only value, or description + value */
  virtual void tipOnlyValue(bool) = 0;

  /* Set ValueType used to format the value */
  virtual void setValueType(ValueType vt) = 0;

  /* Set the type for supplementary graphics, when applicable */
  virtual void setGraphicsType(ValueType vt) = 0;
};


/*
  Standard behaviour for showing/hiding/switching dynamic tooltips
*/
void stdDynTipHandle(DynTipped*,int event);

#endif
