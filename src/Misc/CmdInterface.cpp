#include <Misc/SynthEngine.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <string>
#include <iostream>

using namespace std;

extern map<SynthEngine *, MusicClient *> synthInstances;

void cmdIfaceProcessCommand(std::string cmd)
{
    cout << "processing command: " << cmd << endl;
}
