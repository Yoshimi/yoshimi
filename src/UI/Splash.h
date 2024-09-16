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


#ifndef SPLASH_H
#define SPLASH_H

#include <memory>
#include <string>

class Fl_Window;


class SplashScreen
{
    std::unique_ptr<Fl_Window> splashWin;
    std::string startMsg{YOSHIMI_VERSION};
    uint refreshCycles{1};

    // shall not be copied or moved...
    SplashScreen(SplashScreen&&)                 = delete;
    SplashScreen(SplashScreen const&)            = delete;
    SplashScreen& operator=(SplashScreen&&)      = delete;
    SplashScreen& operator=(SplashScreen const&) = delete;
public:
    SplashScreen() = default;

    void showPopup();
    void showIndicator();

private:
    static void refreshSplash(void*);
    static void disposeSplash(void*);
};

#endif /*SPLASH_H*/
