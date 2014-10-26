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

MusicClient *MusicClient::newMusicClient(SynthEngine *_synth)
{
    MusicClient *musicObj = NULL;
    switch (_synth->getRuntime().audioEngine)
    {
        case jack_audio:
            switch (_synth->getRuntime().midiEngine)
            {
                case jack_midi:
                    if (!(musicObj = new JackClient(_synth)))
                        _synth->getRuntime().Log("Failed to instantiate JackClient");
                    break;

                    case alsa_midi:
                        if (!(musicObj = new JackAlsaClient(_synth)))
                            _synth->getRuntime().Log("Failed to instantiate JackAlsaClient");
                        break;

                default:
                    _synth->getRuntime().Log("Ooops, no midi!");
                    break;
            }
            break;

        case alsa_audio:
            switch (_synth->getRuntime().midiEngine)
            {
                case alsa_midi:
                    if (!(musicObj = new AlsaClient(_synth)))
                        _synth->getRuntime().Log("Failed to instantiate AlsaClient");
                    break;

                    case jack_midi:
                        if (!(musicObj = new AlsaJackClient(_synth)))
                            _synth->getRuntime().Log("Failed to instantiate AlsaJackClient");
                        break;

                default:
                    _synth->getRuntime().Log("Oops, alsa audio, no midi!");
                    break;
            }
            break;

        default:
            _synth->getRuntime().Log("Oops, no audio, no midi!");
            break;
    }
    return musicObj;
}
