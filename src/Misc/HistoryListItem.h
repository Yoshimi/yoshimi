/*
    HistoryListItem.h

    Copyright 2010, Alan Calvert

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
*/

#ifndef HISTORYLISTITEM_H
#define HISTORYLISTITEM_H

#include <limits>

class HistoryListItem {
    public:
        HistoryListItem() :
            index(numeric_limits<unsigned short>::max()),
            program(0) { };
        ~HistoryListItem() { };

        string  name;
        string  file;
        unsigned int index;
        unsigned char program;

        inline bool operator ==(const HistoryListItem& param) const
        {
            return (name == param.name && file == param.file);
        }

        inline bool operator <(const HistoryListItem& param) const
        {
            return (index < param.index);
        }

        inline bool sameFile(const string& fileparam) const
        {
            return (file == fileparam);
        }
};

#endif
