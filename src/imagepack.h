#ifndef IMAGEPACK_H
#define IMAGEPACK_H

#include <vector>
#include <string>
#include <boost/pool/object_pool.hpp>
#include <boost/multi_array.hpp>
#include <boost/format.hpp>
#include <boost/cstdint.hpp>

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
class Pixel
{
public:
    float r,g,b,a;

    Pixel();
    Pixel(float r, float g, float b, float a);
};



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
 * Sheet
 *--------------------------------------------------------------------------*/
struct Sheet
{
    std::vector<Image*> images;
    int width, height;
    Node *root;
    PixelData pixels;
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
    Sheet*                      packCompactSheet(std::vector<Image*> &to_pack, int max_width, int max_height);
    
    bool                        insertR(Node *node, Image *img);
    void                        blitSheets();
    void                        blitSheetR(Node *node, Sheet *sheet);

    bool                        validImageSize(Sheet *s, Image *img);
    void                        computeTexCoords();
    void                        printPackingStats();

    Node*                       createNode(int x, int y, int w, int h);
    Sheet*                      createSheet(int width, int height);
    void                        destroySheet(Sheet *s);
    void                        clearNodes();
    void                        clearImages();
    void                        clearSheets();

    unsigned int                nextPowerOfTwo(unsigned int n) const;

    void                        print(const boost::format &fmt, int type=INFO);
};

} /* end namespace Imagepack */

#endif /* IMAGEPACK_H */

