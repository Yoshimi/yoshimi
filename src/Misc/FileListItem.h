/*
    FileListItem.h

    Copyright 2010, Alan Calvert

    This file is part of yoshimi, which is free software: you can
    redistribute it and/or modify it under the terms of the GNU General
    Public License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    yoshimi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FILELISTITEM_H
#define FILELISTITEM_H

#include <limits>

class FileListItem {
    public:
        FileListItem() : index(numeric_limits<unsigned short>::max()) { };
        ~FileListItem() { };

        string  name;
        string  file;
        unsigned int index;

        inline bool operator ==(const FileListItem& param) const
        {
            return (name == param.name && file == param.file);
        }

        inline bool operator <(const FileListItem& param) const
        {
            if (index > 0)
               return sameFile(param);
            else
                return index < param.index;
        }

        inline bool sameFile(const FileListItem& param) const
        {
            if (name < param.name)
                return true;
            else if (name > param.name)
                return false;
            else
                return (file < param.file);
        }
};

#endif
