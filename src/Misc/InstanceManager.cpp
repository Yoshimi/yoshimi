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
#include "Misc/FormatFuncs.h"
#include "Misc/Util.h"
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include <mutex>
#include <memory>
#include <thread>
#include <utility>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <array>
#include <set>

using std::string;
using std::function;
using std::make_unique;
using std::unique_ptr;
using std::for_each;
using std::move;
using std::set;

using func::asString;
using util::contains;
using util::isLimited;

using Guard = const std::lock_guard<std::mutex>;



namespace { // implementation details...

    // Maximum number of SynthEngine instances allowed.
    // Historically, this limit was imposed due to using a 32bit field;
    // theoretically this number is unlimited, yet in practice, the system's
    // available resources will likely impose an even stricter limit...
    const uint MAX_INSTANCES = 32;


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


    /**
     * Instance Lifecycle
     */
    enum LifePhase {
        PENDING = 0,
        BOOTING,
        RUNNING,
        WANING,
        DEFUNCT
    };
}


/**
 * Descriptor: Synth-Engine instance
 */
class InstanceManager::Instance
{

        unique_ptr<SynthEngine> synth;
        unique_ptr<MusicClient> client;

        LifePhase state{PENDING};

    public: // can be moved and swapped, but not copied...
        Instance(Instance&&)                 = default;
        Instance(Instance const&)            = delete;
        Instance& operator=(Instance&&)      = delete;
        Instance& operator=(Instance const&) = delete;

        Instance(uint id);
       ~Instance();

        bool startUp();
        void shutDown();
        void buildGuiMaster();
        void enterRunningState();

        SynthEngine& getSynth()    { return *synth; }
        MusicClient& getClient()   { return *client; }
        Config& runtime()          { return synth->getRuntime(); }
        LifePhase getState() const { return state; }
        uint getID()         const { return synth->getUniqueId(); }
        bool isPrimary()     const { return 0 == getID(); }
    private:
        void triggerPostBootHook();
        void registerAudioPorts();
};



/**
 * A housekeeper and caretaker responsible for clear-out of droppings.
 * - maintains a registry of all engine instances
 * - serves to further the lifecycle
 * - operates a running state duty cycle
 */
class InstanceManager::SynthGroom
{
        std::mutex mtx;

        using Location = void*;
        struct LocationHash
        {
            size_t operator()(Location const& loc) const { return size_t(loc); }
        };

        using Table = std::unordered_map<const Location, Instance, LocationHash>;

        Table registry;
        Instance* primary{nullptr};

    public: // can be moved and swapped, but not copied...
       ~SynthGroom()                             = default;
        SynthGroom(SynthGroom &&)                = default;
        SynthGroom(SynthGroom const&)            = delete;
        SynthGroom& operator=(SynthGroom &&)     = delete;
        SynthGroom& operator=(SynthGroom const&) = delete;

        // can be default created
        SynthGroom() = default;

        Instance& createInstance(uint instanceID =0)
        {
            Guard lock(mtx);
            instanceID = allocateID(instanceID);
            Instance newEntry{instanceID};
            auto& instance = registry.emplace(&newEntry.getSynth(), move(newEntry))
                                     .first->second;
            if (!primary)
                primary = & instance;
            return instance;
        }

        Instance& getPrimary()
        {
            assert(primary);
            return *primary;
        }

        uint instanceCnt()  const
        {
            return registry.size();
        }
        Instance& find(uint);

        void dutyCycle(function<void(SynthEngine&)>& handleEvents);
        void shutdownRunningInstances();
        void persistRunningInstances();
    private:
        void clearZombies();
        void handleStartRequest();
        uint allocateID(uint);
};


InstanceManager::InstanceManager()
    : groom{make_unique<SynthGroom>()}
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


/** @note unwinding of instances happens automatically by destructor.
 *        Yet shutDown() can be invoked explicitly for secondary instances.
 */
InstanceManager::Instance::~Instance()
{
    if (synth and state == RUNNING)
        try { shutDown();       }
        catch(...) {/* ignore */}
}


Config& InstanceManager::accessPrimaryConfig()
{
    return groom->getPrimary().runtime();
}

SynthEngine& InstanceManager::findSynthByID(uint id)
{
    return groom->find(id).getSynth();
}

