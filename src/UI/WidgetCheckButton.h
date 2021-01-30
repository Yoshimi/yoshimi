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

#ifndef Fl_Light_Button2_H
#define Fl_Light_Button2_H

#include <FL/Fl_Button.H>

/**
  Custom checkbutton, w. dynamically resized check mark
*/
class Fl_Light_Button2 : public Fl_Button {
protected:
    virtual void draw();
public:
    virtual int handle(int);
    Fl_Light_Button2(int x,int y,int w,int h,const char *l = 0);
};

class Fl_Check_Button2 : public Fl_Light_Button2 {
public:
  Fl_Check_Button2(int X, int Y, int W, int H, const char *L = 0);
};


#endif
