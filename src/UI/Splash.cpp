/*
    Splash.cpp - show a splash image during start-up

    Copyright 2015-2023, Andrew Deryabin, Jesper Lloyd, Will Godfrey & others
    Copyright 2024, Ichthyostega

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "globals.h"
#include "UI/Splash.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Box.H>

using std::string;


namespace {// Splash screen layout config and data
    const Fl_Color SPLASH_BG_COLOUR = fl_rgb_color(0xd7, 0xf7, 0xff);
    const int SPLASH_WIDTH = 480;
    const int SPLASH_HEIGHT = 320;
    const int TEXT_HEIGHT = 15;
    const int TEXT_Y = 10;

    const double SPLASH_REFRESH_sec = 0.1;
    const double SPLASH_DURATION_sec = 5.0;
    const double INDICATOR_DURATION_sec = 3.0;

    const uchar SPLASH_PNG_DATA[] = {
    #include "UI/SplashPngHex"
    };

    SplashScreen& instance(void* self)
    {
        return * static_cast<SplashScreen*>(self);
    }
}


/** Trigger display of a splash screen for SPLASH_DURATION_sec */
void SplashScreen::showPopup()
{
    int winH{SPLASH_HEIGHT};
    int winW{SPLASH_WIDTH};
    splashWin = std::make_unique<Fl_Window>(winW, winH, "Yoshimi splash screen");
    int LbX{0};
    int LbY{winH - TEXT_Y - TEXT_HEIGHT};
    int LbW{winW};
    int LbH{TEXT_HEIGHT};

    // note: all the following widgets are automatically added as children of splashWin...
    auto box = new Fl_Box{0, 0, winW,winH};
    auto pix = new Fl_PNG_Image{"splash_screen_png", SPLASH_PNG_DATA, sizeof(SPLASH_PNG_DATA)};
    box->image(pix);

    startMsg = "V " + startMsg;
    auto boxLb = new Fl_Box{FL_NO_BOX, LbX, LbY, LbW, LbH, startMsg.c_str()};
    boxLb->align(FL_ALIGN_CENTER);
    boxLb->labelsize(TEXT_HEIGHT);
    boxLb->labeltype(FL_NORMAL_LABEL);
    boxLb->labelcolor(SPLASH_BG_COLOUR);
    boxLb->labelfont(FL_HELVETICA | FL_BOLD);

    splashWin->border(false);
    splashWin->position((Fl::w() - splashWin->w()) / 2,
                        (Fl::h() - splashWin->h()) / 2);
    splashWin->end(); // close child scope of splashWin
    splashWin->show();

    // schedule a repeated refresh callback...
    refreshCycles = uint(SPLASH_DURATION_sec / SPLASH_REFRESH_sec);
    Fl::add_timeout(SPLASH_REFRESH_sec, SplashScreen::refreshSplash, this);
}


/** Activate display of a tiny start-up notification for INDICATOR_DURATION_sec */
void SplashScreen::showIndicator()
{
    int winH{36};
    int winW{300};
    splashWin = std::make_unique<Fl_Window>(winW, winH, "Yoshimi start-up indicator");

    int LbX{2};
    int LbY{2};
    int LbW{winW - 4};
    int LbH{winH - 4};

    startMsg = "Yoshimi V " + startMsg + " is starting";
    auto boxLb = new Fl_Box{FL_EMBOSSED_FRAME, LbX, LbY, LbW, LbH, startMsg.c_str()};

    boxLb->align(FL_ALIGN_CENTER);
    boxLb->labelsize(16);
    boxLb->labeltype(FL_NORMAL_LABEL);
    boxLb->labelcolor(0x0000e100);
    boxLb->labelfont(FL_BOLD);

    splashWin->border(false);
    splashWin->position((Fl::w() - splashWin->w()) / 2,
                        (Fl::h() - splashWin->h()) / 2);
    splashWin->end();
    splashWin->show();
    // schedule a single callback to remove the indicator window...
    Fl::add_timeout(INDICATOR_DURATION_sec, SplashScreen::disposeSplash, this);
}


/** @internal callback to refresh the splash screen and keep it on top */
void SplashScreen::refreshSplash(void* self)
{
    if (not self) return;
    if (0 < instance(self).refreshCycles--)
    {
        instance(self).splashWin->show();   // keeps it in front
        Fl::repeat_timeout(SPLASH_REFRESH_sec, SplashScreen::refreshSplash, self);
    }
    else // time is up -- remove the splash screen window...
        Fl::add_timeout(SPLASH_REFRESH_sec, SplashScreen::disposeSplash, self);
}

/** @internal callback to terminate the splash screen display */
void SplashScreen::disposeSplash(void* self)
{
    instance(self).splashWin.reset();
}// Fl_Window dtor hides window, disables events and deallocates all child widgets
