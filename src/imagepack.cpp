#include <cstdio>
#include <algorithm>
#include <boost/crc.hpp>
#include "imagepack.h"

using boost::format;
using namespace Imagepack;


static bool imageHeightCompare(Image *a, Image *b)
{
    return a->height > b->height;
}

static bool imageWidthCompare(Image *a, Image *b)
{
    return a->width > b->width;
}



Pixel::Pixel()                                   : r(1.0f), g(0.0f), b(1.0f), a(1.0f) {}
Pixel::Pixel(float r, float g, float b, float a) : r(r),    g(g),    b(b),    a(a)    {}

PixelData::PixelData()
{
    checksum_dirty = true;
    checksum = 0xDEADC0DE;
}


void PixelData::resize(int width, int height)
{
    pixels.resize(boost::extents[std::max(width, 0)][std::max(height, 0)]);
    checksum_dirty = true;
}

void PixelData::set(int x, int y, float r, float g, float b, float a)
{
    if(0 <= x && x < width() && 0 <= y && y < height())
    {
        pixels[x][y] = Pixel(r, g, b, a);
        checksum_dirty = true;
    }
}

void PixelData::set(int x, int y, Pixel p)
{
    set(x, y, p.r, p.g, p.b, p.a);
}

void PixelData::fill(float r, float g, float b, float a)
{
    for(int x = 0, w = width(); x < w; x++)
        for(int y = 0, h = height(); y < h; y++)
            pixels[x][y] = Pixel(r, g, b, a);
    checksum_dirty = true;
}

Pixel PixelData::get(int x, int y) const
{
    if(0 <= x && x < width() && 0 <= y && y < height())
        return pixels[x][y];
    return Pixel();
}

int PixelData::width()  const { return pixels.shape()[0]; }
int PixelData::height() const { return pixels.shape()[1]; }

boost::uint32_t PixelData::getChecksum() const
{
    if(checksum_dirty)
    {
        boost::crc_32_type crc;
        crc.process_bytes(pixels.data(), width()*height()*sizeof(Pixel));
        checksum = crc.checksum();
        checksum_dirty = false;
    }

    return checksum;
}

bool PixelData::operator==(const PixelData &o) const
{
    if(width() != o.width() || height() != o.height())
        return false;

    /*
     * FIXME I'm not sure checksums help in the general case. Most images are
     * likely to be different so the pixel comparison will return false early
     * enough.
     */
    //if(getChecksum() != o.getChecksum())
    //    return false;

    for(int x = 0, w = width(); x < w; x++)
        for(int y = 0, h = height(); y < h; y++)
        {
            if(std::abs(pixels[x][y].r - o.pixels[x][y].r) > 0.00001f)
                return false;
            if(std::abs(pixels[x][y].g - o.pixels[x][y].g) > 0.00001f)
                return false;
            if(std::abs(pixels[x][y].b - o.pixels[x][y].b) > 0.00001f)
                return false;
            if(std::abs(pixels[x][y].a - o.pixels[x][y].a) > 0.00001f)
                return false;
        }

    return true;
}

Image::Image()
{
    x = y = width = height = 0;
    source_x = source_y = source_width = source_height = 0;
    s0 = s1 = t0 = t1 = 0.0f;
    is_packed = false;
}


Packer::Packer()
{
    sheet_width = sheet_height = 1024;
    sheets.reserve(32);
    tex_coord_origin = BOTTOM_LEFT;
    compact = false;
    power_of_two = false;
}

void Packer::pack()
{
    print(format("packing %d images\n") % images.size());

    clearSheets();
    clearNodes();

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
            if(!images[i]->is_packed && validImageSize(s, images[i]))
                to_pack.push_back(images[i]);

        last_packed = packSheet(to_pack, s);

        if(s->images.empty())
            destroySheet(s);

    }while(last_packed > 0);

    if(compact && !sheets.empty())
    {
        Sheet *s = sheets.back();
        to_pack.assign(s->images.begin(), s->images.end());
        destroySheet(s);
        packCompactSheet(to_pack, sheet_width, sheet_height);
    }

    computeTexCoords();
    printPackingStats();
    blitSheets();
}

int Packer::packSheet(std::vector<Image*> &to_pack, Sheet *s)
{
    int num_packed = 0;

    std::stable_sort(to_pack.begin(), to_pack.end(), imageHeightCompare);
    std::stable_sort(to_pack.begin(), to_pack.end(), imageWidthCompare);

    for(size_t i = 0, n = to_pack.size(); i < n; i++)
    {
        if(insertR(s->root, to_pack[i]))
        {
            to_pack[i]->is_packed = true;
            s->images.push_back(to_pack[i]);
            num_packed++;
        }
        else
            to_pack[i]->is_packed = false;
    }

    return num_packed;
}

