#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

#include <string>
#include <sstream>
#include <map>
#include <set>
#include "Misc/SynthEngine.h"
#include "semaphore.h"

using namespace std;

enum YoshimiControlType
{
    YOSHIMI_CONTROL_TYPE_DESCRETE = 0,
    YOSHIMI_CONTROL_TYPE_CONTINUOUS,
    YOSHIMI_CONTROL_TYPE_SWITCH
};

struct YoshimiControlParams
{
    char channel;
    string groupName;
    string controlName;
    YoshimiControlType type;
    float defVal;
    float minVal;
    float maxVal;
    float step;
    float *val;
};

typedef map<string, YoshimiControlParams> YoshimiControlMap;
typedef map<string, YoshimiControlParams>::iterator YoshimiControlMapIterator;

typedef set<string> YoshimiControlGroupMap;
typedef set<string>::iterator YoshimiControlGroupMapIterator;

class ControlInterface
{
private:
    SynthEngine *synth;
    YoshimiControlMap controls;
    YoshimiControlGroupMap groups;
    inline string makeIdWithChannel(char _channel, const string &groupName, const string &controlName);
    inline string makeId(const string &groupName, const string &controlName)
    {
        return makeIdWithChannel(channel, groupName, controlName);
    }

    sem_t channelLock;
    char channel;
public:
    ControlInterface(SynthEngine *_synth);
    ~ControlInterface();
    bool pushChannel(char _channel)
    {
        if(sem_trywait(&channelLock) != 0)
        {
            string msg;
            stringstream ss(msg);
            ss << "ControlInterface: can't perform channel lock for ch #" << channel;
            synth->getRuntime().Log(msg, true);
            return false;
        }
        channel = _channel;
    }

    void popChannel()
    {
        channel = -1;
        sem_post(&channelLock);
    }

    bool checkChannel()
    {
        int sVal = 0;
        if(sem_getvalue(&channelLock, &sVal) != 0)
        {
            return false; // error getting value from semaphore
        }
        if(sVal > 0)
        {
            synth->getRuntime().Log("ControlInterface::registerControl: pushChannel() was not called!");
            return false;
        }
        return true;
    }

    void registerControl(string groupName, string controlName, YoshimiControlType controlType,
                                                           float defVal, float minVal, float maxVal, float step, float *val);
    void setDefVal(string groupName, string controlName, float defVal);
    void setMinVal(string groupName, string controlName, float minVal);
    void setMaxVal(string groupName, string controlName, float maxVal);
    void setStep(string groupName, string controlName, float step);
    void setType(string groupName, string controlName, YoshimiControlType controlType);
    void connect(string groupName, string controlName, float *val);
    void set(string id, float val);
    bool get(string id, float *val);
    void unregisterControl(string groupName, string controlName);
    void dump();

};



#endif