InstanceManager::Instance& InstanceManager::SynthGroom::find(uint id)
{
    for (auto& [_,instance] : registry)
        if (instance.getID() == id)
            return instance;
    assert(primary);
    return *primary;
}



/** boot up this engine instance into working state.
 * - probe a working IO / client setup
 * - init the SynthEngine
 * - start the IO backend
 * @return `true` on success
 * @note after a successful boot, `state == BOOTING`,
 *       which enables some post-boot-hooks to run,
 *       and notably prompts the GUI to become visible;
 *       after that, the state will transition to `RUNNING`.
 *       However, if boot-up fails, `state == EXPIRING` and
 *       further transitioning to `DEFUNCT` after shutdown.
 */
bool InstanceManager::Instance::startUp()
{
    std::cout << "\nStart-up Synth-Instance("<< getID() << ")..."<< std::endl;
    state = BOOTING;
    runtime().setup();
    assert (not runtime().runSynth);
    for (auto [tryAudio,tryMidi] : drivers_to_probe(runtime()))
    {
        runtime().Log("\n-----Connect-attempt----("+display(tryAudio)+"/"+display(tryMidi)+")----");
        if (client->open(tryAudio, tryMidi))
        {
            if (tryAudio == runtime().audioEngine and
                tryMidi == runtime().midiEngine)
                runtime().configChanged = true;
            runtime().audioEngine = tryAudio;
            runtime().midiEngine = tryMidi;
            runtime().runSynth = true;  // mark as active and enable background threads
            runtime().Log("-----Connect-SUCCESS-------------------\n");
            runtime().Log("Using "+display(tryAudio)+" for audio and "+display(tryMidi)+" for midi", _SYS_::LogError);
            break;
        }
    }
    if (not runtime().runSynth)
        runtime().Log("Failed to instantiate MusicClient",_SYS_::LogError);
    else
    {
        if (not synth->Init(client->getSamplerate(), client->getBuffersize()))
            runtime().Log("SynthEngine init failed",_SYS_::LogError);
        else
        {
            if (isPrimary())
                synth->loadHistory();
            // discover persistent bank file structure
            synth->installBanks();
            //
            // Note: the following launches or connects to the processing threads
            if (not client->start())
                runtime().Log("Failed to start MusicIO",_SYS_::LogError);
            else
            {// engine started successfully....
#ifdef GUI_FLTK
                if (runtime().showGui)
                    synth->setWindowTitle(client->midiClientName());
                else
                    runtime().toConsole = false;
#else
                runtime().toConsole = false;
#endif
                runtime().startupReport(client->midiClientName());

                if (isPrimary())
                    std::cout << "\nYay! We're up and running :-)\n";
                else
                    std::cout << "\nStarted "<< synth->getUniqueId() << "\n";

                state = BOOTING;
                assert (runtime().runSynth);
                return true;
    }   }   }

    runtime().Log("Bail: Yoshimi stages a strategic retreat :-(",_SYS_::LogError);
    shutDown();
    return false;
}



/**
 * ensure the instance ends active operation...
 * - signal all background threads to stop
 * - possibly disconnect from audio/MIDI (blocking!)
 * - mark instance for clean-up
 */
void InstanceManager::Instance::shutDown()
{
    state = WANING;
    std::cout << "Stopping Synth-Instance("<< getID() << ")..."<< std::endl;
    runtime().runSynth.store(false, std::memory_order_release); // signal to synth and background threads
    synth->saveBanks();
    client->close();  // may block until background threads terminate
    runtime().flushLog();
    state = DEFUNCT;
}


/** install and start-up the primary SynthEngine and runtime */
bool InstanceManager::bootPrimary(int argc, char *argv[])
{
    assert (0 == groom->instanceCnt());
    CmdOptions baseSettings(argc,argv);
    Instance& primary = groom->createInstance(0);
    baseSettings.applyTo(primary.runtime());
    return primary.startUp();
}

/**
 * Request to allocate a new SynthEngine instance.
 * @return ID of the new instance or zero, if no further instance can be created
 * @remark the new instance will start up asynchronously, see SynthGroom::dutyCycle()
 * @warning this function can block for an extended time (>33ms),
 *          since it contends with the event handling duty cycle.
 */
