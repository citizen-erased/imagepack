#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include <boost/filesystem.hpp>

namespace Imagepack
{

class PixelData;

void setWriteEnabled(bool enabled);
bool loadImage(const boost::filesystem::path &path, PixelData &pixels);
bool saveImage(const boost::filesystem::path &path, PixelData &pixels);

}

#endif /* IMAGE_IO_H */

