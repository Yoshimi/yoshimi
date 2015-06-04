#
#   CMakeLists.txt
#
#   This file is part of yoshimi, which is free software: you can
#   redistribute it and/or modify it under the terms of the GNU General
#   Public License as published by the Free Software Foundation, either
#   version 3 of the License, or (at your option) any later version.
#
#   yoshimi is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with yoshimi.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
file(READ src/version.txt YOSHIMI_VERSION)
string(STRIP ${YOSHIMI_VERSION} YOSHIMI_VERSION)

# tarball target
message(STATUS "Tarball yoshimi-${YOSHIMI_VERSION}")
add_custom_target(tarball
    COMMAND rm -rf yoshimi-${YOSHIMI_VERSION}
    COMMAND mkdir yoshimi-${YOSHIMI_VERSION}
    COMMAND  rsync --recursive --times --perms
             COPYING INSTALL README.txt examples banks src CMakeLists.txt
             yoshimi-${YOSHIMI_VERSION}/
    COMMAND tar -cvjf "yoshimi-${YOSHIMI_VERSION}.tar.bz2" yoshimi-${YOSHIMI_VERSION}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM
)
