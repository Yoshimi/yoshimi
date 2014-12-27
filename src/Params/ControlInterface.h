#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

#include <string>
#include "Misc/SynthEngine.h"

using namespace std;

enum YoshimiControlType
{
    YOSHIMI_CONTROL_TYPE_DESCRETE = 0,
    YOSHIMI_CONTROL_TYPE_CONTINUOUS,
    YOSHIMI_CONTROL_TYPE_SWITCH
};

void registerControl(SynthEngine *s, unsigned char channel, string groupName, string controlName, YoshimiControlType controlType,
                                                       float defVal, float minVal, float maxVal, float step);
void unregisterControl(SynthEngine *s, unsigned char channel, string groupName, string controlName);

#endif

