/*
    LASHClient.cpp - LASH support

    Original ZynAddSubFX author Nasca Octavian Paul
    Copyright (C) 2002-2005 Nasca Octavian Paul

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
#include <unistd.h>
#include <iostream>
#include <string>

using namespace std;

#include "Misc/LASHClient.h"

LASHClient *lash = NULL;

LASHClient::LASHClient(int* argc, char*** argv)
{
    client = lash_init(lash_extract_args(argc, argv), "ZynAddSubFX",
                       LASH_Config_File, LASH_PROTOCOL(2, 0));
}

LASHClient::Event LASHClient::checkEvents(string& filename)
{
    if (!lash_enabled(client))
        return NoEvent;

    Event received = NoEvent;
    lash_event_t* event;
    while ((event = lash_get_event(client)))
    {
        // save
        if (lash_event_get_type(event) == LASH_Save_File)
        {
            cerr << "LASH event: LASH_Save_File" << endl;
            filename = string(lash_event_get_string(event)) + "/master.xmz";
            received = Save;
            break;
        }

        // restore
        else if (lash_event_get_type(event) == LASH_Restore_File)
        {
            cerr << "LASH event: LASH_Restore_File" << endl;
            filename = string(lash_event_get_string(event)) + "/master.xmz";
            received = Restore;
            break;
        }

        // quit
        else if (lash_event_get_type(event) == LASH_Quit)
        {
            cerr << "LASH event: LASH_Quit" << endl;
            received = Quit;
            break;
        }
        lash_event_destroy(event);
    }
    return received;
}

void LASHClient::confirmEvent(Event event)
{
    if (event == Save)
        lash_send_event(client, lash_event_new_with_type(LASH_Save_File));
    else if (event == Restore)
        lash_send_event(client, lash_event_new_with_type(LASH_Restore_File));
}




void LASHClient::setIdent(audio_drivers audio, midi_drivers midi,
                          string jackclientName, int alsaclientId)
{
        switch (midi)
        {
            case jack_midi:
            lash->setJackName(jackclientName.c_str());
            break;

            case alsa_midi:
                lash->setAlsaId(alsaclientId);
                break;

        default:
            break;
    }

    switch (audio)
    {
        case jack_audio:
            if (lash_enabled(client) && jackclientName.size())
            {
                lash_jack_client_name(client, jackclientName.c_str());
                lash_event_t *event = lash_event_new_with_type(LASH_Client_Name);
                lash_event_set_string(event, jackclientName);
                lash_send_event(client, event);
            }
            break;

        case alsa_audio:
            //lash->setAlsaId(alsaclientId);
            if (lash_enabled(client) && alsaclientId != -1)
                lash_alsa_client_id(client, alsaclientId);
            break;

        default:
            break;
    }
}

