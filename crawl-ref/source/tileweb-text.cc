
#include "AppHdr.h"

#ifdef USE_TILE_WEB

#include "tileweb-text.h"

#include "stringutil.h"
#include "tileweb.h"
#include "unicode.h"

WebTextArea::WebTextArea(string name) :
    mx(0),
    my(0),
    m_cbuf(nullptr),
    m_abuf(nullptr),
    m_old_cbuf(nullptr),
    m_old_abuf(nullptr),
    m_client_side_name(name),
    m_dirty(true)
{
}

WebTextArea::~WebTextArea()
{
    if (m_cbuf != nullptr)
    {
        delete[] m_cbuf;
        delete[] m_abuf;
        delete[] m_old_cbuf;
        delete[] m_old_abuf;
    }
}

void WebTextArea::resize(int x, int y)
{
    if (mx == x && my == y)
        return;

    mx = x;
    my = y;

    if (m_cbuf != nullptr)
    {
        delete[] m_cbuf;
        delete[] m_abuf;
        delete[] m_old_cbuf;
        delete[] m_old_abuf;
    }

    int size = mx * my;
    m_cbuf = new char32_t[size];
    m_abuf = new uint8_t[size];
    m_old_cbuf = new char32_t[size];
    m_old_abuf = new uint8_t[size];

    for (int i = 0; i < mx * my; i++)
    {
        m_cbuf[i] = ' ';
        m_abuf[i] = 0;
        m_old_cbuf[i] = ' ';
        m_old_abuf[i] = 0;
    }

    m_dirty = true;

    on_resize();
}

void WebTextArea::clear()
{
    for (int i = 0; i < mx * my; i++)
    {
        m_cbuf[i] = ' ';
        m_abuf[i] = 0;
    }

    m_dirty = true;
}

void WebTextArea::put_character(char32_t chr, int fg, int bg, int x, int y)
{
    ASSERT_RANGE(x, 0, mx);
    ASSERT_RANGE(y, 0, my);
    uint8_t col = (fg & 0xf) + (bg << 4);

    if (m_cbuf[x + y * mx] != chr || m_abuf[x + y * mx] != col)
        m_dirty = true;

    m_cbuf[x + y * mx] = chr;
    m_abuf[x + y * mx] = col;
}

void WebTextArea::send(bool force)
{
    if (m_cbuf == nullptr) return;
    if (!force && !m_dirty) return;
    m_dirty = false;

    int last_col = -1;
    int space_count = 0;
    bool dirty = false;
    string html;

    bool sending = false;

    for (int y = 0; y < my; ++y)
    {
        last_col = -1;
        space_count = 0;
        dirty = false;
        html.clear();

        for (int x = 0; x < mx; ++x)
        {
            char32_t chr = m_cbuf[x + y * mx];
            uint8_t col = m_abuf[x + y * mx];

            if (chr != m_old_cbuf[x + y * mx] ||
                col != m_old_abuf[x + y * mx])
            {
                dirty = true;
                m_old_cbuf[x + y * mx] = chr;
                m_old_abuf[x + y * mx] = col;
            }

            if (chr != ' ' || ((col >> 4) & 0xF) != 0)
            {
                while (space_count)
                {
                    html.push_back(' ');
                    space_count--;
                }
            }

            if (col != last_col && chr != ' ')
            {
                if (last_col != -1)
                    html += "</span>";
                html += make_stringf("<span class=\\\"fg%d bg%d\\\">",
                                     col & 0xf, (col >> 4) & 0xf);
                last_col = col;
            }

            if (chr == ' ' && ((col >> 4) & 0xF) == 0)
                space_count++;
            else
            {
                switch (chr)
                {
                case '<':
                    html += "&lt;";
                    break;
                case '>':
                    html += "&gt;";
                    break;
                case '&':
                    html += "&amp;";
                    break;
                case '\\':
                    html += "\\\\";
                    break;
                case '"':
                    html += "&quot;";
                    break;
                default:
                    char buf[5];
                    buf[wctoutf8(buf, chr)] = 0;
                    html += buf;
                    break;
                }
            }
        }

        if (dirty || (force && !html.empty()))
        {
            if (!sending)
            {
                tiles.write_message("{\"msg\":\"txt\",\"id\":\"%s\"",
                                    m_client_side_name.c_str());
                if (force)
                    tiles.write_message(",\"clear\":true");
                tiles.write_message(",\"lines\":{");
                sending = true;
            }

            tiles.json_write_comma();
            tiles.write_message("\"%u\":\"%s\"", y, html.c_str());
        }
    }
    if (sending)
    {
        tiles.write_message("}}");
        tiles.finish_message();
    }
}

void WebTextArea::on_resize()
{
}
#endif
