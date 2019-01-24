#include "tile.h"

#include <stdio.h>
#include <memory.h>
#include <algorithm>

#ifdef USE_TILE
 #include <png.h>
#endif

#include <assert.h>

tile::tile() : m_width(0), m_height(0), m_pixels(nullptr), m_shrink(true)
{
}

tile::tile(const tile &img, const char *enumnam, const char *parts) :
           m_width(0), m_height(0), m_pixels(nullptr)
{
    copy(img);

    if (enumnam)
        m_enumname.push_back(enumnam);
    if (parts)
        m_parts_ctg = parts;

    for (int i = 0; i < MAX_COLOUR; ++i)
        m_variations[i] = -1;
}

tile::~tile()
{
    unload();
}

void tile::unload()
{
    delete[] m_pixels;
    m_pixels = nullptr;
    m_width  = m_height = 0;
}

bool tile::valid() const
{
#ifdef USE_TILE
    return m_pixels && m_width && m_height;
#else
    return m_pixels && !m_width && !m_height;
#endif
}

const string &tile::filename() const
{
    return m_filename;
}

int tile::enumcount() const
{
    return m_enumname.size();
}

const string &tile::enumname(int idx) const
{
    return m_enumname[idx];
}

void tile::add_enumname(const string &name)
{
    m_enumname.push_back(name);
}

const string &tile::parts_ctg() const
{
    return m_parts_ctg;
}

int tile::width() const
{
    return m_width;
}

int tile::height() const
{
    return m_height;
}

bool tile::shrink()
{
    return m_shrink;
}

void tile::set_shrink(bool new_shrink)
{
    m_shrink = new_shrink;
}

void tile::resize(int new_width, int new_height)
{
    delete[] m_pixels;
    m_width  = new_width;
    m_height = new_height;

    m_pixels = nullptr;

    if (!m_width || !m_height)
        return;

    m_pixels = new tile_colour[m_width * m_height];
}

void tile::add_rim(const tile_colour &rim)
{
    bool *flags = new bool[m_width * m_height];
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            flags[x + y * m_width] =
                (get_pixel(x, y).a > 0 && get_pixel(x,y) != rim);
        }

    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            if (flags[x + y * m_width])
                continue;

            if (x > 0 && flags[(x-1) + y * m_width]
                || y > 0 && flags[x + (y-1) * m_width]
                || x < m_width - 1 && flags[(x+1) + y * m_width]
                || y < m_height - 1 && flags[x + (y+1) * m_width])
            {
                get_pixel(x,y) = rim;
            }
        }

    delete[] flags;
}

void tile::corpsify()
{
    // TODO enne - different wound colours for different bloods
    // TODO enne - use blood variations
    tile_colour red_blood(0, 0, 32, 255);

    const int separate_x = 3;
    const int separate_y = 4;

    // Force all corpses into 32x32, even if bigger.
    corpsify(32, 32, separate_x, separate_y, red_blood);
}

static int _corpse_cut_height(int x, int width, int height)
{
    unsigned int cy = height / 2 + 2;

    // Make the cut bend upwards in the middle
    const int limit1 = width / 8;
    const int limit2 = width / 3;

    if (x < limit1 || x >= width - limit1)
        cy += 2;
    else if (x < limit2 || x >= width - limit2)
        cy += 1;

    return cy;
}

