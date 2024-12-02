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
#ifndef YOSHIMI_LV2_PLUGIN
#include "Misc/CmdOptions.h"
#include "Misc/TestInvoker.h"
#endif
#ifdef GUI_FLTK
    #include "MasterUI.h"
#endif

#include <mutex>
#include <memory>
#include <thread>
#include <utility>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <array>
#include <map>

using std::string;
using std::function;
using std::make_unique;
using std::unique_ptr;
using std::for_each;
using std::move;
using std::cout;
using std::endl;

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
 * An instance of the Synth-Engine,
 * packaged together with a MusicClient
 * and marked with lifecycle (#LifePhase) state.
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

        bool startUp(PluginCreator =PluginCreator());
        void shutDown();
        void enterRunningState();
        void startGUI_forApp();

        SynthEngine& getSynth()    { return *synth; }
        MusicClient& getClient()   { return *client; }
        InterChange& interChange() { return synth->interchange; }
        Config& runtime()          { return synth->getRuntime(); }
        LifePhase getState() const { return state; }
        uint getID()         const { return synth->getUniqueId(); }
        bool isPrimary()     const { return 0 == getID(); }

        void triggerPostBootHook();
        void registerAudioPorts();
};



/**
 * A housekeeper and caretaker responsible for clear-out of droppings.
 * - maintains a registry of all engine instances, keyed by Synth-ID
 * - the dutyCycle watches and drives instance lifecycle
 * - operates a running state duty cycle
 */
class InstanceManager::SynthGroom
{
        std::mutex mtx;

        using Table = std::map<const uint, Instance>;

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

        Instance& createInstance(uint instanceID =0);

        void dutyCycle(function<void(SynthEngine&)>& handleEvents);
        void shutdownRunningInstances();
        void persistRunningInstances();
        void discardInstance(uint);
        void startGUI_forLV2(uint, string);
    private:
        void clearZombies();
        void handleStartRequest();
        uint allocateID(uint);
};


InstanceManager::InstanceManager()
    : groom{make_unique<SynthGroom>()}
    { }

InstanceManager::~InstanceManager() { }


/** Create Synth-Engine and back-end connector for a given ID,
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
    auto entry = registry.find(id);
        if (entry != registry.end())
            return entry->second;
    assert(primary);
    return *primary;
}


InstanceManager::Instance& InstanceManager::SynthGroom::createInstance(uint instanceID)
{
    Guard lock(mtx);
    instanceID = allocateID(instanceID);
    Instance newEntry{instanceID};
    auto& instance = registry.emplace(instanceID, move(newEntry))
                             .first->second;
    if (!primary)
        primary = & instance;
    return instance;
}



/** boot up this engine instance into working state.
 * - probe a working IO / client setup
 * - init the SynthEngine
 * - start the IO backend
 * @param pluginCreator (optional) a functor to attach to an external host (notably LV2).
 *       If _not_ given (which is the default for standalone Yoshimi), then several
 *       combinations of ALSA and Jack are probed to find a working backend.
 * @return `true` on success
 * @note after a successful boot, `state == BOOTING`,
 *       which enables some post-boot-hooks to run,
 *       and notably prompts the GUI to become visible;
 *       after that, the state will transition to `RUNNING`.
 *       However, if boot-up fails, `state == EXPIRING` and
 *       further transitioning to `DEFUNCT` after shutdown.
 */
bool InstanceManager::Instance::startUp(PluginCreator pluginCreator)
{
    cout << "\nStart-up Synth-Instance("<< getID() << ")..."<< endl;
    state = BOOTING;
    runtime().loadConfig();
    bool isLV2 = bool(pluginCreator);
    assert (not runtime().runSynth);
    if (isLV2)
    {
        runtime().Log("\n----Start-LV2-Plugin--ID("+asString(getID())+")----");
        runtime().init();
        if (client->open(pluginCreator))
            runtime().runSynth = true;
    }
    else
    {
        auto configuredAudio = runtime().audioEngine;
        auto configuredMidi  = runtime().midiEngine;

        for (auto [tryAudio,tryMidi] : drivers_to_probe(runtime()))
        {
            runtime().Log("\n-----Connect-attempt----("+display(tryAudio)+"/"+display(tryMidi)+")----");
            runtime().audioEngine = tryAudio;
            runtime().midiEngine = tryMidi;
            runtime().init();
            if (client->open(tryAudio, tryMidi))
            {
                if (tryAudio == configuredAudio and
                    tryMidi == configuredMidi)
                    runtime().configChanged = true;
                runtime().runSynth = true;  // mark as active and enable background threads
                runtime().Log("-----Connect-SUCCESS-------------------\n");
                runtime().Log("Using "+display(tryAudio)+" for audio and "+display(tryMidi)+" for midi", _SYS_::LogError);
                break;
            }
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
                    cout << "\nYay! We're up and running :-)\n";
                else
                    cout << "\nStarted Synth-Instance("<< getID() << ")\n";

                state = BOOTING;
                if (isLV2) enterRunningState();
                assert (runtime().runSynth);
                return true;
    }   }   }

    auto failureMsg = isLV2? string{"Failed to start Yoshimi as LV2 plugin"}
                           : string{"Bail: Yoshimi stages a strategic retreat :-("};
    runtime().Log(failureMsg, _SYS_::LogError);
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
    cout << "Stopping Synth-Instance("<< getID() << ")..."<< endl;
    runtime().runSynth.store(false, std::memory_order_release); // signal to synth and background threads
    synth->saveBanks();
    client->close();  // may block until background threads terminate
    runtime().flushLog();
    state = DEFUNCT;
}


