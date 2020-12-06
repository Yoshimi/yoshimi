/*
    YoshiWin.h

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

#ifndef YoshiWin_h
#define YoshiWin_h

#include <FL/Fl_Double_Window.H>
#include "UI/MiscGui.h"


class YoshiWin : public Fl_Double_Window {
 public:
  YoshiWin(int x,int y, int w, int h, const char *label=0);
  ~YoshiWin();

  void resize(int x, int y, int w, int h);
};
#endif
