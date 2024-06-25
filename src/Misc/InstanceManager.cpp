/*
    InstanceManager.h - manage lifecycle of Synth-Engine instances

    Copyright 2024,  Ichthyostega

    Based on existing code from main.cpp
    Copyright 2009-2011, Alan Calvert
    Copyright 2014-2021, Will Godfrey & others

    This file is part of yoshimi, which is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the License,
    or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License (version 2
    or later) for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "Misc/InstanceManager.h"
#include "Misc/SynthEngine.h"
#include "MusicIO/MusicClient.h"

//#include <string>
#include <memory>
#include <utility>
#include <unordered_map>
#include <array>

//using std::string;
using std::make_unique;
using std::unique_ptr;
using std::move;



namespace { // implementation details...

    /** Combinations to try, in given order, when booting an instance */
    auto drivers_to_probe(Config const& current)
    {
        using Scenario = std::pair<audio_driver,midi_driver>;
        return std::array{Scenario{current.audioEngine, current.midiEngine}
                         ,Scenario{jack_audio, alsa_midi}
                         ,Scenario{jack_audio, jack_midi}
                         ,Scenario{alsa_audio, alsa_midi}
                         ,Scenario{jack_audio, no_midi}
                         ,Scenario{alsa_audio, no_midi}
                         ,Scenario{no_audio, alsa_midi}
                         ,Scenario{no_audio, jack_midi}
                         ,Scenario{no_audio, no_midi}//this one always will do the work :)
                         };
    }

    string display(audio_driver audio)
    {
        switch (audio)
        {
            case   no_audio : return "no_audio";
            case jack_audio : return "jack_audio";
            case alsa_audio : return "alsa_audio";
            default:
                throw std::logic_error("Unknown audio driver ID");
    }   }

    string display(midi_driver midi)
    {
        switch (midi)
        {
            case   no_midi : return "no_midi";
            case jack_midi : return "jack_midi";
            case alsa_midi : return "alsa_midi";
            default:
                throw std::logic_error("Unknown MIDI driver ID");
        }
    }
}


/**
 * Descriptor: Synth-Engine instance
 */
class InstanceManager::Instance
{

        unique_ptr<SynthEngine> synth;
        unique_ptr<MusicClient> client;

    public: // can be moved and swapped, but not copied...
       ~Instance()                           = default;
        Instance(Instance&&)                 = default;
        Instance(Instance const&)            = delete;
        Instance& operator=(Instance&&)      = delete;
        Instance& operator=(Instance const&) = delete;

        Instance(uint id);

        void startUp();
        void shutDown();

        auto& getSynth() { return *synth; }
    private:
        Config& runtime() { return synth->getRuntime(); }
};


/** A »State Table« of all currently active Synth instances */
class InstanceManager::SynthIdx
{

        using Location = void*;
        struct LocationHash
        {
            size_t operator()(Location const& loc) const { return size_t(loc); }
        };

        using Table = std::unordered_map<const Location, Instance, LocationHash>;

        Table registry;

    public: // can be moved and swapped, but not copied...
       ~SynthIdx()                           = default;
        SynthIdx(SynthIdx&&)                 = default;
        SynthIdx(SynthIdx const&)            = delete;
        SynthIdx& operator=(SynthIdx&&)      = delete;
        SynthIdx& operator=(SynthIdx const&) = delete;

        // can be default created
        SynthIdx() = default;

        Instance& createInstance(uint instanceID)
        {
            Instance newEntry{instanceID};
            return registry.emplace(&newEntry.getSynth(), move(newEntry))
                           .first->second;
        }
};


InstanceManager::InstanceManager()
    : index{make_unique<SynthIdx>()}
    , cmdOptions{}
    { }

InstanceManager::~InstanceManager() { }


/** Create Synth-Engine and Connector for a given ID,
 *  possibly loading an existing config for that ID.
 * @remark Engines are created but not yet activated
 */
InstanceManager::Instance::Instance(uint id)
    : synth{make_unique<SynthEngine>(id)}
    , client{make_unique<MusicClient>(*synth)}
    { }


/** */
void InstanceManager::Instance::startUp()
{
    for (auto [tryAudio,tryMidi] : drivers_to_probe(runtime()))
    {
        if (client->open(tryAudio, tryMidi))
        {
            if (tryAudio == runtime().audioEngine and
                tryMidi == runtime().midiEngine)
                runtime().configChanged = true;
            runtime().audioEngine = tryAudio;
            runtime().midiEngine = tryMidi;
            runtime().runSynth = true;  // mark as active
            runtime().Log("Using "+display(tryAudio)+" for audio and "+display(tryMidi)+" for midi", _SYS_::LogError);
        }
    }
}




/** */
void InstanceManager::Instance::shutDown()
{

}

