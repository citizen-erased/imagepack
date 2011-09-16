#ifndef IMAGEPACK_H
#define IMAGEPACK_H

#include <vector>
#include <string>
#include <boost/pool/object_pool.hpp>
#include <boost/multi_array.hpp>
#include <boost/format.hpp>
#include <boost/cstdint.hpp>
#include <boost/utility.hpp>

namespace Imagepack
{

enum
{
    INFO = 1,
    VERBOSE,

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
    pixel_array_t           pixels;
    mutable boost::uint32_t checksum;
    mutable bool            checksum_dirty;

public:
                    PixelData();

    void            resize(int width, int height);
    void            set(int x, int y, float r, float g, float b, float a=1.0f);
    void            set(int x, int y, Pixel p);
    void            fill(float r, float g, float b, float a);
    Pixel           get(int x, int y) const;
    int             width() const;
    int             height() const;

    boost::uint32_t getChecksum() const;

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
    Node *root;
    PixelData pixels;

public:
    Sheet(int width, int height);

    bool insert(Image *img);
    bool insertR(Node *node, Image *img);
    void blit();
    void blitR(Node *node);
    Node* createNode(int x, int y, int w, int h);
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
    bool                        compact;
    bool                        power_of_two;

public:
                                Packer();

    void                        pack();
    void                        addImage(const std::string &name);
    void                        setSheetSize(int width, int height);
    void                        setPowerOfTwo(bool value);
    void                        setCompact(bool value);
    void                        setTexCoordOrigin(int origin);
    int                         numSheets();
    Sheet*                      getSheet(int index);
    virtual bool                loadPixels(const std::string &name, PixelData &pixels) = 0;

    virtual void                print(const char *str, int type=INFO);

private:
    int                         packSheet(std::vector<Image*> &to_pack, Sheet *s);
    void                        packCompactSheet(std::vector<Image*> &to_pack, int max_width, int max_height);
    
    void                        blitSheets();

    bool                        validImageSize(const Sheet *s, const Image *img);
    void                        computeTexCoords();
    void                        printPackingStats();

    Sheet*                      createSheet(int width, int height);
    void                        destroySheet(Sheet *s);
    void                        clearSheets();
    void                        clearImages();

    unsigned int                nextPowerOfTwo(unsigned int n) const;

    void                        print(const boost::format &fmt, int type=INFO);
};

} /* end namespace Imagepack */

#endif /* IMAGEPACK_H */

