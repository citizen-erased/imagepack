#ifndef OUTPUT_H
#define OUTPUT_H

#include <boost/format.hpp>
#include <string>

namespace Imagepack {

enum
{
    SILENT,
    INFO,
    VERBOSE
};

void setPrintMode(int mode);
void print(const char *str, int type=INFO);
void print(const std::string &str, int type=INFO);
void print(const boost::format &fmt, int type=INFO);
bool fatal(const boost::format &fmt);

}

#endif /* OUTPUT_H */

