#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <FreeImage.h>
#include "imagepack.h"

/* automatically generated by the build. */
#include "cmd_help.h"


namespace fs = boost::filesystem;
namespace opts = boost::program_options;

using boost::format;
using boost::str;
using namespace Imagepack;


/*
 * files to pack or directories to search for files in.  if a directory is
 * specified all files in it will be added to the list of files to pack.  if
 * recursive is set, directories are scanned recursively.
 */
std::vector<std::string> cmd_paths;

/* 
 * output path including directory. prepended to all written files.
 * "/dir1/file"  will result in files like "/dir1/file.ext"
 */
std::string cmd_output;

/*
 * size of the packed image sheets. must be in the format [0-9]+x[0-9]+
 */
std::string cmd_img_size = "2048x2048";

/*
 * results of parsing cmd_img_size.
 */
int cmd_sheet_width  = 2048;
int cmd_sheet_height = 2048;

/* 
 * string to prepend to written files. parsed from cmd_output. if cmd_output is
 * "/dir/file" then the prepend will be "file"
 */
std::string out_file_prepend;

/*
 * directory to write output files. parsed from cmd_output.
 */
fs::path out_dir;

/*
 * True to keep sheets a power of two. set on the command line.
 */
bool power_of_two = false;

/*
 * True to tightly pack sheets. set on the command line.
 */
bool compact = false;

/*
 * true to scan directories recursively when building the list of files to
 * pack. set on the command line.
 */
bool recursive = false;

/*
 * true to not write any files. set on the command line.
 */
bool dry_run = false;

/*
 * true to give detailed output. set on the command line.
 */
bool verbose = false;

/*
 * true to disable printing. set on the command line.
 */
bool silent = false;

/*
 * 0 disables all printing. print levels defined in the Imagepack namespace
 */
int print_mode = 1;

/*
 * Where to use as the origin when computing texture coordinates for the
 * definitions file. cmd_tex_coord_origin is taken in on the command line and
 * parsed to set tex_coord_origin.
 */
std::string cmd_tex_coord_origin = "bottom-left";
int         tex_coord_origin     = BOTTOM_LEFT;

std::string help_str;

static void parseCmdLine(int argc, char *argv[]);
static bool loadImage(const fs::path &path, PixelData &pixels);
static void saveImage(const fs::path &path, PixelData &pixels);
static std::string getSheetDefinitions(const fs::path &path, Sheet *s);
static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message);

static void print(const char *str, int type=INFO);
static void print(const std::string &str, int type=INFO);
static void print(const format &fmt, int type=INFO);


class CmdPacker : public Packer
{
public:
    bool loadPixels(const std::string &name, PixelData &pixels) { return loadImage(name, pixels); }
    void print(const char *str, int type) { ::print(str, type); }
};



int main(int argc, char *argv[])
{
    help_str = std::string((char*)cmd_help, cmd_help_len);
    parseCmdLine(argc, argv);

    FreeImage_SetOutputMessage(FreeImageErrorHandler);

    out_dir          = fs::path(cmd_output).parent_path();
    out_file_prepend = fs::path(cmd_output).filename().string();

    if(out_file_prepend == "." && boost::ends_with(fs::path(cmd_output).string(), "/"))
        out_file_prepend = "";

    std::vector<fs::path> files, paths(cmd_paths.begin(), cmd_paths.end());

    while(!paths.empty())
    {
        fs::path path = paths.back();
        paths.pop_back();

        if(fs::is_regular_file(path))
            files.push_back(path);
        else if(fs::is_directory(path))
        {
            for(fs::directory_iterator it = fs::directory_iterator(path); it != fs::directory_iterator(); it++)
                if(!fs::is_directory(it->path()) || recursive)
                    paths.push_back(it->path());
        }
    }

    print(format("%d files found\n") % files.size());

    if(files.empty())
        return EXIT_SUCCESS;

    CmdPacker packer;
    packer.setSheetSize(cmd_sheet_width, cmd_sheet_height);
    packer.setTexCoordOrigin(tex_coord_origin);
    packer.setPowerOfTwo(power_of_two);
    packer.setCompact(compact);

    for(size_t i = 0; i < files.size(); i++)
        packer.addImage(files[i].string());

    packer.pack();

    if(!dry_run)
        fs::create_directories(out_dir);

    print(format("write directory   = %s\n")     % out_dir);
    print(format("write file prefix = \"%s\"\n") % out_file_prepend);

    std::string defs;
    for(int i = 0; i < packer.numSheets(); i++)
    {
        std::string name = str(format("%s%d.%s") % out_file_prepend % i % "png");
        print(format("writing sheet to %s\n") % (out_dir / name));
        saveImage(out_dir / name, packer.getSheet(i)->pixels);
        defs += getSheetDefinitions(out_dir / name, packer.getSheet(i));
    }

    fs::path defs_path = out_dir / (out_file_prepend + ".defs");
    print(format("writing definitions to %s\n") % defs_path);

    if(!dry_run)
    {
        fs::ofstream out(defs_path);
        out << defs;

        if(out.fail())
            print(format("failed to write %s\n") % defs_path, VERBOSE);
    }

    return EXIT_SUCCESS;
}

