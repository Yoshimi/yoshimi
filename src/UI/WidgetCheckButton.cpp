/*
    Custom Checkbox

    Copyright 2021 Will Godfrey & others

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
#include "UI/WidgetCheckButton.h"


void Fl_Light_Button2::draw() {
  if (box()) draw_box(this==Fl::pushed() ? fl_down(box()) : box(), color());
  Fl_Color col = value() ? (active_r() ? selection_color() :
                            fl_inactive(selection_color())) : color();

  int W  = labelsize();
  int bx = Fl::box_dx(box());     // box frame width
  int dx = bx + 2;                // relative position of check mark etc.
  int dy = (h() - W) / 2;         // neg. offset o.k. for vertical centering
  int lx = 0;                     // relative label position (STR #3237)

  if (down_box())
  {
    // draw other down_box() styles:
    switch (down_box())
    {
      case FL_DOWN_BOX :
      case FL_UP_BOX :
      case _FL_PLASTIC_DOWN_BOX :
      case _FL_PLASTIC_UP_BOX :
        // Check box...
        draw_box(down_box(), x()+dx, y()+dy, W, W, FL_BACKGROUND2_COLOR);
        if (value()) {
          if (Fl::is_scheme("gtk+")) {
            fl_color(FL_SELECTION_COLOR);
          } else {
            fl_color(col);
          }
          /* The only difference to the fltk original is the checkmark line width
             calculations, line points, and the fl_begin_line/fl_end_line calls */
          int lw = (int)((float)W / 8);
          lw = lw ? lw : 1;
          int tx = x() + dx + 3 + (lw / 2);
          int tw = W - 6 - lw;
          int d1 = tw/3;
          int d2 = tw-d1;
          int ty = y() + dy + (W+d2)/2-d1-2;
          fl_line_style(FL_JOIN_ROUND | FL_CAP_ROUND, lw);
          fl_begin_line();
          for (int n = 0; n < 3; n++, ty++) {
            fl_line(tx, ty, tx+d1, ty+d1);
            fl_line(tx+d1, ty+d1, tx+tw-1, ty+d1-d2+1);
          }
          fl_end_line();
          fl_line_style(0);
        }
        break;
      case _FL_ROUND_DOWN_BOX :
      case _FL_ROUND_UP_BOX :
        // Radio button...
        draw_box(down_box(), x()+dx, y()+dy, W, W, FL_BACKGROUND2_COLOR);
        if (value()) {
          int tW = (W - Fl::box_dw(down_box())) / 2 + 1;
          if ((W - tW) & 1) tW++; // Make sure difference is even to center
          int tdx = dx + (W - tW) / 2;
          int tdy = dy + (W - tW) / 2;

          if (Fl::is_scheme("gtk+")) {
            fl_color(FL_SELECTION_COLOR);
            tW --;
            fl_pie(x() + tdx - 1, y() + tdy - 1, tW + 3, tW + 3, 0.0, 360.0);
            fl_color(fl_color_average(FL_WHITE, FL_SELECTION_COLOR, 0.2f));
          } else fl_color(col);

          switch (tW) {
            // Larger circles draw fine...
            default :
              fl_pie(x() + tdx, y() + tdy, tW, tW, 0.0, 360.0);
              break;

            // Small circles don't draw well on many systems...
            case 6 :
              fl_rectf(x() + tdx + 2, y() + tdy, tW - 4, tW);
              fl_rectf(x() + tdx + 1, y() + tdy + 1, tW - 2, tW - 2);
              fl_rectf(x() + tdx, y() + tdy + 2, tW, tW - 4);
              break;

            case 5 :
            case 4 :
            case 3 :
              fl_rectf(x() + tdx + 1, y() + tdy, tW - 2, tW);
              fl_rectf(x() + tdx, y() + tdy + 1, tW, tW - 2);
              break;

            case 2 :
            case 1 :
              fl_rectf(x() + tdx, y() + tdy, tW, tW);
              break;
          }

          if (Fl::is_scheme("gtk+")) {
            fl_color(fl_color_average(FL_WHITE, FL_SELECTION_COLOR, 0.5));
            fl_arc(x() + tdx, y() + tdy, tW + 1, tW + 1, 60.0, 180.0);
          }
        }
        break;
      default :
        draw_box(down_box(), x()+dx, y()+dy, W, W, col);
        break;
    }
    lx = dx + W + 2;
  }
  else
  {
    // if down_box() is zero, draw light button style:
    int hh = h()-2*dy - 2;
    int ww = W/2+1;
    int xx = dx;
    if (w()<ww+2*xx) xx = (w()-ww)/2;
    if (Fl::is_scheme("plastic")) {
      col = active_r() ? selection_color() : fl_inactive(selection_color());
      fl_color(value() ? col : fl_color_average(col, FL_BLACK, 0.5f));
      fl_pie(x()+xx, y()+dy+1, ww, hh, 0, 360);
    } else {
      draw_box(FL_THIN_DOWN_BOX, x()+xx, y()+dy+1, ww, hh, col);
    }
    lx = dx + ww + 2;
  }
  draw_label(x()+lx, y(), w()-lx-bx, h());
  if (Fl::focus() == this) draw_focus();
}

int Fl_Light_Button2::handle(int event)
{
  /*
   * changed from 'case' to 'if' as only one event is tested
   * and code is simpler with no drop-through warning
   */
  if (event == FL_RELEASE && box())
      redraw();
  return Fl_Button::handle(event);
}

/*
  Creates a new Fl_Light_Button2 widget using the given
  position, size, and label string.
  <P>The destructor deletes the check button.
*/
Fl_Light_Button2::Fl_Light_Button2(int X, int Y, int W, int H, const char* l)
: Fl_Button(X, Y, W, H, l)
{
  type(FL_TOGGLE_BUTTON);
  selection_color(FL_YELLOW);
  align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
}

Fl_Check_Button2::Fl_Check_Button2(int X, int Y, int W, int H, const char *L)
: Fl_Light_Button2(X, Y, W, H, L)
{
  box(FL_NO_BOX);
  down_box(FL_DOWN_BOX);
  selection_color(FL_FOREGROUND_COLOR);
}
