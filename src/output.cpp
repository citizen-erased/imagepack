#include <cstdio>
#include "output.h"

namespace {
int print_mode = Imagepack::VERBOSE;
}

namespace Imagepack
{

void print(const char *str, int type)
{
    if(print_mode >= type)
        printf("%s", str);
}

void print(const std::string &str,  int type) { print(str.c_str(), type); }
void print(const boost::format &fmt,int type) { print(boost::str(fmt).c_str(), type); }
void setPrintMode(int mode)                   { print_mode = mode; }

} /* end namespace Imagepack */

