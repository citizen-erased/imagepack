#include <cstdio>
#include <algorithm>
#include <boost/crc.hpp>
#include "output.h"
#include "image_io.h"
#include "imagepack.h"

using boost::format;
using namespace Imagepack;


namespace {

bool imageHeightCompare(Image *a, Image *b) { return a->height > b->height; }
bool imageWidthCompare(Image *a, Image *b)  { return a->width > b->width;   }

} /* end unnamed namespace */


/*--------------------------------------------------------------------------*
 *
 * PixelFloat
 *
 *--------------------------------------------------------------------------*/
PixelFloat::PixelFloat()                                   : r(1.0f), g(0.0f), b(1.0f), a(1.0f) {}
PixelFloat::PixelFloat(float r, float g, float b, float a) : r(r),    g(g),    b(b),    a(a)    {}

void PixelFloat::set(float r, float g, float b, float a)
{
    this->r = r; this->g = g; this->b = b; this->a = a;
}

uint8_t PixelFloat::redByte()   const { return static_cast<uint8_t>(r * 255.0f); }
uint8_t PixelFloat::greenByte() const { return static_cast<uint8_t>(g * 255.0f); }
uint8_t PixelFloat::blueByte()  const { return static_cast<uint8_t>(b * 255.0f); }
uint8_t PixelFloat::alphaByte() const { return static_cast<uint8_t>(a * 255.0f); }

bool PixelFloat::operator==(const PixelFloat &o) const
{
    if(std::abs(r - o.r) > 0.00001f)
        return false;
    if(std::abs(g - o.g) > 0.00001f)
        return false;
    if(std::abs(b - o.b) > 0.00001f)
        return false;
    if(std::abs(a - o.a) > 0.00001f)
        return false;
    return true;
}

bool PixelFloat::operator!=(const PixelFloat &o) const { return !(*this == o); }



/*--------------------------------------------------------------------------*
 *
 * Pixel32
 *
 *--------------------------------------------------------------------------*/

Pixel32::Pixel32()                                   : rgba(0xFF00FFFF) {}
Pixel32::Pixel32(float r, float g, float b, float a) { set(r, g, b, a); }

void Pixel32::set(float r, float g, float b, float a)
{
    uint32_t ir = static_cast<uint32_t>(r*255.0f) << 24;
    uint32_t ig = static_cast<uint32_t>(g*255.0f) << 16;
    uint32_t ib = static_cast<uint32_t>(b*255.0f) <<  8;
    uint32_t ia = static_cast<uint32_t>(a*255.0f) <<  0;

    rgba = (ir & 0xFF000000) | (ig & 0x00FF0000) | (ib & 0x0000FF00) | (ia & 0x000000FF);
}

uint8_t Pixel32::redByte()   const { return (rgba >> 24) & 0xFF; }
uint8_t Pixel32::greenByte() const { return (rgba >> 16) & 0xFF; }
uint8_t Pixel32::blueByte()  const { return (rgba >>  8) & 0xFF; }
uint8_t Pixel32::alphaByte() const { return (rgba >>  0) & 0xFF; }

bool Pixel32::operator==(const Pixel32 &o) const { return rgba == o.rgba; }
bool Pixel32::operator!=(const Pixel32 &o) const { return !(*this == o); }



/*--------------------------------------------------------------------------*
 *
 * PixelData
 *
 *--------------------------------------------------------------------------*/

void PixelData::resize(int width, int height)
{
    pixels.resize(boost::extents[std::max(width, 0)][std::max(height, 0)]);
}

void PixelData::set(int x, int y, float r, float g, float b, float a)
{
    set(x, y, Pixel(r, g, b, a));
}

void PixelData::set(int x, int y, Pixel p)
{
    if(0 <= x && x < width() && 0 <= y && y < height())
        pixels[x][y] = p;
}

void PixelData::fill(float r, float g, float b, float a)
{
    fillRect(0, 0, width()-1, height()-1, Pixel(r, g, b, a));
}