uint InstanceManager::requestNewInstance(uint desiredID)
{
    if (groom->instanceCnt() < MAX_INSTANCES)
        return groom->createInstance(desiredID).getID();

    groom->getPrimary().runtime().LogError("Maximum number("+asString(MAX_INSTANCES)
                                          +") of Synth-Engine instances exceeded.");
    return 0;
}

/**
 * Initiate restoring of specific instances, as persisted in the base config.
 * This function must be called after the »primary« SynthEngine was started, but prior
 * to launching any further instances; the new allotted engines will start asynchronously
 */
void InstanceManager::triggerRestoreInstances()
{
    assert (1 == groom->instanceCnt());
    Config& cfg{accessPrimaryConfig()};
    if (cfg.autoInstance)
        for (uint id=1; id<MAX_INSTANCES; ++id)
            if (cfg.activeInstances.test(id))
                groom->createInstance(id);
}

/**
 * Handle an OS-signal to start a new instance.
 * @remark to avoid any blocking, we send this indirect through the command system;
 *         it will handled in the background thread and from there invoke requestNewInstance()
 */
void InstanceManager::handleNewInstanceSignal()
{
    assert (1 <= groom->instanceCnt());

    CommandBlock triggerMsg;
    triggerMsg.data.control = MAIN::control::startInstance;
    triggerMsg.data.source  = TOPLEVEL::action::lowPrio;
    triggerMsg.data.part    = TOPLEVEL::section::main;
    triggerMsg.data.type    = TOPLEVEL::type::Integer;
    triggerMsg.data.value   = 0;     // request next free Synth-ID
    //
    triggerMsg.data.offset    = UNUSED;
    triggerMsg.data.kit       = UNUSED;
    triggerMsg.data.engine    = UNUSED;
    triggerMsg.data.insert    = UNUSED;
    triggerMsg.data.parameter = UNUSED;
    triggerMsg.data.miscmsg   = UNUSED;
    triggerMsg.data.spare0    = UNUSED;
    triggerMsg.data.spare1    = UNUSED;

    // MIDI ringbuffer is the only one always active
    groom->getPrimary().getSynth().interchange.fromMIDI.write(triggerMsg.bytes);
}


void InstanceManager::performWhileActive(function<void(SynthEngine&)> handleEvents)
{
    while (groom->getPrimary().runtime().runSynth.load(std::memory_order_acquire))
    {
        groom->getPrimary().runtime().signalCheck();
        groom->dutyCycle(handleEvents);
        std::this_thread::yield();
    }     // tiny break allowing other threads to acquire the mutex
}

void InstanceManager::SynthGroom::dutyCycle(function<void(SynthEngine&)>& handleEvents)
{
    Guard lock(mtx); // warning: concurrent modifications could corrupt instance lifecycle

    for (auto& [_,instance] : registry)
    {
        switch (instance.getState())
        {
            case BOOTING:
                 // successfully booted, make ready for use
                if (primary->runtime().showGui)
                    instance.buildGuiMaster();
                instance.enterRunningState();
            break;
            case RUNNING:
                if (instance.runtime().runSynth.load(std::memory_order_acquire))
                     // perform GUI and command returns for this instance
                    handleEvents(instance.getSynth());
                else
                    instance.shutDown();
            break;
            default:
                /* do nothing */
            break;
        }
    }
    clearZombies();
    handleStartRequest();
}


/**
 * respond to the request to start a new engine instance, if any.
 * @note deliberately handling only a single request, as start-up is
 *       time consuming and risks tailback in other instances' GUI queues.
 */
void InstanceManager::SynthGroom::handleStartRequest()
{
    for (auto& [_,instance] : registry)
        if (PENDING == instance.getState())
        {
            bool success = instance.startUp();
            if (not success)
                primary->runtime().Log("FAILED to launch Synth-Instance("
                                      +asString(instance.getID())+")", _SYS_::LogError);
            return;  // only one per duty cycle
        }
}

void InstanceManager::SynthGroom::clearZombies()
{
    for (auto it = registry.begin(); it != registry.end();)
    {
        Instance& instance{it->second};
        if (instance.getState() == DEFUNCT
                and not instance.isPrimary())
            it = registry.erase(it);
        else
            ++it;
    }
}


/** invoked when leaving main-event-thread because primary synth stopped */
void InstanceManager::performShutdownActions()
{
    groom->persistRunningInstances();
    groom->getPrimary().getSynth().saveHistory();
}

