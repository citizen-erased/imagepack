#ifndef IMAGEPACK_H
#define IMAGEPACK_H

#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/multi_array.hpp>
#include <boost/format.hpp>
#include <boost/cstdint.hpp>
#include <boost/utility.hpp>

namespace Imagepack
{

enum
{
    BOTTOM_LEFT,
    TOP_LEFT
};

/*--------------------------------------------------------------------------*
 * Pixel
 *--------------------------------------------------------------------------*/
class PixelFloat
{
private:
    float r,g,b,a;

public:
    PixelFloat();
    PixelFloat(float r, float g, float b, float a);

    void set(float r, float g, float b, float a);
    
    uint8_t redByte()   const;
    uint8_t greenByte() const;
    uint8_t blueByte()  const;
    uint8_t alphaByte() const;

    bool operator==(const PixelFloat &o) const;
    bool operator!=(const PixelFloat &o) const;
};

class Pixel32
{
private:
    uint32_t rgba;

public:
    Pixel32();
    Pixel32(float r, float g, float b, float a);

    void set(float r, float g, float b, float a);

    uint8_t redByte()   const;
    uint8_t greenByte() const;
    uint8_t blueByte()  const;
    uint8_t alphaByte() const;

    bool operator==(const Pixel32 &o) const;
    bool operator!=(const Pixel32 &o) const;
};

/*
 * Set the pixel type to use.
 *
 * Since only 32 bit imges are read at the moment there's no point using
 * floating point pixels.  Using 32bit saves x4 memory and is a bit faster
 */
typedef Pixel32 Pixel;


/*--------------------------------------------------------------------------*
 * PixelData
 *--------------------------------------------------------------------------*/
typedef boost::multi_array<Pixel, 2> pixel_array_t;

class PixelData
{
private:
    pixel_array_t   pixels;

public:
    void            resize(int width, int height);
    void            set(int x, int y, float r, float g, float b, float a=1.0f);
    void            set(int x, int y, Pixel p);
    void            fill(float r, float g, float b, float a);
    void            fillRect(int x0, int y0, int x1, int y1, Pixel p);
    void            blit(int px, int py, const PixelData &data);

    Pixel           get(int x, int y) const;
    int             width() const;
    int             height() const;
    uint32_t        computeChecksum() const;

    bool operator==(const PixelData &o) const;
};


/*--------------------------------------------------------------------------*
 * Packed Image
 *--------------------------------------------------------------------------*/

/**
 * A padded image is the original image data with any additionl borders or
 * padding.  Both padded and source image coordinates are stored relative to a
 * sheet in rectangles.  The source rectangle should always be equal to or
 * smaller than the padded on and should always be contained in it.
 */

#if 0
class Image
{
public:
    /* coordinates of the image in a sheet including any borders/padding */
    int x, y;

    /* size of the image including any borders/padding */
    int width, height;

    /* coordinates in a sheet of the source image data */
    int source_x, source_y;

    /* size of the source image */
    int source_width, source_height;

    /* normalized texture coordinates */
    float s0, s1, t0, t1;

    /* true if the image was packed. used during packing */
    bool is_packed;

    /* names of all images that refer to the pixel data */
    std::vector<std::string> names;

    /* modified pixel data including borders */
    PixelData pixels;

public:
    Image();

    bool loadData();
};
#endif


class Image
{
public:
    /* coordinates of the image in a sheet including any borders/padding */
    int sheet_x, sheet_y;

    /* size of the image including any borders/padding */
    int width, height;

    /* coordinates in a sheet of the source image data relative to [sheet_x, sheet_y] */
    int source_x_offset, source_y_offset;

    /* size of the source image */
    int source_width, source_height;

    /* normalized texture coordinates */
    float s0, s1, t0, t1;

    /* number of pixels to extrude each edge by */
    int extrude;

    /* true if the image was packed. used during packing */
    bool is_packed;

    /* true if pixel data is currently loaded */
    bool has_data;

    /* names of all images that refer to the pixel data */
    std::vector<std::string> names;

    /* modified pixel data including borders */
    PixelData pixels;

    /* pixel data checksum for equality and recreating image data */
    uint32_t checksum;


public:
    bool                initialize(const std::string &name, int extrude);
    const PixelData&    getPixels();
    bool                equalPixelData(Image &other);
    void                purgeMemory();
    void                addName(const std::string &name);

private:
    bool                createImageData();
    bool                recreateImageData();
};

/*--------------------------------------------------------------------------*
 * Node
 *--------------------------------------------------------------------------*/
struct Node
{
    Node *left, *right;
    Image *img;
   
    /* coordinates of the node relative to the root */
    int x, y;

    /* node's dimensions */
    int width, height;
};


/*--------------------------------------------------------------------------*
 * sheet
 *--------------------------------------------------------------------------*/
class Sheet : private boost::noncopyable
{
public:
    boost::object_pool<Node> node_pool;
    std::vector<Image*> images;
    std::vector<Node*> nodes;
    int width, height;
    int extrude;
    Node *root;

public:
    Sheet(int width, int height);

    bool insert(Image *img);
    bool insertR(Node *node, Image *img);
    void blit(PixelData &pixels);
    void blitR(Node *node, PixelData &pixels);
    Node* createNode(int x, int y, int w, int h);
    bool saveImage(boost::filesystem::path &path);
};



/*--------------------------------------------------------------------------*
 * Packer
 *--------------------------------------------------------------------------*/
class Packer
{
private:
    boost::object_pool<Image>   image_pool;
    boost::object_pool<Node>    node_pool;
    boost::object_pool<Sheet>   sheet_pool;

    std::vector<Image*>         images;
    std::vector<Node*>          nodes;
    std::vector<Sheet*>         sheets;
    int                         sheet_width;
    int                         sheet_height;
    int                         tex_coord_origin;
    int                         extrude;
    bool                        compact;
    bool                        power_of_two;
    bool                        cache_images;

public:
                                Packer();

    void                        pack();
    void                        addImage(const std::string &name);
    int                         numImages();
    void                        setSheetSize(int width, int height);
    void                        setPowerOfTwo(bool value);
    void                        setCompact(bool value);
    void                        setTexCoordOrigin(int origin);
    void                        setExtrude(int extrude);
    void                        setCaching(bool cache);
    int                         numSheets();
    Sheet*                      getSheet(int index);

private:
    int                         packSheet(std::vector<Image*> &to_pack, Sheet *s);
    void                        packCompactSheet(std::vector<Image*> &to_pack, int max_width, int max_height);
    
    void                        blitSheets();

    void                        computeTexCoords();
    void                        printPackingStats();

    Sheet*                      createSheet(int width, int height);
    void                        destroySheet(Sheet *s);
    void                        clearSheets();
    void                        clearImages();

    unsigned int                nextPowerOfTwo(unsigned int n) const;
};

} /* end namespace Imagepack */

#endif /* IMAGEPACK_H */