// Adapted from rltiles' cp_monst_32 and then ruthlessly rewritten for clarity.
// rltiles can be found at http://rltiles.sourceforge.net
void tile::corpsify(int corpse_width, int corpse_height,
                    int cut_separate, int cut_height, const tile_colour &wound)
{
    // Make a temporary backup
    tile orig(*this);

    resize(corpse_width, corpse_height);
    fill(tile_colour::transparent);

    // Track which pixels have been written to with valid image data
    bool *flags = new bool[corpse_width * corpse_height];
    memset(flags, 0, corpse_width * corpse_height * sizeof(bool));

#define flags(x,y) (flags[((x) + (y) * corpse_width)])

    // Find extents
    int xmin, ymin, bbwidth, bbheight;
    orig.get_bounding_box(xmin, ymin, bbwidth, bbheight);

    const int xmax = xmin + bbwidth - 1;
    const int ymax = ymin + bbheight - 1;
    const int centerx = (xmax + xmin) / 2;
    const int centery = (ymax + ymin) / 2;

    // Use maximum scale in case aspect ratios differ.
    const float width_scale  = (float)m_width / (float)corpse_width;
    const float height_scale = (float)m_height / (float)corpse_height;
    const float image_scale  = max(width_scale, height_scale);

    // Amount to scale height by to fake a projection.
    const float height_proj = 2.0f;

    for (int y = 0; y < corpse_height; y++)
        for (int x = 0; x < corpse_width; x++)
        {
            const int cy = _corpse_cut_height(x, corpse_width, corpse_height);
            if (y > cy - cut_height && y <= cy)
                continue;

            // map new center to old center, including image scale
            int x1 = (int)((x - m_width/2)*image_scale) + centerx;
            int y1 = (int)((y - m_height/2)*height_proj*image_scale) + centery;

            if (y >= cy)
            {
                x1 -= cut_separate;
                y1 -= cut_height / 2;
            }
            else
            {
                x1 += cut_separate;
                y1 += cut_height / 2 + cut_height % 2;
            }

            if (x1 < 0 || x1 >= m_width || y1 < 0 || y1 >= m_height)
                continue;

            tile_colour &mapped = orig.get_pixel(x1, y1);

            // Ignore rims, shadows, and transparent pixels.
            if (mapped == tile_colour::black
                || mapped.is_transparent())
            {
                continue;
            }

            get_pixel(x,y) = mapped;
            flags(x, y) = true;
        }

    const int wound_height = min(2, cut_height);

    // Add some colour to the cut wound.
    for (int x = 0; x < corpse_width; x++)
    {
        int cy = _corpse_cut_height(x, corpse_width, corpse_height);
        if (flags(x, cy - cut_height))
        {
            const int start = cy - cut_height + 1;
            for (int y = start; y < start + wound_height; y++)
                get_pixel(x, y) = wound;
        }
    }

    // Add diagonal shadowing...
    for (int y = 1; y < corpse_height; y++)
        for (int x = 1; x < corpse_width; x++)
        {
            if (!flags(x, y) && flags(x-1, y-1)
                && get_pixel(x,y).is_transparent())
            {
                get_pixel(x, y) = tile_colour::black;
            }
        }

    // Extend shadow...
    for (int y = 3; y < corpse_height; y++)
        for (int x = 3; x < corpse_width; x++)
        {
            // Extend shadow if there are two real pixels along
            // the diagonal. Also, don't extend if the top or
            // left pixel is not filled in. This prevents lone
            // shadow pixels only connected via diagonals.

            if (get_pixel(x-1,y-1) == tile_colour::black
                && flags(x-2, y-2) && flags(x-3, y-3)
                && get_pixel(x-1, y) == tile_colour::black
                && get_pixel(x, y-1) == tile_colour::black)
            {
                get_pixel(x, y) = tile_colour::black;
            }
        }

    delete[] flags;
}

void tile::copy(const tile &img)
{
    unload();

    m_width    = img.m_width;
    m_height   = img.m_height;
    m_filename = img.m_filename;
    m_pixels   = new tile_colour[m_width * m_height];
    m_shrink   = img.m_shrink;
    memcpy(m_pixels, img.m_pixels, m_width * m_height * sizeof(tile_colour));

    // enum explicitly not copied
    m_enumname.clear();
}

bool tile::compose(const tile &img)
{
#ifdef USE_TILE
    if (!valid())
    {
        fprintf(stderr, "Error: can't compose onto an unloaded image.\n");
        return false;
    }

    if (!img.valid())
    {
        fprintf(stderr, "Error: can't compose from an unloaded image.\n");
        return false;
    }

    if (m_width != img.m_width || m_height != img.m_height)
    {
        fprintf(stderr, "Error: can't compose with mismatched dimensions. "
                        "(%d, %d) onto (%d, %d)\n",
                img.m_width, img.m_height, m_width, m_height);
        return false;
    }

    for (int i = 0; i < m_width * m_height; i += 1)
    {
        const tile_colour *src = &img.m_pixels[i];
        tile_colour *dest = &m_pixels[i];

        dest->r = (src->r * src->a + dest->r * (255 - src->a)) / 255;
        dest->g = (src->g * src->a + dest->g * (255 - src->a)) / 255;
        dest->b = (src->b * src->a + dest->b * (255 - src->a)) / 255;
        dest->a = (src->a * 255    + dest->a * (255 - src->a)) / 255;
    }
#endif

    return true;
}

bool tile::texture(const tile &img)
{
    if (!valid())
    {
        fprintf(stderr, "Error: can't texture onto an unloaded image.\n");
        return false;
    }

    if (!img.valid())
    {
        fprintf(stderr, "Error: can't texture from an unloaded image.\n");
        return false;
    }

    if (m_width != img.m_width || m_height != img.m_height)
    {
        fprintf(stderr, "Error: can't texture with mismatched dimensions. "
                        "(%d, %d) onto (%d, %d)\n",
                img.m_width, img.m_height, m_width, m_height);
        return false;
    }

    for (int i = 0; i < m_width * m_height; i += 1)
    {
        const tile_colour *src = &img.m_pixels[i];
        tile_colour *dest = &m_pixels[i];

        if (dest->r || dest->g || dest->b)
            dest->r = src->r, dest->g = src->g, dest->b = src->b;
        // alpha is unchanged
    }

    return true;
}

