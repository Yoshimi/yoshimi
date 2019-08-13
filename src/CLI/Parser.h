/*
    Parser.h

    Copyright 2010, Alan Calvert
    Copyright 2014-2019, Will Godfrey

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 2 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

    Modified August 2019
*/

#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <cstring>
#include <cctype>
#include <readline/readline.h>
#include <readline/history.h>


namespace cli {

using std::string;


inline char * skipSpace(char * buf)
{
    while (isspace((unsigned char)*buf))
    {
        ++ buf;
    }
    return buf;
}


inline char * skipChars(char * buf)
{
    while (*buf && !isspace((unsigned char)*buf)) // will also stop on line ends
    {
        ++ buf;
    }
    buf = skipSpace(buf); // now find the next word (if any)
    return buf;
}


inline int matchWord(int numChars, char * buf, const char * word)
{
    int newp = 0;
    int size = strlen(word);
    while (buf[newp] > 0x20 && buf[newp] < 0x7f && newp < size && (tolower(buf[newp])) == (tolower(word[newp])))
            ++ newp;
    if (newp >= numChars && (buf[newp] <= 0x20 || buf[newp] >= 0x7f))
        return newp;
    return 0;
}


inline bool matchnMove(int num, char *& pnt, const char * word)
{
 bool found = matchWord(num, pnt, word);
 if (found)
     pnt = skipChars(pnt);
 return found;
}


inline bool startsWith(const char *point, const char *prefix)
{
    return 0 == strncmp(prefix, point, strlen(prefix));
}


inline bool lineEnd(char * point, unsigned char controlType)
{
    return (point[0] == 0 && controlType == TOPLEVEL::type::Write);
    // so all other controls aren't tested
    // e.g. you don't need to send a value when you're reading it!
}


inline int toggle(char  *point)
{
    if (matchnMove(2, point, "enable") || matchnMove(2, point, "on") || matchnMove(3, point, "yes"))
        return 1;
    if (matchnMove(2, point, "disable") || matchnMove(3, point, "off") || matchnMove(2, point, "no") )
        return 0;
    return -1;
    /*
     * this allows you to specify enable or other, disable or other or must be those specifics
     */
}


inline string asAlignedString(int n, int len)
{
    string res = std::to_string(n);
    int size = res.length();
    if (size < len)
    {
        for (int i = size; i < len; ++ i)
            res = " " + res;
    }
    return res;
}


/*
 * Finds the index number of an item in a string list. If 'min' <= 0 the
 * input string must be an exact match of all characters and of equal length.
 * Otherwise 'min' should be set to a value that provides the fewest tests
 * for an unambiguous match.
 * If a string in the list is shorter than 'min' then this length is used.
 */
inline int stringNumInList(string toFind, string * theList, size_t min)
{
    if (toFind.length() < min)
        return -1;
    int count = -1;
    string name;
    bool found = false;
    do
    {
        ++ count;
        name = theList[count];
        if (min > 0)
        {
            size_t match = name.length();
            if (match > min)
                match = min;
            int result = 0;
            for (string::size_type i = 0; i < match; ++i)
            {
                result |= (tolower(toFind[i]) ^ tolower(name[i]));
            }
            if (result == 0)
                found = true;
        }
        else // exact match
        {
            if (toFind == name)
                found = true;
        }
    }
    while (!found && name != "end");
    if (name == "end")
        return -1;
    return count;
}



class Parser
{
    char* buffer;
    char* point;
    char* mark;
    string prompt;
    string hist_filename;

    public:
        Parser() :
            buffer{nullptr},
            point{nullptr},
            mark{nullptr},
            prompt{"yoshimi> "},
            hist_filename{}
        { }

       ~Parser()
        {
            writeHistory();
            cleanUp();
        }

        // Parser is not copyable and can only be passed by reference
        Parser(const Parser&) = delete;
        Parser(Parser&&) = delete;
        Parser& operator=(const Parser&) = delete;
        Parser& operator=(Parser&&) = delete;


        // string conversion: get content after parsing point
        operator string()  const
        {
            return string{isValid()? point : ""};
        }

        bool isValid()  const
        {
            return point && strlen(point) < COMMAND_SIZE;
        }

        bool isTooLarge()  const
        {
            return point && strlen(point) >= COMMAND_SIZE;
        }


        void setPrompt(string newPrompt)
        {
            prompt = newPrompt;
        }

        void readline()
        {
            cleanUp();
            buffer = ::readline(prompt.c_str());
            if (!buffer)
                cleanUp();
            else
            {
                point = mark = buffer;
                add_history(buffer);
            }
        }

        // initialise the parser with an externally owned and managed buffer
        void initWithExternalBuffer(char* buffer)
        {
            if (!buffer || !*buffer) return;
            cleanUp();
            point = mark = buffer;
        }

        void setHistoryFile(string filename)
        {
            if (filename.length() == 0)
                return;
            else
                hist_filename = filename;

            using_history();
            stifle_history(80); // Never more than 80 commands
            if (read_history(hist_filename.c_str()) != 0)
            {   // reading failed
                perror(hist_filename.c_str());
                std::ofstream outfile (hist_filename.c_str()); // create an empty file
            }
        }

        void markPoint()
        {
            mark = point;
        }

        void reset_to_mark()
        {
            point = mark;
        }

    private:
        void cleanUp()
        {
            if (buffer)
                free(buffer);
            buffer = mark = point = nullptr;
        }

        void writeHistory()
        {
            if (hist_filename.length() == 0)
                return;
            if (write_history(hist_filename.c_str()) != 0)
            {   // writing of history file failed
                perror(hist_filename.c_str());
            }
        }


    public:
        void trim()
        {
            if (!point) return;
            this->skipSpace();
            char *end = point + strlen(point) - 1;
            while (end > point && isspace((unsigned char)*end))
            {
                end--;
            }
            end[1] = '\0';  // place new end terminator
        }


        /* ==== Parsing API ==== */

        bool matchnMove(int num, const char * word)
        {
            return cli::matchnMove(num, point, word);
        }

        int matchWord(int numChars, const char * word)
        {
            return cli::matchWord(numChars, point, word);
        }

        void skipSpace()
        {
            point = cli::skipSpace(point);
        }

        void skipChars()
        {
            point = cli::skipChars(point);
        }

        void skip(int cnt)
        {
            point += cnt;
        }

        int toggle()
        {
            return cli::toggle(point);
        }

        bool lineEnd(unsigned char controlType)
        {
            return cli::lineEnd(point, controlType);
        }

        bool isAtEnd()
        {
            return *point == '\0';
        }

        bool nextChar(char expected)
        {
            return *point == expected;
        }

        bool startsWith(const char *prefix)
        {
            return cli::startsWith(point, prefix);
        }

        bool isdigit()
        {
            return ::isdigit(*point);
        }

        char peek()
        {
            return *point;
        }
};


}//(End)namespace cli
#endif /*PARSER_H*/