void PixelData::fillRect(int x0, int y0, int x1, int y1, Pixel p)
{
    if(x0 > x1) std::swap(x0, x1);
    if(y0 > y1) std::swap(y0, y1);

    x0 = std::min(std::max(0, x0), width() -1);
    x1 = std::min(std::max(0, x1), width() -1);
    y0 = std::min(std::max(0, y0), height()-1);
    y1 = std::min(std::max(0, y1), height()-1);

    for(int x = x0; x <= x1; x++)
        for(int y = y0; y <= y1; y++)
            pixels[x][y] = p;
}

void PixelData::blit(int px, int py, const PixelData &data)
{
    int x0 = std::max(0, px);
    int y0 = std::max(0, py);

    int x1 = std::min(width(),  x0 + data.width());
    int y1 = std::min(height(), y0 + data.height());

    for(int x = x0; x < x1; x++)
        for(int y = y0; y < y1; y++)
            pixels[x][y] = data.pixels[x-x0][y-y0];
}

Pixel PixelData::get(int x, int y) const
{
    if(0 <= x && x < width() && 0 <= y && y < height())
        return pixels[x][y];
    return Pixel();
}

int PixelData::width()  const { return pixels.shape()[0]; }
int PixelData::height() const { return pixels.shape()[1]; }

uint32_t PixelData::computeChecksum() const
{
    boost::crc_32_type crc;
    crc.process_bytes(pixels.data(), width()*height()*sizeof(Pixel));
    return crc.checksum();
}

bool PixelData::operator==(const PixelData &o) const
{
    if(width() != o.width() || height() != o.height())
        return false;

    for(int x = 0, w = width(); x < w; x++)
        for(int y = 0, h = height(); y < h; y++)
            if(pixels[x][y] != o.pixels[x][y])
                return false;

    return true;
}



/*--------------------------------------------------------------------------*
 *
 * Packed Image
 *
 *--------------------------------------------------------------------------*/
#if 0
Image::Image()
{
    x = y = width = height = 0;
    source_x = source_y = source_width = source_height = 0;
    s0 = s1 = t0 = t1 = 0.0f;
    is_packed = false;
}

bool Image::loadData()
{
    assert(!names.empty());
    return loadImage(names.front(), pixels);
}
#endif




bool Image::initialize(const std::string &name, int extrude)
{
    names.assign(1, name);
    this->extrude = extrude;

    sheet_x = sheet_y = width = height = 0;
    source_x_offset = source_y_offset = source_width = source_height = 0;
    s0 = s1 = t0 = t1 = 0.0f;
    checksum = 0xDEADC0DE;
    is_packed = false;
    has_data = false;

    return createImageData();
}

bool Image::createImageData()
{
    PixelData src_data;

    if(!loadImage(names[0], src_data))
        return false;

    if(src_data.width() == 0 || src_data.height() == 0)
        return false;

    source_x_offset = extrude;
    source_y_offset = extrude;
    source_width    = src_data.width();
    source_height   = src_data.height();
    width           = src_data.width()  + extrude*2;
    height          = src_data.height() + extrude*2;

    pixels.resize(width, height);

    if(extrude > 0)
    {
        int src_x0 = source_x_offset - 1;
        int src_y0 = source_y_offset - 1;
        int src_x1 = source_x_offset + source_width;
        int src_y1 = source_y_offset + source_height;

        int dst_x0 = src_x0 - extrude + 1;
        int dst_y0 = src_y0 - extrude + 1;
        int dst_x1 = src_x1 + extrude - 1;
        int dst_y1 = src_y1 + extrude - 1;

        pixels.fillRect(src_x0, src_y0, dst_x0, dst_y0, src_data.get(0,              0              ));
        pixels.fillRect(src_x1, src_y0, dst_x1, dst_y0, src_data.get(source_width-1, 0              ));
        pixels.fillRect(src_x1, src_y1, dst_x1, dst_y1, src_data.get(source_width-1, source_height-1));
        pixels.fillRect(src_x0, src_y1, dst_x0, dst_y1, src_data.get(0,              source_height-1));

        for(int x = 0; x < source_width; x++)
        {
            pixels.fillRect(extrude+x, src_y0, extrude+x, dst_y0, src_data.get(x, 0              ));
            pixels.fillRect(extrude+x, src_y1, extrude+x, dst_y1, src_data.get(x, source_height-1));
        }

        for(int y = 0; y < source_height; y++)
        {
            pixels.fillRect(src_x0, extrude+y, dst_x0, extrude+y, src_data.get(0,              y));
            pixels.fillRect(src_x1, extrude+y, dst_x1, extrude+y, src_data.get(source_width-1, y));
        }
    }

    pixels.blit(extrude, extrude, src_data);
    checksum = pixels.computeChecksum();
    has_data = true;

    return true;
}

