#include <iostream>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if(argc < 4)
    {
        std::cerr << "yoshimi-cmdline supported commands:" <<  std::endl;
        std::cerr << "noteon <instancenum> <channel> <note> <velocity>" <<  std::endl;
        std::cerr << "noteoff <instancenum> <channel> <note>" <<  std::endl;
        std::cerr << "controller <instancenum> <channel> <type> <parameter>" <<  std::endl;
        std::cerr << "bank <instancenum> <bank>" <<  std::endl;
        std::cerr << "program <instancenum> <channel> <prg_num>" <<  std::endl;
        return 0;
    }
    int fd = open(YOSHIMI_CMDLINE_FIFO_NAME, O_WRONLY);
    if(fd == -1)
    {
        std::cerr << "Can't open yoshimi fifo file. Is Yoshimi running?" << std::endl;
        return -1;
    }
    std::stringstream ss;
    ss << argv [1];
    for(int i = 2; i < argc; ++i)
    {
        ss << " " << argv [i];
    }

    std::string sCmd = ss.str();

    std::cerr << sCmd << std::endl;

    write(fd, sCmd.c_str(), sCmd.length());

    close(fd);

    return 0;
}
