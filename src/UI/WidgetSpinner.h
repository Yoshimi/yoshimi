/*
    Custom Checkbox

    Original ZynAddSubFX author Nasca Octavian Paul
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

#ifndef WidgetSpinner_H
#define WidgetSpinner_H

#include <FL/Fl_Spinner.H>

/**
  Custom spinner with dynamically sized button labels
*/
class WidgetSpinner : public Fl_Spinner {
  protected:
    Fl_Repeat_Button
    		*up_button_,		// Up button
    		*down_button_;		// Down button
public:
  WidgetSpinner(int x,int y,int w,int h,const char *l = 0);
  void labelsize(int size) {
    Fl_Spinner::labelsize(size);
    this->up_button_->labelsize(1 + size/5);
    this->down_button_->labelsize(1 + size/5);
    // TODO: find out where to move this, putting it in
    // the constructor does not work.
    this->box(FL_FLAT_BOX);
    this->color(FL_BACKGROUND2_COLOR);
  }
};

#endif