bool Image::recreateImageData()
{
    int prev_w = width, prev_h = height;
    uint32_t prev_checksum = checksum;

    /*
     * checksum should pick up any changes in the image between multiple reads
     * from disk, but size is checked just in case different data produces the
     * same checksum. A different size will lead to undefined behaviour while
     * different image data will just produce incorrect output.
     */
    if(!createImageData() || prev_checksum != checksum || prev_w != width || prev_h != height)
        return fatal(format("failed to reload '%s'. File changed or removed?\n"));
    return true;
}

const PixelData& Image::getPixels()
{
    if(!has_data)
        recreateImageData();
    return pixels;
}

bool Image::equalPixelData(Image &other)
{
    return checksum == other.checksum && getPixels() == other.getPixels();
}

void Image::purgeMemory()
{
    pixels.resize(0, 0);
    has_data = false;
}

void Image::addName(const std::string &name) { names.push_back(name); }


/*--------------------------------------------------------------------------*
 *
 * sheet
 *
 *--------------------------------------------------------------------------*/

Sheet::Sheet(int width, int height)
{
    this->width  = width;
    this->height = height;
    extrude      = 0;
    root         = createNode(0, 0, width, height);
}

bool Sheet::insert(Image *img)
{
    if(insertR(root, img))
    {
        images.push_back(img);
        return true;
    }

    return false;
}

bool Sheet::insertR(Node *node, Image *img)
{
    if(node->left && node->right)
    {
        return insertR(node->left, img) || insertR(node->right, img);
    }
    else
    {
        if(node->img || img->width > node->width || img->height > node->height)
            return false;

        if(img->width == node->width && img->height == node->height)
        {
            img->sheet_x = node->x;
            img->sheet_y = node->y;
            node->img = img;
            return true;
        }

        /* 
         * calculate the remaining width and height in the node. the node is
         * guaranted to be bigger than the image.
         */
        int rw = node->width  - img->width;
        int rh = node->height - img->height;

        if(rw > rh)
        {
            node->left  = createNode(node->x,            node->y, img->width, node->height);
            node->right = createNode(node->x+img->width, node->y, rw,         node->height);
        }
        else
        {
            node->left  = createNode(node->x, node->y,             node->width, img->height);
            node->right = createNode(node->x, node->y+img->height, node->width, rh);
        }

        return insertR(node->left, img);
    }

    return false;
}

void Sheet::blit(PixelData &pixels)
{
    pixels.resize(width, height);
    pixels.fill(0.0f, 0.0f, 0.0f, 0.0f); //TODO fill colour
    blitR(root, pixels);
}

void Sheet::blitR(Node *node, PixelData &pixels)
{
    if(!node) return;
    
    if(node->img)
    {
        bool purge = !node->img->has_data;
        pixels.blit(node->x, node->y, node->img->getPixels());

        if(purge)
            node->img->purgeMemory();
    }
    
    blitR(node->left,  pixels);
    blitR(node->right, pixels);
}

Node* Sheet::createNode(int x, int y, int w, int h)
{
    Node *n = node_pool.construct();
    nodes.push_back(n);
    n->img = NULL; n->x = x; n->y = y; n->width = w; n->height = h;
    return n;
}

bool Sheet::saveImage(boost::filesystem::path &path)
{
    PixelData pixels;
    blit(pixels);
    return Imagepack::saveImage(path, pixels);
}



/*--------------------------------------------------------------------------*
 *
 * Packer
 *
 *--------------------------------------------------------------------------*/

