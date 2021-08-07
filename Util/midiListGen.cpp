/*
 * High accuracy program to create HTML formatted
 * list of musical note names, MIDI note numbers
 * and actual frequencies.
 * Only covers the useful range of note numbers
 * not the full range.
 *
 * Note:
 *   you can get an approximation of 12root2 with:
 *  196 / 185
 * this gives:
 *  1.05946
 *
 * 07/08/2021
 *
 * g++ -Wall midiListGen.cpp -o midiListGen
 *
 */
#include <math.h>
#include <limits>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <regex>

using namespace std;

std::string asLongString(double n)
{
   std::ostringstream oss;
   oss.precision(6);
   oss.width(6);
   oss << n;
   std::string value = oss.str();
   value = std::regex_replace(value, std::regex("^ +"), "");
   //if (value.length() < 5)
   if (value.find('.') == std::string::npos)
       value += '.';
   while (value.length() < 7)
       value += '0';
   return value;
}


int main(void)
{
    double twelf = 12.0;
    double two = 2.0;
    double multiplier;
    multiplier = pow(two, 1/twelf);
    //std::cout.precision(10);
    //std::cout << "res " << multiplier << std::endl;

    static std::string names [] = {
    "A", "#", "B", "C", "#", "D", "#", "E", "F", "#", "G", "#"
    };
    int stringcount = 0;
    int octave = 0;
    double result = 27.5;
    //std::cout.precision(6);
    std::string currentNote;
    std::string fullString;
    std::vector <std::string> ourlist;
    for (int i = 21; i < 109; ++i) // practical MIDI note range
    {
        currentNote = names[stringcount];
        if (currentNote == "C")
            ++ octave;
        if (currentNote != "#")
            currentNote += std::to_string(octave);
        ++ stringcount;
        if (stringcount >= 12)
            stringcount = 0;
        fullString = "        <td>" + currentNote + "</td><td>" + std::to_string(i) + "</td><td>" + asLongString(result) + "</td>";
        ourlist.push_back("      </tr>");
        ourlist.push_back(fullString);
        ourlist.push_back("      <tr align=\"center\">");
        result *= multiplier;
    }
    size_t idx = ourlist.size();
    ofstream midiList;
    midiList.open("midiList.txt");
    if (!midiList.is_open())
    {
        std::cout << "Failed to open midiList.txt" << std::endl;
        return 0;
    }
    while (idx > 0)
    {
        --idx;
        midiList << ourlist[idx] << endl;
    }
    midiList.close();
    return 0;
}
