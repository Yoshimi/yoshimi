/*
    Custom Spinner

    Copyright 2021 William Godfrey

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

    This file is derived from (GPL2) fltk 1.3.5 source code.
*/

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include "WidgetSpinner.h"

WidgetSpinner::WidgetSpinner(int X, int Y, int W, int H, const char *L)
  : Fl_Spinner(X, Y, W, H, L)
{
  this->up_button_ = (Fl_Repeat_Button*) this->child(1);
  this->up_button_->label("@+42<");
  this->down_button_ = (Fl_Repeat_Button*) this->child(2);
  this->down_button_->label("@+42>");
}