Packer::Packer()
{
    sheet_width = sheet_height = 1024;
    sheets.reserve(32);
    tex_coord_origin = BOTTOM_LEFT;
    extrude = 0;
    compact = false;
    power_of_two = false;
    cache_images = true;
}

void Packer::pack()
{
    print(format("packing %d images\n") % images.size());

    clearSheets();

    for(size_t i = 0, n = images.size(); i < n; i++)
        images[i]->is_packed = false;

    std::vector<Image*> to_pack;
    to_pack.reserve(images.size());

    int last_packed = 0;

    do
    {
        Sheet *s = createSheet(sheet_width, sheet_height);

        to_pack.clear();
        for(size_t i = 0, n = images.size(); i < n; i++)
            if(!images[i]->is_packed)
                to_pack.push_back(images[i]);

        last_packed = packSheet(to_pack, s);

        if(s->images.empty())
            destroySheet(s);

    }while(last_packed > 0);

    if(compact && !sheets.empty())
    {
        to_pack.assign(sheets.back()->images.begin(), sheets.back()->images.end());
        destroySheet(sheets.back());
        packCompactSheet(to_pack, sheet_width, sheet_height);
    }

    computeTexCoords();
    printPackingStats();
}

int Packer::packSheet(std::vector<Image*> &to_pack, Sheet *s)
{
    int num_packed = 0;

    std::stable_sort(to_pack.begin(), to_pack.end(), imageHeightCompare);
    std::stable_sort(to_pack.begin(), to_pack.end(), imageWidthCompare);

    for(size_t i = 0, n = to_pack.size(); i < n; i++)
    {
        if(s->insert(to_pack[i]))
        {
            to_pack[i]->is_packed = true;
            num_packed++;
        }
        else
            to_pack[i]->is_packed = false;
    }

    return num_packed;
}

void Packer::packCompactSheet(std::vector<Image*> &to_pack, int max_width, int max_height)
{
    int sizes[2]     = {1, 1};
    int max_sizes[2] = {max_width, max_height};
    int size_index   = 0;
    int packed       = 0;

    /*
     * Set the initial size to be large enough to hold the smallest sprite.
     * This ensures that all sprices can be placed by increasing just width or
     * height.
     */
    for(size_t i = 0; i < to_pack.size(); i++)
    {
        sizes[0] = std::max(sizes[0], to_pack[i]->width);
        sizes[1] = std::max(sizes[1], to_pack[i]->height);
    }

    do
    {
        Sheet s(sizes[0], sizes[1]);
        packed = packSheet(to_pack, &s);

        if(packed != 0)
            size_index = (size_index + 1) % 2;

        if(sizes[size_index] == max_sizes[size_index])
            size_index = (size_index + 1) % 2;

        if(packed != (int)to_pack.size())
            sizes[size_index]++;

        /*
         * this shouldn't happen when when max_width and max_height have been
         * obtained from a previous sheet with the same packed images.
         */
        if(sizes[0] > max_sizes[0] && sizes[1] > max_sizes[1])
        {
            print("failed to fit all sprites in a compact sheet\n", VERBOSE);
            break;
        }
    } while(packed != (int)to_pack.size());

    /*
     * sizes can be > max_size if all images can't be packed into an image of
     * max_size.
     */
    sizes[0] = std::min(sizes[0], max_sizes[0]);
    sizes[1] = std::min(sizes[1], max_sizes[1]);

    if(power_of_two)
    {
        sizes[0] = nextPowerOfTwo(sizes[0]);
        sizes[1] = nextPowerOfTwo(sizes[1]);
    }

    /*
     * The minimum sheet size to fit all images is known so calling pack sheet
     * will pack every image successfully.
     */
    packSheet(to_pack, createSheet(sizes[0], sizes[1]));
}

void Packer::computeTexCoords()
{
    for(size_t i = 0, n = sheets.size(); i < n; i++)
        for(size_t j = 0, m = sheets[i]->images.size(); j < m; j++)
        {
            Image *img = sheets[i]->images[j];
            int x = img->sheet_x + img->source_x_offset;
            int y = img->sheet_y + img->source_y_offset;
            int w = img->source_width;
            int h = img->source_height;

            if(tex_coord_origin == BOTTOM_LEFT)
                y = sheets[i]->height - y - h;

            img->s0 = (x     + 0.5f) / sheets[i]->width;
            img->s1 = (x + w - 0.5f) / sheets[i]->width;
            img->t0 = (y     + 0.5f) / sheets[i]->height;
            img->t1 = (y + h - 0.5f) / sheets[i]->height;
        }
}