Sheet* Packer::packCompactSheet(std::vector<Image*> &to_pack, int max_width, int max_height)
{
    Sheet *s         = NULL;
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
        destroySheet(s);
        s = createSheet(sizes[0], sizes[1]);
        packed = packSheet(to_pack, s);

        if(packed != 0)
            size_index = (size_index + 1) % 2;

        if(sizes[size_index] == max_sizes[size_index])
            size_index = (size_index + 1) % 2;

        /*
         * this shouldn't happen when when max_width and max_height have been
         * obtained from a previous sheet with the same packed images.
         */
        if(sizes[0] >= max_sizes[0] && sizes[1] >= max_sizes[1])
        {
            printf("failed to fit all sprites in a compact sheet\n");
            break;
        }

        sizes[size_index]++;
    } while(packed != (int)to_pack.size());

    /*
     * By increasing width and height at the same time lots of free space
     * could be created on the right or bottom of the sheet.  This can be
     * eliminated by finding the maximum extents of the packed images.
     */
    int max_x = 0, max_y = 0;
    for(size_t i = 0; i < to_pack.size(); i++)
    {
        int ix = to_pack[i]->x + to_pack[i]->width;
        int iy = to_pack[i]->y + to_pack[i]->height;
        max_x = std::max(max_x, ix);
        max_y = std::max(max_y, iy);
    }

    if(power_of_two)
    {
        max_x = nextPowerOfTwo(max_x);
        max_y = nextPowerOfTwo(max_y);
    }

    /*
     * If the sheet sizes were to be modified all node and image rectangles
     * would have to be updated.  Easier to just pack a new sheet and know
     * everything will fit.
     */
    destroySheet(s);
    s = createSheet(max_x, max_y);
    packSheet(to_pack, s);

    return s;
}

void Packer::computeTexCoords()
{
    for(size_t i = 0, n = sheets.size(); i < n; i++)
        for(size_t j = 0, m = sheets[i]->images.size(); j < m; j++)
        {
            Image *img = sheets[i]->images[j];
            int x = img->source_x, y = img->source_y, w = img->source_width, h = img->source_height;

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

bool Packer::validImageSize(Sheet *s, Image *img)
{
    /*
     * addImage discards images that are too small, so just need to check the
     * height.
     */
    return img->width <= s->width && img->height <= s->height;
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
    img->names.push_back(name);

    if(!loadPixels(name, img->pixels))
    {
        image_pool.destroy(img);
        return;
    }

    img->source_width  = img->pixels.width();
    img->source_height = img->pixels.height();
    img->width         = img->pixels.width();
    img->height        = img->pixels.height();

    if(!(img->width > 1 && img->height > 1))
    {
        print("image is too small. requires width > 1 && height > 1\n");
        image_pool.destroy(img);
        return;
    }

    Image *duplicate_of = NULL;
    for(size_t i = 0, n = images.size(); i < n && !duplicate_of; i++)
        if(img->pixels == images[i]->pixels)
            duplicate_of = images[i];

    if(duplicate_of)
    {
        print(format("duplicate image data ['%s' == '%s']\n") % name % duplicate_of->names[0], VERBOSE);
        duplicate_of->names.push_back(name);
        image_pool.destroy(img);
    }
    else
    {
        images.push_back(img);
    }
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

bool Packer::insertR(Node *node, Image *img)
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
            img->x = node->x;
            img->y = node->y;
            img->source_x = node->x; //TODO offset when borders implemented
            img->source_y = node->y;
            node->img = img;
            return true;
        }

        /* 
         * calculate the remaining width and height in the node the node is
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

void Packer::blitSheets()
{
    for(size_t i = 0, n = sheets.size(); i < n; i++)
    {
        Sheet *s = sheets[i];
        s->pixels.resize(s->width, s->height);
        s->pixels.fill(0.0f, 0.0f, 0.0f, 0.0f); //TODO fill colour
        blitSheetR(s->root, s);
    }
}

void Packer::blitSheetR(Node *node, Sheet *sheet)
{
    if(!node) return;
    
    if(node->img)
    {
        for(int x = 0; x < node->width; x++)
            for(int y = 0; y < node->height; y++)
                sheet->pixels.set(node->x + x, node->y + y, node->img->pixels.get(x, y));
    }
    
    blitSheetR(node->left, sheet);
    blitSheetR(node->right, sheet);
}

Node* Packer::createNode(int x, int y, int w, int h)
{
    Node *n = node_pool.construct();
    nodes.push_back(n);
    n->img = NULL; n->x = x; n->y = y; n->width = w; n->height = h;
    return n;
}

Sheet* Packer::createSheet(int width, int height)
{
    Sheet *s = sheet_pool.construct();
    sheets.push_back(s);

    s->width  = width;
    s->height = height;
    s->root   = createNode(0, 0, width, height);

    return s;
}

void Packer::destroySheet(Sheet *s)
{
    if(!s) return;

    for(size_t i = 0; i < sheets.size(); i++)
        if(sheets[i] == s)
        {
            sheets.erase(sheets.begin() + i);
            break;
        }

    sheet_pool.destroy(s);
}

void Packer::clearNodes()
{
    for(size_t i = 0, n = nodes.size(); i < n; i++)
        node_pool.destroy(nodes[i]);
    nodes.clear();
}

void Packer::clearImages()
{
    for(size_t i = 0, n = images.size(); i < n; i++)
        image_pool.destroy(images[i]);
    images.clear();
}

void Packer::clearSheets()
{
    for(size_t i = 0, n = sheets.size(); i < n; i++)
        sheet_pool.destroy(sheets[i]);
    sheets.clear();
}
void Packer::print(const char *str, int)
{
    printf("%s", str);
}

unsigned int Packer::nextPowerOfTwo(unsigned int n) const
{
    n -= 1;
    for(unsigned int i = 1; i < sizeof(unsigned int) * 8; i++)
        n |= n >> i;
    return n + 1;
}

void Packer::print(const boost::format &fmt, int type)
{
    print(boost::str(fmt).c_str(), type);
}

