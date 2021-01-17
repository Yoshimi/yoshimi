/*
    YoshiWin.cpp
    Copyright 2020, Will Godfrey

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

*/

/*
 *
 * This code is no longer used but is mothballed for future refence


#include "YoshiWin.h"
#include "Misc/NumericFuncs.h"

#include <iostream>

#include <FL/fl_draw.H>
#include <FL/x.H>

#include <FL/Fl.H>

using func::limit;

YoshiWin::YoshiWin(int x,int y, int w, int h, const char *label) : Fl_Double_Window(x,y,w,h,label)
{
    ;
}

YoshiWin::~YoshiWin()
{
    ;
}

void textResize(void* done) {

  //printf("Timeout expired!\n");
  if (done != NULL)
  {
      Fl_Double_Window* currentWin = reinterpret_cast<Fl_Double_Window*>(done);
      currentWin->do_callback();
  }
}

void YoshiWin::resize(int x, int y, int w, int h)
{
  Fl_Double_Window::resize(x, y, w, h);
  //std::cout << "Resized: x" << x << "  y" << y << "  w " << w << "  h " << h << std::endl;

  Fl::remove_timeout(textResize, NULL);
  Fl::add_timeout(0.2, textResize, this);
  // ensure at least one refresh within FLTK update
}
*/

/*
 * In the header
 *
 * decl {\#include "UI/YoshiWin.h"} {public local
}
 * The code below should be placed in the window's callback
 * and 'windowRtext()' then used to adjust text etc.
 * every time the window is resized.
 *
if (Fl::event() == FL_CLOSE)
              o->hide();
          else if (Fl::event() == FL_MOVE || Fl::event() == FL_FULLSCREEN)
              windowRtext();}
*/
