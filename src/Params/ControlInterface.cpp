#include <stdlib.h>

using namespace std;

#include "Params/ControlInterface.h"
#include <iostream>

string ControlInterface::makeIdWithChannel(char _channel, const string &groupName, const string &controlName)
{
    string id = groupName + string("_") + controlName;
    if(_channel >= 0 && _channel < 16)
    {
        stringstream ss(id);
        ss << "_" << _channel;
    }
    return id;

}

ControlInterface::ControlInterface(SynthEngine *_synth):
    synth(_synth)
{
    sem_init(&channelLock, 0, 1);
}

ControlInterface::~ControlInterface()
{
    sem_destroy(&channelLock);

}

void ControlInterface::registerControl(string groupName, string controlName, YoshimiControlType controlType, float defVal, float minVal, float maxVal, float step, float *val)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlParams cp;
    cp.channel = channel;
    cp.groupName = groupName;
    cp.controlName = controlName;
    cp.type = controlType;
    cp.defVal = defVal;
    cp.minVal = minVal;
    cp.maxVal = maxVal;
    cp.step = step;
    cp.val = val;
    controls.insert(make_pair<string, YoshimiControlParams>(id, cp));
    //also register control group for fast searching
    groups.insert(groupName);
}

void ControlInterface::setDefVal(string groupName, string controlName, float defVal)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.defVal = defVal;
    }
}

void ControlInterface::setMinVal(string groupName, string controlName, float minVal)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.minVal = minVal;
    }

}

void ControlInterface::setMaxVal(string groupName, string controlName, float maxVal)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.maxVal = maxVal;
    }

}

void ControlInterface::setStep(string groupName, string controlName, float step)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.step = step;
    }

}

void ControlInterface::setType(string groupName, string controlName, YoshimiControlType controlType)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.type = controlType;
    }
}

void ControlInterface::connect(string groupName, string controlName, float *val)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        it->second.val = val;
    }
}

void ControlInterface::set(string id, float val)
{
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        *(it->second.val) = val;
    }
}

bool ControlInterface::get(string id, float *val)
{
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        *val = *(it->second.val);
        return true;
    }
    return false;
}

void ControlInterface::unregisterControl(string groupName, string controlName)
{
    if(!checkChannel())
        return;
    string id = makeId(groupName, controlName);
    YoshimiControlMapIterator it = controls.find(id);
    if(it != controls.end())
    {
        controls.erase(it);
        //if it was the last control in the group, erase the group too
        it = controls.lower_bound(groupName);
        if(it == controls.end())
        {
            groups.erase(groupName);
        }
        else if(it != controls.end() && it->first.substr(0, groupName.size()) != groupName)
        {
            groups.erase(groupName);
        }
    }

}

void ControlInterface::dump()
{
    cout << "<------------Dumping controls by group:------------>" << endl << endl;
    for(YoshimiControlGroupMapIterator itg = groups.begin(); itg != groups.end(); ++itg)
    {
        cout << "\t[GRP] " << *(itg) << endl;
        for(YoshimiControlMapIterator it = controls.lower_bound(*(itg)); it != controls.upper_bound(*(itg)); ++it)
        {
            cout << "\t\t[CTRL] chn=" << it->second.channel << ", id=" << it->first << ", name=" << it->second.controlName << endl;
        }

    }


}
