/*
    MusicClient.h

    Copyright 2009-2011, Alan Calvert

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

#include "MusicIO/MusicClient.h"
#include "MusicIO/JackClient.h"
#include "MusicIO/AlsaClient.h"
#include "MusicIO/JackAlsaClient.h"
#include "MusicIO/AlsaJackClient.h"

MusicClient *musicClient = NULL;

MusicClient *MusicClient::newMusicClient(void)
{
    MusicClient *musicObj = NULL;
    switch (Runtime.audioEngine)
    {
        case jack_audio:
            switch (Runtime.midiEngine)
            {
                case jack_midi:
                    if (!(musicObj = new JackClient()))
                        Runtime.Log("Failed to instantiate JackClient");
                    break;

                    case alsa_midi:
                        if (!(musicObj = new JackAlsaClient()))
                            Runtime.Log("Failed to instantiate JackAlsaClient");
                        break;

                default:
                    Runtime.Log("Ooops, no midi!");
                    break;
            }
            break;

        case alsa_audio:
            switch (Runtime.midiEngine)
            {
                case alsa_midi:
                    if (!(musicObj = new AlsaClient()))
                        Runtime.Log("Failed to instantiate AlsaClient");
                    break;

                    case jack_midi:
                        if (!(musicObj = new AlsaJackClient()))
                            Runtime.Log("Failed to instantiate AlsaJackClient");
                        break;

                default:
                    Runtime.Log("Oops, alsa audio, no midi!");
                    break;
            }
            break;

        default:
            Runtime.Log("Oops, no audio, no midi!");
            break;
    }
    return musicObj;
}
