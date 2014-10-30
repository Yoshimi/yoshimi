#include "lv2/lv2plug.in/ns/ext/data-access/data-access.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/buf-size/buf-size.h"
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/options/options.h"
#include "lv2/lv2plug.in/ns/ext/parameters/parameters.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/port-groups/port-groups.h"
#include "lv2/lv2plug.in/ns/ext/presets/presets.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/time/time.h"
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/ext/worker/worker.h"
#include "lv2/lv2plug.in/ns/ext/port-props/port-props.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include <string>
#include <semaphore.h>

#include "Misc/SynthEngine.h"
#include "MusicIO/MusicIO.h"


class YoshimiLV2Plugin : public MusicIO
{
private:
   SynthEngine *_synth;
   uint32_t _sampleRate;
   uint32_t _bufferSize;
   std::string _bundlePath;   
   LV2_URID_Map _uridMap;
   LV2_Atom_Sequence *_midiDataPort;
   LV2_URID _midi_event_id;
   uint32_t _bufferPos;
   uint32_t _offsetPos;
   sem_t _midiSem;

   float *lv2Left [NUM_MIDI_PARTS + 1];
   float *lv2Right [NUM_MIDI_PARTS + 1];

   void process(uint32_t sample_count);
public:
   YoshimiLV2Plugin(SynthEngine *synth, double sampleRate, const char *bundlePath, const LV2_Feature *const *features);
   virtual ~YoshimiLV2Plugin();
   bool init();

   //virtual methods from MusicIO
   unsigned int getSamplerate(void) {return _sampleRate; }
   int getBuffersize(void) {return _bufferSize; }
   bool Start(void) {synth->Unmute(); return true; }
   void Close(void) {synth->Mute(); synth->defaults();}

   //static methods
   static LV2_Handle	instantiate (const struct _LV2_Descriptor *, double sample_rate, const char *bundle_path, const LV2_Feature *const *features);
   static void connect_port(LV2_Handle instance, uint32_t port, void *data_location);
   static void activate(LV2_Handle instance);
   static void deactivate(LV2_Handle instance);
   static void run(LV2_Handle instance, uint32_t   sample_count);
   static void cleanup(LV2_Handle instance);
   static const void * extension_data(const char * uri);
   /*
   static LV2_Worker_Status lv2wrk_work(LV2_Handle instance, LV2_Worker_Respond_Function respond, LV2_Worker_Respond_Handle handle, uint32_t size, const void *data);
   static LV2_Worker_Status lv2wrk_response(LV2_Handle instance, uint32_t size, const void *body);
   static LV2_Worker_Status	lv2_wrk_end_run(LV2_Handle instance);
   */

};