void Packer::printPackingStats()
{
    int unpacked = 0;
    for(size_t i = 0, n = images.size(); i < n; i++)
        if(!images[i]->is_packed)
        {
            unpacked++;
            print(format("unabled to pack '%s'\n") % images[i]->names[0]);
        }

    print(format("packed %d/%d images into %d sheets\n") % (images.size() - unpacked) % images.size() % sheets.size());
}

void Packer::addImage(const std::string &name)
{
    print(format("adding %s\n") % name, VERBOSE);

    for(size_t i = 0, n = images.size(); i < n; i++)
        if(std::find(images[i]->names.begin(), images[i]->names.end(), name) != images[i]->names.end())
        {
            print(format("image '%s' already added\n") % name);
            return;
        }

    Image *img = image_pool.construct();

    if(!img->initialize(name, extrude))
    {
        image_pool.destroy(img);
        return;
    }

    Image *duplicate_of = NULL;
    for(size_t i = 0, n = images.size(); i < n && !duplicate_of; i++)
    {
        if(img->equalPixelData(*images[i]))
            duplicate_of = images[i];

        if(!cache_images)
            images[i]->purgeMemory();
    }

    if(duplicate_of)
    {
        print(format("duplicate image data ['%s' == '%s']\n") % name % duplicate_of->names[0], VERBOSE);
        duplicate_of->addName(name);
        image_pool.destroy(img);
    }
    else
    {
        if(!cache_images)
            img->purgeMemory();

        images.push_back(img);
    }
}

int Packer::numImages()
{
    return images.size();
}

void Packer::setSheetSize(int width, int height)
{
    sheet_width  = std::max(1, width);
    sheet_height = std::max(1, height);

    if(power_of_two)
    {
        sheet_width  = nextPowerOfTwo(sheet_width);
        sheet_height = nextPowerOfTwo(sheet_height);
    }
}

void Packer::setPowerOfTwo(bool value)
{
    power_of_two = value;
    setSheetSize(sheet_width, sheet_height);
}

void Packer::setCompact(bool value)
{
    compact = value;
}

void Packer::setTexCoordOrigin(int origin)
{
    if(!(origin == BOTTOM_LEFT || origin == TOP_LEFT))
        return;
    this->tex_coord_origin = origin;
}

void Packer::setExtrude(int extrude)
{
    this->extrude = std::max(0, extrude);
}

void Packer::setCaching(bool cache)
{
    cache_images = cache;

    if(!cache_images)
    {
        for(size_t i = 0; i < images.size(); i++)
            images[i]->purgeMemory();
    }
}

int Packer::numSheets()
{
    return (int)sheets.size();
}

Sheet* Packer::getSheet(int index)
{
    if(0 <= index && index < (int)sheets.size())
        return sheets[index];
    return NULL;
}

Sheet* Packer::createSheet(int width, int height)
{
    Sheet *s   = sheet_pool.construct(width, height);
    s->extrude = extrude;
    sheets.push_back(s);

    return s;
}

void Packer::destroySheet(Sheet *s)
{
    if(!s) return;

    sheets.erase(std::remove(sheets.begin(), sheets.end(), s), sheets.end());
    sheet_pool.destroy(s);
}

void Packer::clearSheets()
{
    for(size_t i = 0, n = sheets.size(); i < n; i++)
        sheet_pool.destroy(sheets[i]);
    sheets.clear();
}

void Packer::clearImages()
{
    for(size_t i = 0, n = images.size(); i < n; i++)
        image_pool.destroy(images[i]);
    images.clear();
}

unsigned int Packer::nextPowerOfTwo(unsigned int n) const
{
    n -= 1;
    for(unsigned int i = 1; i < sizeof(unsigned int) * 8; i++)
        n |= n >> i;
    return n + 1;
}

