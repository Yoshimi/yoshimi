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
#include <unordered_map>

//using std::string;
using std::make_unique;
using std::unique_ptr;

//namespace fs = std::filesystem;


namespace { // implementation details...

    // using the allocated memory location as key

}


/**
 * Descriptor: Synth-Engine instance 
 */
class InstanceManager::Instance {
        
        unique_ptr<SynthEngine> synth;
        unique_ptr<MusicClient> client;
        
    public: // can be moved and swapped, but not copied...
       ~Instance()                           = default;
        Instance(Instance&&)                 = default;
        Instance(Instance const&)            = delete;
        Instance& operator=(Instance&&)      = delete;
        Instance& operator=(Instance const&) = delete;
        
        Instance(); //////////////////////////////////OOO need a new combined parameter set
        
        void startUp();
        void shutDown();
};


/** A »State Table« of all currently active Synth instances */
class InstanceManager::SynthIdx {
        
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
};


InstanceManager::InstanceManager()
    : index{make_unique<SynthIdx>()}
    { }
    
InstanceManager::~InstanceManager() { }



/** */
InstanceManager::Instance::Instance()/////////////////OOO define and use parameter set
    : synth{}                     ////////////////////OOO create Synth object
    , client{}                    ////////////////////OOO create MusicClient object
    { }


/** */
void InstanceManager::Instance::startUp()
{
    
}


/** */
void InstanceManager::Instance::shutDown()
{
    
}