bool tile::load(const string &new_filename)
{
    m_filename = new_filename;

    if (m_pixels)
        unload();

#ifdef USE_TILE
    FILE* fp = fopen(new_filename.c_str(), "rb");
    if (!fp)
        return false;

    // Read and check PNG signature
    const unsigned sig_bytes = 8;
    png_byte sig[sig_bytes];
    if (fread(sig, 1, sig_bytes, fp) < sig_bytes
        || png_sig_cmp(sig, 0, sig_bytes))
    {
        fclose(fp);
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 nullptr, nullptr, nullptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    // libpng error handling!
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);

    // let libpng know we already read the first 8 bytes
    png_set_sig_bytes(png_ptr, sig_bytes);

    png_read_info(png_ptr, info_ptr);

    // get some info
    int bit_depth, color_type;
    png_uint_32 w, h;

    png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
                 nullptr, nullptr, nullptr);

    // enable various transformations to get 8-bit RGBA pixels
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0)
        png_set_add_alpha(png_ptr, 255, PNG_FILLER_AFTER);

    png_read_update_info(png_ptr, info_ptr);

    m_width = (int) w;
    m_height = (int) h;

    m_pixels = new tile_colour[m_width * m_height];

    png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    assert(rowbytes == m_width * sizeof (tile_colour));

    // init row pointer buffer

    // Note that with the transformations above and because the color
    // channels in PNG files are always ordered RGBA, we get the
    // pixels in exactly the format tile_colour expects them in, so we
    // can just cast the tile_colour* to png_bytep and everything
    // works; but it's admittedly a bit dangerous.
    png_bytep *row_pointers = new png_bytep[h];

    for (png_uint_32 i = 0; i < h; ++i)
        row_pointers[i] = ((png_bytep) m_pixels) + i * rowbytes;

    // and read it
    png_read_image(png_ptr, row_pointers);

    delete[] row_pointers;

    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    replace_colour(tile_colour::background, tile_colour::transparent);
#else
    FILE* fp = fopen(new_filename.c_str(), "rb");
    if (!fp)
        return false;
    fclose(fp);

    m_width  = 0;
    m_height = 0;
    m_pixels = new tile_colour[1];
#endif

    return true;
}

void tile::fill(const tile_colour &col)
{
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
            get_pixel(x, y) = col;
}

void tile::replace_colour(tile_colour &find, tile_colour &replace)
{
    for (int y = 0; y < m_height; y++)
        for (int x = 0; x < m_width; x++)
        {
            tile_colour &p = get_pixel(x, y);
            if (p == find)
                p = replace;
        }
}

tile_colour &tile::get_pixel(int x, int y)
{
#ifdef USE_TILE
    assert(m_pixels && x < m_width && y < m_height);
    return m_pixels[x + y * m_width];
#else
    static tile_colour dummy;
    return dummy;
#endif
}

void tile::get_bounding_box(int &x0, int &y0, int &w, int &h)
{
    if (!valid())
    {
        x0 = y0 = w = h = 0;
        return;
    }

    x0 = y0 = 0;
    int x1 = m_width - 1;
    int y1 = m_height - 1;
    while (x0 <= x1)
    {
        bool found = false;
        for (int y = y0; !found && y < y1; y++)
            found |= (get_pixel(x0, y).a > 0);

        if (found)
            break;
        x0++;
    }

    while (x0 <= x1)
    {
        bool found = false;
        for (int y = y0; !found && y < y1; y++)
            found |= (get_pixel(x1, y).a > 0);

        if (found)
            break;
        x1--;
    }

    while (y0 <= y1)
    {
        bool found = false;
        for (int x = x0; !found && x < x1; x++)
            found |= (get_pixel(x, y0).a > 0);

        if (found)
            break;
        y0++;
    }

    while (y0 <= y1)
    {
        bool found = false;
        for (int x = x0; !found && x < x1; x++)
            found |= (get_pixel(x, y1).a > 0);

        if (found)
            break;
        y1--;
    }

    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
}

void tile::add_variation(int colour, int idx)
{
    assert(colour >= 0);
    assert(colour < MAX_COLOUR);
    m_variations[colour] = idx;
}

bool tile::get_variation(int colour, int &idx)
{
    if (m_variations[colour] == -1)
        return false;

    idx = m_variations[colour];
    return true;
}
