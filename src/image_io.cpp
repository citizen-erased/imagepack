/*
 * Include Windows.h before FreeImage.h since freeimage will define some things
 * that are supposed to be defined in Widows.h and will break boost.
 */
#if _WIN32
#include <Windows.h>
#endif

#include <FreeImage.h>
#include "output.h"
#include "imagepack.h"
#include "image_io.h"

#if _WIN32
    #define IMAGEPACK_FreeImage_GetFileType(a, b)       FreeImage_GetFileTypeU((a), (b))
    #define IMAGEPACK_FreeImage_GetFIFFromFilename(a)   FreeImage_GetFIFFromFilenameU((a))
    #define IMAGEPACK_FreeImage_Load(a, b, c)           FreeImage_LoadU((a), (b), (c))
    #define IMAGEPACK_FreeImage_Save(a, b, c, d)        FreeImage_SaveU((a), (b), (c), (d))
#else
    #define IMAGEPACK_FreeImage_GetFileType(a, b)       FreeImage_GetFileType((a), (b))
    #define IMAGEPACK_FreeImage_GetFIFFromFilename(a)   FreeImage_GetFIFFromFilename((a))
    #define IMAGEPACK_FreeImage_Load(a, b, c)           FreeImage_Load((a), (b), (c))
    #define IMAGEPACK_FreeImage_Save(a, b, c, d)        FreeImage_Save((a), (b), (c), (d))
#endif

using boost::format;

namespace {

bool write_enabled = true;
bool initialized   = false;

void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message)
{
    Imagepack::print("FreeImage Error", Imagepack::VERBOSE);

    if(fif != FIF_UNKNOWN)
        Imagepack::print(format(" (%s Format)") % FreeImage_GetFormatFromFIF(fif), Imagepack::VERBOSE);

    Imagepack::print(format(": %s\n") % message, Imagepack::VERBOSE);
}

void ensureInitialized()
{
    if(initialized) return;
    FreeImage_SetOutputMessage(FreeImageErrorHandler);
    initialized = true;
}

} /* end unnamed namespace */



namespace Imagepack {

void setWriteEnabled(bool enabled)
{
    write_enabled = enabled;
}

bool loadImage(const boost::filesystem::path &path, PixelData &pixels)
{
    ensureInitialized();
    print(format("loading image %s\n") % path, VERBOSE);

    FREE_IMAGE_FORMAT fif = IMAGEPACK_FreeImage_GetFileType(path.c_str(), 0);

    if(fif == FIF_UNKNOWN)
		fif = IMAGEPACK_FreeImage_GetFIFFromFilename(path.c_str());

    if(fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
    {
        print("format not supported or not an image file\n", VERBOSE);
        return false;
    }

    FIBITMAP *dib = IMAGEPACK_FreeImage_Load(fif, path.c_str(), 0);

    if(!dib)
    {
        print("failed to load image data\n", VERBOSE);
        return false;
    }

    if(!FreeImage_HasPixels(dib))
    {
        print("no pixel data\n", VERBOSE);
        FreeImage_Unload(dib);
        return false;
    }

    FIBITMAP *img = FreeImage_ConvertTo32Bits(dib);
    FreeImage_Unload(dib);

    if(!img)
    {
        print("failed to convert image to 32bits\n", VERBOSE);
        return false;
    }

    int bytes_per_pixel = FreeImage_GetLine(img) / FreeImage_GetWidth(img);
    pixels.resize(FreeImage_GetWidth(img), FreeImage_GetHeight(img));

    for(int y = 0, h = pixels.height(); y < h; y++)
    {
        BYTE *bits = FreeImage_GetScanLine(img, h-y-1);

        for(int x = 0, w = pixels.width(); x < w; x++)
        {
            float r = bits[FI_RGBA_RED]   / 255.0f;
            float g = bits[FI_RGBA_GREEN] / 255.0f;
            float b = bits[FI_RGBA_BLUE]  / 255.0f;
            float a = bits[FI_RGBA_ALPHA] / 255.0f;

            pixels.set(x, y, r, g, b, a);
            bits += bytes_per_pixel;
        }
    }

    FreeImage_Unload(img);
    return true;
}

bool saveImage(const boost::filesystem::path &path, PixelData &pixels)
{
    ensureInitialized();
    FIBITMAP *dib = FreeImage_Allocate(pixels.width(), pixels.height(), 32);

    if(!dib)
        return false;

    for(int y = 0; y < pixels.height(); y++)
    {
        BYTE *bits = FreeImage_GetScanLine(dib, pixels.height()-y-1);

        for(int x = 0; x < pixels.width(); x++)
        {
            bits[FI_RGBA_RED]   = pixels.get(x, y).redByte();
            bits[FI_RGBA_GREEN] = pixels.get(x, y).greenByte();
            bits[FI_RGBA_BLUE]  = pixels.get(x, y).blueByte();
            bits[FI_RGBA_ALPHA] = pixels.get(x, y).alphaByte();

            bits += 4;
        }
    }

    print(format("writing %s\n") % path, VERBOSE);

    if(write_enabled)
        if(!IMAGEPACK_FreeImage_Save(FIF_PNG, dib, path.c_str(), 0))
        {
            print("failed to write image\n", VERBOSE);
            return false;
        }

    FreeImage_Unload(dib);
    return true;
}


} /* end namespace Imagepack */

