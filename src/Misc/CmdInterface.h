/*
  ZynAddSubFX - a software synthesizer

  WavFile.h - Records sound to a file
  Copyright (C) 2008 Nasca Octavian Paul
  Author: Nasca Octavian Paul
          Mark McCurry

  This program is free software; you can redistribute it and/or modify
  it under the terms of either version 2 of the License, or (at your option)
  any later version, as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License (version 2 or later) for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef CMDINTERFACE_H
#define CMDINTERFACE_H
#include <string>
#include <Misc/MiscFuncs.h>
#include <Misc/SynthEngine.h>
#include <Effects/EffectMgr.h>

extern map<SynthEngine *, MusicClient *> synthInstances;

// all_fx and ins_fx MUST be the first two
typedef enum { all_fx = 0, ins_fx, vect_lev, part_lev, } level_bits;

typedef enum { todo_msg = 0, done_msg, value_msg, opp_msg, what_msg, range_msg, low_msg, high_msg, unrecognised_msg, parameter_msg, level_msg, available_msg,} error_messages;

class CmdInterface : private MiscFuncs
{
    public:
        void defaults();
        void cmdIfaceCommandLoop();
        
    private:
        bool query(string text, bool priority);
        void helpLoop(list<string>& msg, string *commands, int indent);
        bool helpList();
        int effectsList();
        int effects(int level);
        int volPanShift();
        int commandVector();
        int commandPart(bool justSet);
        int commandSet();
        bool cmdIfaceProcessCommand();
        char *cCmd;
        char *point;
        SynthEngine *synth;  
        char welcomeBuffer [128];
        
        int npart;
        int nFX;
        int nFXtype;
        int chan;
        int axis;
        unsigned int level;
        string replyString;
};
#endif