/** detect all instances currently running and store this information persistently */
void InstanceManager::SynthGroom::persistRunningInstances()
{
    auto& cfg = getPrimary().runtime();
    cfg.activeInstances.reset();
    cfg.activeInstances.set(0); // always mark the primary
    for (auto& [id,instance] : registry)
        if (instance.getState() == RUNNING
                and instance.runtime().runSynth.load(std::memory_order_acquire))
            cfg.activeInstances.set(id);
    // persist the running instances
    cfg.saveMasterConfig();
}

/** terminate and disconnect all IO on all instances */
void InstanceManager::disconnectAll()
{
    groom->shutdownRunningInstances();
}
void InstanceManager::SynthGroom::shutdownRunningInstances()
{
    for (auto& [_,instance] : registry)
        if (instance.getState() == RUNNING)
            instance.shutDown();
}


/**
 * Allocate an unique Synth-ID not yet in use.
 * @param desiredID explicitly given desired ID;
 *                  set to zero to request allocation of next free ID
 * @return new ID which is not currently in use.
 * @note   assuming that only a limited number of Synth instances is requested
 * @remark when called for the first time, ID = 0 will be returned, which
 *         also marks the associated instance as »primary instance«, responsible
 *         for coordinates some application global aspects.
 */
uint InstanceManager::SynthGroom::allocateID(uint desiredID)
{
    set<uint> allIDs;
    for_each(registry.begin(),registry.end()
            ,[&](auto& entry){ allIDs.insert(entry.second.getID()); });

    if (desiredID >= 32 or (desiredID > 0 and contains(allIDs, desiredID)))
        desiredID = 0; // use the next free ID instead

    if (not desiredID)
    {
        for (uint id : allIDs)
            if (desiredID < id)
                break;
            else
                ++desiredID;
    }

    assert(desiredID < MAX_INSTANCES);
    assert((!primary and 0==desiredID)
          or(primary and 0 < desiredID));

    return desiredID;
}


void InstanceManager::Instance::buildGuiMaster()
{
#ifdef GUI_FLTK
    MasterUI& guiMaster = synth->interchange.createGuiMaster();
    guiMaster.Init();

    if (runtime().audioEngine < 1)
        alert(synth.get(), "Yoshimi could not connect to any sound system. Running with no Audio.");
    if (runtime().midiEngine < 1)
        alert(synth.get(), "Yoshimi could not connect to any MIDI system. Running with no MIDI.");
#endif
}

void InstanceManager::Instance::enterRunningState()
{
    triggerPostBootHook();
    registerAudioPorts();

    // this instance is now in fully operational state...
    state = RUNNING;
}

/** send command to invoke the SynthEngine::postBootHook() in the Synth-thread */
void InstanceManager::Instance::triggerPostBootHook()
{
    CommandBlock triggerMsg;

    triggerMsg.data.type    = TOPLEVEL::type::Integer | TOPLEVEL::type::Write;
    triggerMsg.data.control = TOPLEVEL::control::dataExchange;
    triggerMsg.data.part    = TOPLEVEL::section::main;
    triggerMsg.data.source  = TOPLEVEL::action::noAction;
    //                               // Important: not(action::lowPrio) since we want direct execution in Synth-thread
    triggerMsg.data.offset    = UNUSED;
    triggerMsg.data.kit       = UNUSED;
    triggerMsg.data.engine    = UNUSED;
    triggerMsg.data.insert    = UNUSED;
    triggerMsg.data.parameter = UNUSED;
    triggerMsg.data.miscmsg   = UNUSED;
    triggerMsg.data.spare0    = UNUSED;
    triggerMsg.data.spare1    = UNUSED;
    triggerMsg.data.value     = 0;

    // MIDI ringbuffer is the only one always active
    synth->interchange.fromMIDI.write(triggerMsg.bytes);
}

void InstanceManager::Instance::registerAudioPorts()
{
    for (uint portNum=0; portNum < NUM_MIDI_PARTS; ++portNum)
        if (synth->partonoffRead(portNum))
            client->registerAudioPort(portNum);
}

void InstanceManager::registerAudioPort(uint synthID, uint portNum)
{
    auto& instance = groom->find(synthID);
    if (isLimited(0u, portNum, uint(NUM_MIDI_PARTS-1)))
        instance.getClient().registerAudioPort(portNum);
}