void parseCmdLine(int argc, char *argv[])
{
    opts::options_description desc("Options");

    desc.add_options()
        ("help,h", "Print this message.\n")

        ("output,o", 
         opts::value<std::string>(&cmd_output)->required(),
         "Path to prepend to all files written.\n")

        ("input,i",
         opts::value< std::vector<std::string> >(&cmd_paths)->required(),
         "Path a files or folder to pack. Multiple inputs can be specified as positional arguments.\n")

        ("recursive,r",
         opts::bool_switch(&recursive),
         "Recurse into any directories specified.\n")

        ("image-size,s",
         opts::value<std::string>(&cmd_img_size),
         str(format("Size of the packed images. This is a maximum size if --compact is specified and a minimum size if --power-of-two is specified. Default size is set to %s. Example: --image-size 1024x1024\n") % cmd_img_size).c_str())

        ("power-of-two,p",
         opts::bool_switch(&power_of_two),
         "Keep sheet sizes a power of two. If a dimension in --image-size is not a power of two it is rounded up to the nearst power.\n")

        ("compact,c",
         opts::bool_switch(&compact),
         "Create sheets smaller than --image-size if possible.\n")

        ("tex-coord-origin,t",
         opts::value<std::string>(&cmd_tex_coord_origin),
         "Origin to use when computing sprite texture coordinates. Either bottom-left or top-left.\n")

        ("dry-run,d",
         opts::bool_switch(&dry_run),
         "Don't write any files.\n")

        ("silent,S",
         opts::bool_switch(&silent),
         "Disables printing.\n")

        ("verbose,v",
         opts::bool_switch(&verbose),
         "Print detailed information.\n")
    ;

    opts::positional_options_description p;
    p.add("input", -1);

    opts::variables_map vars;

    try
    {
        opts::store(opts::command_line_parser(argc, argv).options(desc).positional(p).run(), vars);

        if(vars.count("help"))
        {
            print(help_str);
            print(format("%1%") % desc);
            exit(EXIT_SUCCESS);
        }

        opts::notify(vars);
    }
    catch(const opts::error &e)
    {
        print(format("%1%\n\n") % e.what());
        print(help_str);
        print(format("%1%") % desc);
        exit(EXIT_FAILURE);
    }


    std::vector<std::string> strs;
    boost::split(strs, cmd_img_size, boost::is_any_of("x"));

    if(strs.size() != 2)
    {
        print("error parsing image-size\n");
        exit(EXIT_FAILURE);
    }

    try
    {
        cmd_sheet_width  = boost::lexical_cast<int>(strs[0]);
        cmd_sheet_height = boost::lexical_cast<int>(strs[1]);
    }
    catch(const boost::bad_lexical_cast &e)
    {
        print("error parsing image-size\n");
        exit(EXIT_FAILURE);
    }

    if(silent)
        print_mode = 0;
    else if(verbose)
        print_mode = VERBOSE;
    else
        print_mode = INFO;

    if(cmd_tex_coord_origin == "bottom-left")
        tex_coord_origin = BOTTOM_LEFT;
    else if(cmd_tex_coord_origin == "top-left")
        tex_coord_origin = TOP_LEFT;
    else
        print("unknown texture coordinate origin\n");
}

bool loadImage(const fs::path &path, PixelData &pixels)
{
    print(format("loading image %s\n") % path, VERBOSE);

    FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(path.c_str(), 0);

    if(fif == FIF_UNKNOWN)
        fif = FreeImage_GetFIFFromFilename(path.c_str());

    if(fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
    {
        print("format not supported or not an image file\n", VERBOSE);
        return false;
    }

    FIBITMAP *dib = FreeImage_Load(fif, path.c_str(), 0);

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

    for(int y = 0; y < pixels.height(); y++)
    {
        BYTE *bits = FreeImage_GetScanLine(img, pixels.height()-y-1);

        for(int x = 0; x < pixels.width(); x++)
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

void saveImage(const fs::path &path, PixelData &pixels)
{
    FIBITMAP *dib = FreeImage_Allocate(pixels.width(), pixels.height(), 32);

    if(!dib)
        return;

    for(int y = 0; y < pixels.height(); y++)
    {
        BYTE *bits = FreeImage_GetScanLine(dib, pixels.height()-y-1);

        for(int x = 0; x < pixels.width(); x++)
        {
            bits[FI_RGBA_RED]   = pixels.get(x, y).r * 255.0f;
            bits[FI_RGBA_GREEN] = pixels.get(x, y).g * 255.0f;
            bits[FI_RGBA_BLUE]  = pixels.get(x, y).b * 255.0f;
            bits[FI_RGBA_ALPHA] = pixels.get(x, y).a * 255.0f;

            bits += 4;
        }
    }

    print(format("writing %s\n") % path, VERBOSE);

    if(!dry_run)
    {
        if(!FreeImage_Save(FIF_PNG, dib, path.c_str(), 0))
        {
            print("failed to write image\n", VERBOSE);
        }
    }

    FreeImage_Unload(dib);
}


void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
    print("FreeImage Error", VERBOSE);

    if(fif != FIF_UNKNOWN)
        print(format(" (%s Format)") % FreeImage_GetFormatFromFIF(fif), VERBOSE);

    print(format(": %s\n") % message, VERBOSE);
}

std::string getSheetDefinitions(const fs::path &path, Sheet *s)
{
    std::string out;

    for(size_t i = 0; i < s->images.size(); i++)
    {
        Image *img = s->images[i];

        for(size_t j = 0; j < img->names.size(); j++)
        {
            out += str(format("%s\n") % img->names[j]);
            out += str(format("%s\n") % path.string());
            out += str(format("%d %d %d %d\n") % img->source_x % img->source_y % img->source_width % img->source_height);
            out += str(format("%f %f %f %f\n") % img->s0 % img->s1 % img->t0 % img->t1);
        }
    }

    return out;
}


void print(const char *str, int type)
{
    if(print_mode >= type)
        printf("%s", str);
}

void print(const std::string &str, int type) { print(str.c_str()); }
void print(const format &fmt,      int type) { print(boost::str(fmt).c_str(), type); }

