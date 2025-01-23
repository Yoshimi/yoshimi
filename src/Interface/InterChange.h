/*
    InterChange.h - General communications

    Copyright 2016-2020 Will Godfrey
    Copyright 2021 Will Godfrey, Rainer Hans Liffers
    Copyright 2022-2025, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
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

#ifndef INTERCH_H
#define INTERCH_H

#include "globals.h"
#include "Interface/Data2Text.h"
#include "Interface/RingBuffer.h"
#include "Interface/GuiDataExchange.h"
#include "Params/LFOParams.h"
#include "Params/FilterParams.h"
#include "Params/EnvelopeParams.h"
#include "Params/OscilParameters.h"
#include "Synth/Resonance.h"

#include <semaphore.h>

#include <list>
#include <memory>
#include <string>

class DataText;
class MasterUI;
class SynthEngine;
class PADnoteParameters;

// used by main.cpp and SynthEngine.cpp
extern std::string singlePath;
extern int startInstance;


/* compile time function log2 as per
   https://hbfs.wordpress.com/2016/03/22/log2-with-c-metaprogramming          */
static constexpr size_t log2
                        (const size_t n)
 {
  return ((n < 2) ? 0 : 1 + log2 (n / 2));
 }


class InterChange : private DataText
{

    private:
        static constexpr size_t commandBlockSize = sizeof (CommandBlock);
        SynthEngine& synth;

#ifdef GUI_FLTK
        std::unique_ptr<MasterUI> guiMaster;

        ///////////////////TODO 1/2024 : retract usage of direct SynthEngine* from UI
        friend class SynthEngine;
#endif

    public:
        InterChange(SynthEngine&);
       ~InterChange();
        // shall not be copied or moved or assigned
        InterChange(InterChange&&)                 = delete;
        InterChange(InterChange const&)            = delete;
        InterChange& operator=(InterChange&&)      = delete;
        InterChange& operator=(InterChange const&) = delete;

        bool Init();

#ifdef GUI_FLTK
        void createGuiMaster();
        void shutdownGui();
#endif

        CommandBlock commandData;
#ifndef YOSHIMI_LV2_PLUGIN
        RingBuffer <9, log2 (commandBlockSize)> fromCLI;
#endif
        RingBuffer <10, log2 (commandBlockSize)> decodeLoopback;
#ifdef GUI_FLTK
        RingBuffer <10, log2 (commandBlockSize)> fromGUI;
        RingBuffer <11, log2 (commandBlockSize)> toGUI;
#endif
        RingBuffer <10, log2 (commandBlockSize)> fromMIDI;
        RingBuffer <10, log2 (commandBlockSize)> returnsBuffer;
        RingBuffer <4, log2 (commandBlockSize)> muteQueue;

        GuiDataExchange guiDataExchange;

        sem_t sortResultsThreadSemaphore;
        void spinSortResultsThread();

        void generateSpecialInstrument(int npart, std::string name);
        void mediate();
        void historyActionCheck(CommandBlock&);
        void returns(CommandBlock&);
        void doClearPartInstrument(int npart);
        bool commandSend(CommandBlock&);
        float readAllData(CommandBlock&);
        float buildWindowTitle(CommandBlock&);
        void resolveReplies(CommandBlock&);
        std::string resolveText(CommandBlock&, bool addValue);
        void testLimits(CommandBlock&);
        float returnLimits(CommandBlock&);
        void Log(std::string const& msg);

        std::atomic<bool> syncWrite;
        std::atomic<bool> lowPrioWrite;

    private:
        void* sortResultsThread();
        static void* _sortResultsThread(void* arg);
        pthread_t  sortResultsThreadHandle;
        void muteQueueWrite(CommandBlock&);
        void indirectTransfers(CommandBlock&, bool noForward = false);
        int indirectVector(CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        int indirectMidi  (CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        int indirectScales(CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        int indirectMain  (CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text, float& valuef);
        int indirectBank  (CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        int indirectConfig(CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        int indirectPart  (CommandBlock&, uchar& newMsg, bool& guiTo, std::string& text);
        std::string formatScales(std::string text);
        std::string formatKeys(std::string text);

        unsigned int swapRoot1;
        unsigned int swapBank1;
        unsigned int swapInstrument1;
        bool processAdd(CommandBlock&, SynthEngine&);
        bool processVoice(CommandBlock&, SynthEngine&);
        bool processSub(CommandBlock&, SynthEngine&);
        bool processPad(CommandBlock&);

        void commandMidi(CommandBlock&);
        void vectorClear(int Nvector);
        void commandVector(CommandBlock&);
        void commandMicrotonal(CommandBlock&);
        void commandConfig(CommandBlock&);
        void commandMain(CommandBlock&);
        void commandBank(CommandBlock&);
        void commandPart(CommandBlock&);
        void commandControllers(CommandBlock&, bool write);
        void commandAdd(CommandBlock&);
        void commandAddVoice(CommandBlock&);
        void commandSub(CommandBlock&);
        bool commandPad(CommandBlock&, PADnoteParameters& pars);
        void commandOscillator(CommandBlock&, OscilParameters *oscil);
        void commandResonance(CommandBlock&, Resonance *respar);
        void commandLFO(CommandBlock&);
        void lfoReadWrite(CommandBlock&, LFOParams *pars);
        void commandFilter(CommandBlock&);
        void filterReadWrite(CommandBlock&, FilterParams*, uchar* velsnsamp, uchar* velsns);
        void commandEnvelope(CommandBlock&);
        void envelopeReadWrite(CommandBlock&, EnvelopeParams*);
        void envelopePointAdd(CommandBlock&, EnvelopeParams*);
        void envelopePointDelete(CommandBlock&, EnvelopeParams*);
        void envelopePointChange(CommandBlock&, EnvelopeParams*);

        void commandSysIns(CommandBlock&);

        void add2undo(CommandBlock&, bool& noteSeen, bool group = false);
        void addFixed2undo(CommandBlock&);
        void undoLast(CommandBlock& candidate);
        std::list<CommandBlock> undoList;
        std::list<CommandBlock> redoList;
        CommandBlock lastEntry;
        CommandBlock undoMarker;
        bool undoLoopBack;
        void manageDisplay(CommandBlock& cmd);
        bool setUndo;
        bool setRedo;
        bool undoStart;
        int cameFrom; // 0 = new command, 1 = undo, 2 = redo

    public:
        bool noteSeen;
        void undoRedoClear();
        /*
         * this is made public specifically so that it can be
         * reached from SynthEngine by jack freewheeling NRPNs.
         * This avoids unnecessary (error prone) duplication.
         */
        void commandEffects(CommandBlock&);

    private:
        bool commandSendReal(CommandBlock&);

        int searchInst;
        int searchBank;
        int searchRoot;
};

#endif
