#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include "output.h"
#include "image_io.h"
#include "imagepack.h"

#if IMAGEPACK_BUILD_HELP
    /* automatically generated by the build. */
    #include "cmd_help.h"
#else
    unsigned char *cmd_help     = NULL;
    unsigned int   cmd_help_len = 0;
#endif


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
static std::vector<std::string> input_paths;

/*
 * true to read paths from stdin. Appended to input_paths.
 */
static bool read_stdin = false;

/* 
 * output path including directory. prepended to all written files.
 * "/dir1/file"  will result in files like "/dir1/file.ext"
 */
static std::string cmd_output;

/*
 * size of the packed image sheets. must be in the format [0-9]+x[0-9]+
 */
static std::string cmd_img_size = "2048x2048";

/*
 * results of parsing cmd_img_size.
 */
static int cmd_sheet_width  = 2048;
static int cmd_sheet_height = 2048;

/* 
 * string to prepend to written files. parsed from cmd_output. if cmd_output is
 * "/dir/file" then the prepend will be "file"
 */
static std::string out_file_prepend;

/*
 * directory to write output files. parsed from cmd_output.
 */
static fs::path out_dir;

/*
 * Number of pixels to extrude the edges of each input image by.
 */
static int extrude = 0;

/*
 * True to keep sheets a power of two. set on the command line.
 */
static bool power_of_two = false;

/*
 * True to tightly pack sheets. set on the command line.
 */
static bool compact = false;

/*
 * true to scan directories recursively when building the list of files to
 * pack. set on the command line.
 */
static bool recursive = false;

/*
 * true to not write any files. set on the command line.
 */
static bool dry_run = false;

/*
 * true to disable the image cache and load/unload images when necessary.
 */
static bool no_cache = false;

/*
 * true to give detailed output. set on the command line.
 */
static bool verbose = false;

/*
 * true to disable printing. set on the command line.
 */
static bool silent = false;

/*
 * Where to use as the origin when computing texture coordinates for the
 * definitions file. cmd_tex_coord_origin is taken in on the command line and
 * parsed to set tex_coord_origin.
 */
static std::string cmd_tex_coord_origin = "bottom-left";
static int         tex_coord_origin     = BOTTOM_LEFT;

/*
 * Command line help string.
 */
static std::string help_str;


static void         parseCmdLine(int argc, char *argv[]);
static void         findFiles(std::vector<fs::path> &files);
static void         writeData();
static std::string  getSheetDefinitions(const fs::path &path, Sheet *s);

static Packer packer;



int main(int argc, char *argv[])
{
    help_str = std::string((char*)cmd_help, cmd_help_len);
    parseCmdLine(argc, argv);
    setWriteEnabled(!dry_run);

    if(silent)
        setPrintMode(SILENT);
    else if(verbose)
        setPrintMode(VERBOSE);
    else
        setPrintMode(INFO);


    out_dir          = fs::path(cmd_output).parent_path();
    out_file_prepend = fs::path(cmd_output).filename().string();

    if(out_file_prepend == "." && boost::ends_with(fs::path(cmd_output).string(), "/"))
        out_file_prepend = "";

    if(read_stdin)
    {
        std::string line;
        while(!std::getline(std::cin, line).eof())
            input_paths.push_back(line);
    }

    std::vector<fs::path> files;
    findFiles(files);

    print(format("%d files found\n") % files.size());

    if(files.empty())
        return EXIT_SUCCESS;

    packer.setSheetSize(cmd_sheet_width, cmd_sheet_height);
    packer.setTexCoordOrigin(tex_coord_origin);
    packer.setPowerOfTwo(power_of_two);
    packer.setCompact(compact);
    packer.setExtrude(extrude);
    packer.setCaching(!no_cache);

    for(size_t i = 0; i < files.size(); i++)
        packer.addImage(files[i].string());

    if(packer.numImages() == 0)
        return EXIT_SUCCESS;

    packer.pack();
    writeData();

    return EXIT_SUCCESS;
}

void findFiles(std::vector<fs::path> &files)
{
    std::vector<fs::path> paths(input_paths.begin(), input_paths.end());

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
}

void writeData()
{
    std::string defs;
    fs::path defs_path = out_dir / (out_file_prepend + ".defs");

    print(format("write directory   = %s\n")     % out_dir);
    print(format("write file prefix = \"%s\"\n") % out_file_prepend);

    if(!dry_run) fs::create_directories(out_dir);

    for(int i = 0; i < packer.numSheets(); i++)
    {
#if 0
        std::string name = str(format("%s%d.%s") % out_file_prepend % i % "png");
        print(format("writing sheet to %s\n") % (out_dir / name));
        saveImage(out_dir / name, packer.getSheet(i)->pixels);
        defs += getSheetDefinitions(out_dir / name, packer.getSheet(i));
#endif

        Sheet *s         = packer.getSheet(i);
        fs::path dst     = out_dir / str(format("%s%d.%s") % out_file_prepend % i % "png");

        print(format("writing sheet to %s\n") % dst);
        s->saveImage(dst);
        defs += getSheetDefinitions(dst, s);
    }

    print(format("writing definitions to %s\n") % defs_path);
    if(!dry_run)
    {
        fs::ofstream out(defs_path);
        out << defs;

        if(out.fail())
            print(format("failed to write %s\n") % defs_path, VERBOSE);
    }
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
            out += str(format("%d %d %d %d\n") % (img->sheet_x + img->source_x_offset) % (img->sheet_y + img->source_y_offset) % img->source_width % img->source_height);
            out += str(format("%f %f %f %f\n") % img->s0 % img->s1 % img->t0 % img->t1);
        }
    }

    return out;
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
         opts::value< std::vector<std::string> >(&input_paths),
         "Path a files or folder to pack. Multiple inputs can be specified as positional arguments.\n")

        ("stdin",
         opts::bool_switch(&read_stdin),
         "Read input paths from the standard input. Any paths read are appended to those specified by --input.\n")

        ("recursive,r",
         opts::bool_switch(&recursive),
         "Recurse into any directories specified.\n")

        ("image-size,s",
         opts::value<std::string>(&cmd_img_size),
         str(format("Size of the packed images. This is a maximum size if --compact is specified and a minimum size if --power-of-two is specified. Default size is set to %s. Example: --image-size 1024x1024\n") % cmd_img_size).c_str())

        ("power-of-two,p",
         opts::bool_switch(&power_of_two),
         "Keep sheet sizes a power of two. If a dimension in --image-size is not a power of two it is rounded up to the nearst power.\n")

        ("extrude,e",
         opts::value<int>(&extrude),
         "Number of pixels to extrude the edges of source images by. Example: --extrude 1. default = 0.\n")

        ("compact,c",
         opts::bool_switch(&compact),
         "Create sheets smaller than --image-size if possible.\n")

        ("tex-coord-origin,t",
         opts::value<std::string>(&cmd_tex_coord_origin),
         "Origin to use when computing sprite texture coordinates. Either bottom-left or top-left.\n")

        ("dry-run,d",
         opts::bool_switch(&dry_run),
         "Don't write any files.\n")

        ("no-cache",
         opts::bool_switch(&no_cache),
         "Disables caching of image data and causes images to be loaded and unloaded on demand. Useful when packing more images than can fit into memory.\n")

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

    if(cmd_tex_coord_origin == "bottom-left")
        tex_coord_origin = BOTTOM_LEFT;
    else if(cmd_tex_coord_origin == "top-left")
        tex_coord_origin = TOP_LEFT;
}

