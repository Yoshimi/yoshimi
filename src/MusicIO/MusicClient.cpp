/*
    MusicClient.h

    Copyright 2009, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>

using namespace std;

#include "MusicIO/MusicClient.h"
#include "MusicIO/JackClient.h"
#include "MusicIO/AlsaClient.h"
#include "MusicIO/JackAlsaClient.h"
#include "MusicIO/AlsaJackClient.h"

MusicClient *MusicClient::newMusicClient(void)
{
    MusicClient *musicObj = NULL;
    switch (Runtime.settings.audioEngine)
    {
        case jack_audio:
            switch (Runtime.settings.midiEngine)
            {
                case jack_midi:
                    if (NULL == (musicObj = new JackClient()))
                        cerr << "Error, failed to instantiate JackClient" << endl;
                    break;

                    case alsa_midi:
                        if (NULL == (musicObj = new JackAlsaClient()))
                            cerr << "Error, failed to instantiate JackAlsaClient" << endl;
                        break;

                default:
                    cerr << "Ooops, no midi!" << endl;
                    break;
            }
            break;

        case alsa_audio:
            switch (Runtime.settings.midiEngine)
            {
                case alsa_midi:
                    if (NULL == (musicObj = new AlsaClient()))
                        cerr << "Error, failed to instantiate AlsaClient" << endl;
                    break;

                    case jack_midi:
                        if (NULL == (musicObj = new AlsaJackClient()))
                            cerr << "Error, failed to instantiate AlsaJackClient" << endl;
                        break;

                default:
                    cerr << "Oops, alsa audio, no midi!" << endl;
                    break;
            }
            break;

        default:
            cerr << "Oops, no audio, no midi!" << endl;
            break;
    }
    return musicObj;
}