/** install and start-up the primary SynthEngine and runtime */
bool InstanceManager::bootPrimary(int argc, char *argv[])
{
#ifndef YOSHIMI_LV2_PLUGIN
    assert (0 == groom->instanceCnt());
    CmdOptions baseSettings(argc,argv);
    Instance& primary = groom->createInstance(0);
    baseSettings.applyTo(primary.runtime());
    return primary.startUp();
#else
    (void)argc; (void)argv;
    throw std::logic_error("Must not boot a standalone primary Synth for LV2");
#endif                  //(actual reason is: we do not link in CmdOptions.cpp)
}

/** create and manage a SynthEngine instance attached to a (LV2) plugin */
bool InstanceManager::startPluginInstance(PluginCreator buildPluginInstance)
{
    return groom->instanceCnt() < MAX_INSTANCES
       and groom->createInstance(0)  // choose next free ID
                   .startUp(buildPluginInstance);
}

void InstanceManager::terminatePluginInstance(uint synthID)
{
    groom->discardInstance(synthID);
}
void InstanceManager::SynthGroom::discardInstance(uint synthID)
{
    auto& instance{find(synthID)};
    if (instance.getID() == synthID)
    {
        instance.shutDown();
        {
            Guard lock(mtx);
            clearZombies();
}   }   }


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
                    instance.startGUI_forApp();
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
            return;
        }// only one per duty cycle
}

void InstanceManager::SynthGroom::clearZombies()
{
    for (auto elm = registry.begin(); elm != registry.end();)
    {
        Instance& instance{elm->second};
        if (instance.getState() == DEFUNCT
                and not instance.isPrimary())
            elm = registry.erase(elm);
        else
            ++elm;
    }
}

/**
 * Launch the GUI at any time on-demand while Synth is already running.
 * @note LV2 possibly re-creates the GUI-Plugin after it has been closed;
 *       for that reason, everything in this function must be idempotent.
 */
void InstanceManager::launchGui_forPlugin(uint synthID, string windowTitle)
{
    groom->startGUI_forLV2(synthID, windowTitle);
}

void InstanceManager::SynthGroom::startGUI_forLV2(uint synthID, string windowTitle)
{
#ifdef GUI_FLTK
    // ensure data visibility since LV2 GUI-plugin can run in any thread and in any order
    Guard lock(mtx);
    auto& instance{find(synthID)};
    assert (instance.getID() == synthID);

    instance.runtime().showGui = true;
    instance.triggerPostBootHook();  // trigger push-updates for UI state
    instance.getSynth().setWindowTitle(windowTitle);
    instance.interChange().createGuiMaster();
#endif
}

void InstanceManager::Instance::startGUI_forApp()
{
#ifdef GUI_FLTK
    interChange().createGuiMaster();

    if (runtime().audioEngine < 1)
        alert(synth.get(), "Yoshimi could not connect to any sound system. Running with no Audio.");
    if (runtime().midiEngine < 1)
        alert(synth.get(), "Yoshimi could not connect to any MIDI system. Running with no MIDI.");
#endif
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


#ifndef YOSHIMI_LV2_PLUGIN
bool InstanceManager::requestedSoundTest()
{
    return test::TestInvoker::access().activated;
}

void InstanceManager::launchSoundTest()
{
    auto& soundTest{test::TestInvoker::access()};
    auto& primarySynth{groom->getPrimary().getSynth()};
    assert(soundTest.activated);
    soundTest.performSoundCalculation(primarySynth);
}
#endif


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
    if (desiredID >= 32 or (desiredID > 0 and contains(registry, desiredID)))
        desiredID = 0; // use the next free ID instead

    if (not desiredID)
    {
        for (auto& [id,_] : registry)
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
    triggerMsg.data.parameter = (state != RUNNING? 1 : 0);  // initial boot-up init or later refresh for GUI
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
