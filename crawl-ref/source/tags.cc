/**
 * @file
 * @brief Auxiliary functions to make savefile versioning simpler.
**/

/*
   The marshalling and unmarshalling of data is done in big endian and
   is meant to keep savefiles cross-platform. Note also that the marshalling
   sizes are 1, 2, and 4 for byte, short, and int. If a strange platform
   with different sizes of these basic types pops up, please sed it to fixed-
   width ones. For now, that wasn't done in order to keep things convenient.
*/

#include "AppHdr.h"

#include "tags.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>
#ifdef UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "abyss.h"
#include "act-iter.h"
#include "artefact.h"
#include "art-enum.h"
#include "branch.h"
#include "butcher.h"
#include "colour.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "dbg-util.h"
#include "describe.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "end.h"
#include "errors.h"
#include "ghost.h"
#include "god-abil.h" // just for the Ru sac penalty key
#include "god-companions.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-type-id-state-type.h"
#include "items.h"
#include "jobs.h"
#include "mapmark.h"
#include "misc.h"
#include "mon-death.h"
#include "place.h"
#include "player-stats.h"
#include "prompt.h" // index_to_letter
#include "religion.h"
#include "skills.h"
#include "species.h"
#include "spl-wpnench.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tilepick.h"
#include "tileview.h"
#ifdef USE_TILE
 #include "tilemcache.h"
#endif
#include "transform.h"
#include "unwind.h"
#include "version.h"

vector<ghost_demon> global_ghosts; // only for reading/writing

// defined in dgn-overview.cc
extern map<branch_type, set<level_id> > stair_level;
extern map<level_pos, shop_type> shops_present;
extern map<level_pos, god_type> altars_present;
extern map<level_pos, branch_type> portals_present;
extern map<level_pos, string> portal_notes;
extern map<level_id, string> level_annotations;
extern map<level_id, string> level_exclusions;
extern map<level_id, string> level_uniques;
extern set<pair<string, level_id> > auto_unique_annotations;

// defined in abyss.cc
extern abyss_state abyssal_state;

reader::reader(const string &_read_filename, int minorVersion)
    : _filename(_read_filename), _chunk(0), _pbuf(nullptr), _read_offset(0),
      _minorVersion(minorVersion), _safe_read(false)
{
    _file       = fopen_u(_filename.c_str(), "rb");
    opened_file = !!_file;
}

reader::reader(package *save, const string &chunkname, int minorVersion)
    : _file(0), _chunk(0), opened_file(false), _pbuf(0), _read_offset(0),
     _minorVersion(minorVersion), _safe_read(false)
{
    ASSERT(save);
    _chunk = new chunk_reader(save, chunkname);
}

reader::~reader()
{
    if (_chunk)
        delete _chunk;
    close();
}

void reader::close()
{
    if (opened_file && _file)
        fclose(_file);
    _file = nullptr;
}

void reader::advance(size_t offset)
{
    char junk[128];

    while (offset)
    {
        const size_t junklen = min(sizeof(junk), offset);
        offset -= junklen;
        read(junk, junklen);
    }
}

bool reader::valid() const
{
    return (_file && !feof(_file)) ||
           (_pbuf && _read_offset < _pbuf->size());
}

static NORETURN void _short_read(bool safe_read)
{
    if (!crawl_state.need_save || safe_read)
        throw short_read_exception();
    // Would be nice to name the save chunk here, but in interesting cases
    // we're reading a copy from memory (why?).
    die_noline("short read while reading save");
}

// Reads input in network byte order, from a file or buffer.
unsigned char reader::readByte()
{
    if (_file)
    {
        int b = fgetc(_file);
        if (b == EOF)
            _short_read(_safe_read);
        return b;
    }
    else if (_chunk)
    {
        unsigned char buf;
        if (_chunk->read(&buf, 1) != 1)
            _short_read(_safe_read);
        return buf;
    }
    else
    {
        if (_read_offset >= _pbuf->size())
            _short_read(_safe_read);
        return (*_pbuf)[_read_offset++];
    }
}

void reader::read(void *data, size_t size)
{
    if (_file)
    {
        if (data)
        {
            if (fread(data, 1, size, _file) != size)
                _short_read(_safe_read);
        }
        else
            fseek(_file, (long)size, SEEK_CUR);
    }
    else if (_chunk)
    {
        if (_chunk->read(data, size) != size)
            _short_read(_safe_read);
    }
    else
    {
        if (_read_offset+size > _pbuf->size())
            _short_read(_safe_read);
        if (data && size)
            memcpy(data, &(*_pbuf)[_read_offset], size);

        _read_offset += size;
    }
}

int reader::getMinorVersion() const
{
    ASSERT(_minorVersion != TAG_MINOR_INVALID);
    return _minorVersion;
}

void reader::setMinorVersion(int minorVersion)
{
    _minorVersion = minorVersion;
}

void reader::fail_if_not_eof(const string &name)
{
    char dummy;
    if (_chunk ? _chunk->read(&dummy, 1) :
        _file ? (fgetc(_file) != EOF) :
        _read_offset >= _pbuf->size())
    {
        fail("Incomplete read of \"%s\" - aborting.", name.c_str());
    }
}

void writer::check_ok(bool ok)
{
    if (!ok && !failed)
    {
        failed = true;
        if (!_ignore_errors)
            end(1, true, "Error writing to %s", _filename.c_str());
    }
}

void writer::writeByte(unsigned char ch)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(&ch, 1);
    else if (_file)
        check_ok(fputc(ch, _file) != EOF);
    else
        _pbuf->push_back(ch);
}

void writer::write(const void *data, size_t size)
{
    if (failed)
        return;

    if (_chunk)
        _chunk->write(data, size);
    else if (_file)
        check_ok(fwrite(data, 1, size, _file) == size);
    else
    {
        const unsigned char* cdata = static_cast<const unsigned char*>(data);
        _pbuf->insert(_pbuf->end(), cdata, cdata+size);
    }
}

long writer::tell()
{
    ASSERT(!_chunk);
    return _file? ftell(_file) : _pbuf->size();
}

#ifdef DEBUG_GLOBALS
// Force a conditional jump valgrind may pick up, no matter the optimizations.
static volatile uint32_t hashroll;
static void CHECK_INITIALIZED(uint32_t x)
{
    hashroll = 0;
    if ((hashroll += x) & 1)
        hashroll += 2;
}
#else
#define CHECK_INITIALIZED(x)
#endif

// static helpers
static void tag_construct_char(writer &th);
static void tag_construct_you(writer &th);
static void tag_construct_you_items(writer &th);
static void tag_construct_you_dungeon(writer &th);
static void tag_construct_lost_monsters(writer &th);
static void tag_construct_companions(writer &th);
static void tag_read_you(reader &th);
static void tag_read_you_items(reader &th);
static void tag_read_you_dungeon(reader &th);
static void tag_read_lost_monsters(reader &th);
static void tag_read_lost_items(reader &th);
static void tag_read_companions(reader &th);

static void tag_construct_level(writer &th);
static void tag_construct_level_items(writer &th);
static void tag_construct_level_monsters(writer &th);
static void tag_construct_level_tiles(writer &th);
static void tag_read_level(reader &th);
static void tag_read_level_items(reader &th);
static void tag_read_level_monsters(reader &th);
static void tag_read_level_tiles(reader &th);
static void _regenerate_tile_flavour();
static void _draw_tiles();

static void tag_construct_ghost(writer &th, vector<ghost_demon> &);
static vector<ghost_demon> tag_read_ghost(reader &th);

static void marshallGhost(writer &th, const ghost_demon &ghost);
static ghost_demon unmarshallGhost(reader &th);

static void marshallSpells(writer &, const monster_spells &);
static void unmarshallSpells(reader &, monster_spells &
);

static void marshallMonsterInfo (writer &, const monster_info &);
static void unmarshallMonsterInfo (reader &, monster_info &mi);
static void marshallMapCell (writer &, const map_cell &);
static void unmarshallMapCell (reader &, map_cell& cell);

template<typename T, typename T_iter, typename T_marshal>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshal marshal);
template<typename T, typename U>
static void unmarshall_vector(reader& th, vector<T>& vec, U T_unmarshall);

template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr);
template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr);

void marshallByte(writer &th, int8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

int8_t unmarshallByte(reader &th)
{
    return th.readByte();
}

void marshallUByte(writer &th, uint8_t data)
{
    CHECK_INITIALIZED(data);
    th.writeByte(data);
}

uint8_t unmarshallUByte(reader &th)
{
    return th.readByte();
}

// Marshall 2 byte short in network order.
void marshallShort(writer &th, short data)
{
    CHECK_INITIALIZED(data);
    const char b2 = (char)(data & 0x00FF);
    const char b1 = (char)((data & 0xFF00) >> 8);
    th.writeByte(b1);
    th.writeByte(b2);
}

// Unmarshall 2 byte short in network order.
int16_t unmarshallShort(reader &th)
{
    int16_t b1 = th.readByte();
    int16_t b2 = th.readByte();
    int16_t data = (b1 << 8) | (b2 & 0x00FF);
    return data;
}

// Marshall 4 byte int in network order.
void marshallInt(writer &th, int32_t data)
{
    CHECK_INITIALIZED(data);
    char b4 = (char) (data & 0x000000FF);
    char b3 = (char)((data & 0x0000FF00) >> 8);
    char b2 = (char)((data & 0x00FF0000) >> 16);
    char b1 = (char)((data & 0xFF000000) >> 24);

    th.writeByte(b1);
    th.writeByte(b2);
    th.writeByte(b3);
    th.writeByte(b4);
}

// Unmarshall 4 byte signed int in network order.
int32_t unmarshallInt(reader &th)
{
    int32_t b1 = th.readByte();
    int32_t b2 = th.readByte();
    int32_t b3 = th.readByte();
    int32_t b4 = th.readByte();

    int32_t data = (b1 << 24) | ((b2 & 0x000000FF) << 16);
    data |= ((b3 & 0x000000FF) << 8) | (b4 & 0x000000FF);
    return data;
}

void marshallUnsigned(writer& th, uint64_t v)
{
    do
    {
        unsigned char b = (unsigned char)(v & 0x7f);
        v >>= 7;
        if (v)
            b |= 0x80;
        th.writeByte(b);
    }
    while (v);
}

uint64_t unmarshallUnsigned(reader& th)
{
    unsigned i = 0;
    uint64_t v = 0;
    for (;;)
    {
        unsigned char b = th.readByte();
        v |= (uint64_t)(b & 0x7f) << i;
        i += 7;
        if (!(b & 0x80))
            break;
    }
    return v;
}

void marshallSigned(writer& th, int64_t v)
{
    if (v < 0)
        marshallUnsigned(th, (uint64_t)((-v - 1) << 1) | 1);
    else
        marshallUnsigned(th, (uint64_t)(v << 1));
}

int64_t unmarshallSigned(reader& th)
{
    uint64_t u;
    unmarshallUnsigned(th, u);
    if (u & 1)
        return (int64_t)(-(u >> 1) - 1);
    else
        return (int64_t)(u >> 1);
}

// Optimized for short vectors that have only the first few bits set, and
// can have invalid length. For long ones you might want to do this
// differently to not lose 1/8 bits and speed.
template<int SIZE>
void marshallFixedBitVector(writer& th, const FixedBitVector<SIZE>& arr)
{
    int last_bit;
    for (last_bit = SIZE - 1; last_bit > 0; last_bit--)
        if (arr[last_bit])
            break;

    int i = 0;
    while (1)
    {
        uint8_t byte = 0;
        for (int j = 0; j < 7; j++)
            if (i < SIZE && arr[i++])
                byte |= 1 << j;
        if (i <= last_bit)
            marshallUByte(th, byte);
        else
        {
            marshallUByte(th, byte | 0x80);
            break;
        }
    }
}

template<int SIZE>
void unmarshallFixedBitVector(reader& th, FixedBitVector<SIZE>& arr)
{
    arr.reset();

    int i = 0;
    while (1)
    {
        uint8_t byte = unmarshallUByte(th);
        for (int j = 0; j < 7; j++)
            if (i < SIZE)
                arr.set(i++, !!(byte & (1 << j)));
        if (byte & 0x80)
            break;
    }
}

// FIXME: Kill this abomination - it will break!
template<typename T>
static void _marshall_as_int(writer& th, const T& t)
{
    marshallInt(th, static_cast<int>(t));
}

template <typename data>
void marshallSet(writer &th, const set<data> &s,
                 void (*marshall)(writer &, const data &))
{
    marshallInt(th, s.size());
    for (const data &elt : s)
        marshall(th, elt);
}

template<typename key, typename value>
void marshallMap(writer &th, const map<key,value>& data,
                 void (*key_marshall)(writer&, const key&),
                 void (*value_marshall)(writer&, const value&))
{
    marshallInt(th, data.size());
    for (const auto &entry : data)
    {
        key_marshall(th, entry.first);
        value_marshall(th, entry.second);
    }
}

template<typename T_iter, typename T_marshall_t>
static void marshall_iterator(writer &th, T_iter beg, T_iter end,
                              T_marshall_t T_marshall)
{
    marshallInt(th, distance(beg, end));
    while (beg != end)
    {
        T_marshall(th, *beg);
        ++beg;
    }
}

template<typename T, typename U>
static void unmarshall_vector(reader& th, vector<T>& vec, U T_unmarshall)
{
    vec.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        vec.push_back(T_unmarshall(th));
}

template <typename T_container, typename T_inserter, typename T_unmarshall>
static void unmarshall_container(reader &th, T_container &container,
                                 T_inserter inserter, T_unmarshall unmarshal)
{
    container.clear();
    const int num_to_read = unmarshallInt(th);
    for (int i = 0; i < num_to_read; ++i)
        (container.*inserter)(unmarshal(th));
}

static unsigned short _pack(const level_id& id)
{
    return (static_cast<int>(id.branch) << 8) | (id.depth & 0xFF);
}

void marshall_level_id(writer& th, const level_id& id)
{
    marshallShort(th, _pack(id));
}

static void _marshall_level_id_set(writer& th, const set<level_id>& id)
{
    marshallSet(th, id, marshall_level_id);
}

// XXX: Redundant with level_pos.save()/load().
static void _marshall_level_pos(writer& th, const level_pos& lpos)
{
    marshallInt(th, lpos.pos.x);
    marshallInt(th, lpos.pos.y);
    marshall_level_id(th, lpos.id);
}

template <typename data, typename set>
void unmarshallSet(reader &th, set &dset,
                   data (*data_unmarshall)(reader &))
{
    dset.clear();
    int len = unmarshallInt(th);
    for (int i = 0; i < len; ++i)
        dset.insert(data_unmarshall(th));
}

template<typename key, typename value, typename map>
void unmarshallMap(reader& th, map& data,
                   key   (*key_unmarshall)  (reader&),
                   value (*value_unmarshall)(reader&))
{
    const int len = unmarshallInt(th);
    key k;
    for (int i = 0; i < len; ++i)
    {
        k = key_unmarshall(th);
        pair<key, value> p(k, value_unmarshall(th));
        data.insert(p);
    }
}

template<typename T>
static T unmarshall_int_as(reader& th)
{
    return static_cast<T>(unmarshallInt(th));
}

level_id _unpack(unsigned short place)
{
    level_id id;

    id.branch     = static_cast<branch_type>((place >> 8) & 0xFF);
    id.depth      = (int8_t)(place & 0xFF);

    return id;
}

level_id unmarshall_level_id(reader& th)
{
    return _unpack(unmarshallShort(th));
}

static set<level_id> _unmarshall_level_id_set(reader& th)
{
    set<level_id> id;
    unmarshallSet(th, id, unmarshall_level_id);
    return id;
}

static level_pos _unmarshall_level_pos(reader& th)
{
    level_pos lpos;
    lpos.pos.x = unmarshallInt(th);
    lpos.pos.y = unmarshallInt(th);
    lpos.id    = unmarshall_level_id(th);
    return lpos;
}

void marshallCoord(writer &th, const coord_def &c)
{
    marshallInt(th, c.x);
    marshallInt(th, c.y);
}

coord_def unmarshallCoord(reader &th)
{
    coord_def c;
        c.x = unmarshallInt(th);
        c.y = unmarshallInt(th);
    return c;
}


static void _marshall_constriction(writer &th, const actor *who)
{
    marshallInt(th, who->constricted_by);
    marshallInt(th, who->escape_attempts);

    // Assumes an empty map is marshalled as just the int 0.
    const actor::constricting_t * const cmap = who->constricting;
    if (cmap)
        marshallMap(th, *cmap, _marshall_as_int<mid_t>, _marshall_as_int<int>);
    else
        marshallInt(th, 0);
}

static void _unmarshall_constriction(reader &th, actor *who)
{
    who->constricted_by = unmarshallInt(th);
    who->escape_attempts = unmarshallInt(th);

    actor::constricting_t cmap;
    unmarshallMap(th, cmap, unmarshall_int_as<mid_t>, unmarshallInt);

    if (cmap.size() == 0)
        who->constricting = 0;
    else
        who->constricting = new actor::constricting_t(cmap);
}

template <typename marshall, typename grid>
static void _run_length_encode(writer &th, marshall m, const grid &g,
                               int width, int height)
{
    int last = 0, nlast = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (!nlast)
                last = g[x][y];
            if (last == g[x][y] && nlast < 255)
            {
                nlast++;
                continue;
            }

            marshallByte(th, nlast);
            m(th, last);

            last = g[x][y];
            nlast = 1;
        }

    marshallByte(th, nlast);
    m(th, last);
}

template <typename unmarshall, typename grid>
static void _run_length_decode(reader &th, unmarshall um, grid &g,
                               int width, int height)
{
    const int end = width * height;
    int offset = 0;
    while (offset < end)
    {
        const int run = unmarshallUByte(th);
        const int value = um(th);

        for (int i = 0; i < run; ++i)
        {
            const int y = offset / width;
            const int x = offset % width;
            g[x][y] = value;
            ++offset;
        }
    }
}

union float_marshall_kludge
{
    float    f_num;
    int32_t  l_num;
};
COMPILE_CHECK(sizeof(float) == sizeof(int32_t));

// single precision float -- marshall in network order.
void marshallFloat(writer &th, float data)
{
    float_marshall_kludge k;
    k.f_num = data;
    marshallInt(th, k.l_num);
}

// single precision float -- unmarshall in network order.
float unmarshallFloat(reader &th)
{
    float_marshall_kludge k;
    k.l_num = unmarshallInt(th);
    return k.f_num;
}

// string -- 2 byte length, string data
void marshallString(writer &th, const string &data)
{
    size_t len = data.length();
    // A limit of 32K.
    if (len > SHRT_MAX)
        die("trying to marshall too long a string (len=%ld)", (long int)len);
    marshallShort(th, len);

    th.write(data.c_str(), len);
}

string unmarshallString(reader &th)
{
    char buffer[SHRT_MAX];

    short len = unmarshallShort(th);
    ASSERT(len >= 0);
    ASSERT(len <= (ssize_t)sizeof(buffer));

    th.read(buffer, len);

    return string(buffer, len);
}

// This one must stay with a 16 bit signed big-endian length tag, to allow
// older versions to browse and list newer saves.
static void marshallString2(writer &th, const string &data)
{
    marshallString(th, data);
}

static string unmarshallString2(reader &th)
{
    return unmarshallString(th);
}

// string -- 4 byte length, non-terminated string data.
void marshallString4(writer &th, const string &data)
{
    marshallInt(th, data.length());
    th.write(data.c_str(), data.length());
}
void unmarshallString4(reader &th, string& s)
{
    const int len = unmarshallInt(th);
    s.resize(len);
    if (len) th.read(&s.at(0), len);
}

// boolean (to avoid system-dependent bool implementations)
void marshallBoolean(writer &th, bool data)
{
    th.writeByte(data ? 1 : 0);
}

// boolean (to avoid system-dependent bool implementations)
bool unmarshallBoolean(reader &th)
{
    return th.readByte() != 0;
}

// Saving the date as a string so we're not reliant on a particular epoch.
string make_date_string(time_t in_date)
{
    if (in_date <= 0)
        return "";

    struct tm *date = TIME_FN(&in_date);

    return make_stringf(
              "%4d%02d%02d%02d%02d%02d%s",
              date->tm_year + 1900, date->tm_mon, date->tm_mday,
              date->tm_hour, date->tm_min, date->tm_sec,
              ((date->tm_isdst > 0) ? "D" : "S"));
}

static void marshallStringVector(writer &th, const vector<string> &vec)
{
    marshall_iterator(th, vec.begin(), vec.end(), marshallString);
}

static vector<string> unmarshallStringVector(reader &th)
{
    vector<string> vec;
    unmarshall_vector(th, vec, unmarshallString);
    return vec;
}

static monster_type unmarshallMonType(reader &th)
{
    monster_type x = static_cast<monster_type>(unmarshallShort(th));

    if (x >= MONS_NO_MONSTER)
        return x;


    return x;
}


static spell_type unmarshallSpellType(reader &th
                                      )
{
    spell_type x = SPELL_NO_SPELL;
        x = static_cast<spell_type>(unmarshallShort(th));


    return x;
}

static dungeon_feature_type rewrite_feature(dungeon_feature_type x,
                                            int minor_version)
{

    return x;
}

dungeon_feature_type unmarshallFeatureType(reader &th)
{
    dungeon_feature_type x = static_cast<dungeon_feature_type>(unmarshallUByte(th));
    return rewrite_feature(x, th.getMinorVersion());
}


#define CANARY     marshallUByte(th, 171)
#define EAT_CANARY do if ( unmarshallUByte(th) != 171)                  \
                   {                                                    \
                       die("save corrupted: canary gone");              \
                   } while (0)


// Write a tagged chunk of data to the FILE*.
// tagId specifies what to write.
void tag_write(tag_type tagID, writer &outf)
{
    vector<unsigned char> buf;
    writer th(&buf);
    switch (tagID)
    {
    case TAG_CHR:
        tag_construct_char(th);
        break;
    case TAG_YOU:
        tag_construct_you(th);
        CANARY;
        tag_construct_you_items(th);
        CANARY;
        tag_construct_you_dungeon(th);
        CANARY;
        tag_construct_lost_monsters(th);
        CANARY;
        tag_construct_companions(th);
        break;
    case TAG_LEVEL:
        tag_construct_level(th);
        CANARY;
        tag_construct_level_items(th);
        CANARY;
        tag_construct_level_monsters(th);
        CANARY;
        tag_construct_level_tiles(th);
        break;
    case TAG_GHOST:
        tag_construct_ghost(th, global_ghosts);
        break;
    default:
        // I don't know how to make that!
        break;
    }

    // make sure there is some data to write!
    if (buf.empty())
        return;

    // Write tag header.
    marshallInt(outf, buf.size());

    // Write tag data.
    outf.write(&buf[0], buf.size());
}

static void _shunt_monsters_out_of_walls()
{
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monster &m(menv[i]);
        if (m.alive() && in_bounds(m.pos()) && cell_is_solid(m.pos())
            && (grd(m.pos()) != DNGN_MALIGN_GATEWAY
                || mons_genus(m.type) != MONS_ELDRITCH_TENTACLE))
        {
            for (distance_iterator di(m.pos()); di; ++di)
                if (!actor_at(*di) && !cell_is_solid(*di))
                {
                    mprf(MSGCH_ERROR, "Error: monster %s in %s at (%d,%d)",
                         m.name(DESC_PLAIN, true).c_str(),
                         dungeon_feature_name(grd(m.pos())),
                         m.pos().x, m.pos().y);
                    env.mgrid(m.pos()) = NON_MONSTER;
                    m.position = *di;
                    env.mgrid(*di) = i;
                    break;
                }
        }
    }
}

// Read a piece of data from inf into memory, then run the appropriate reader.
//
// minorVersion is available for any sub-readers that need it
void tag_read(reader &inf, tag_type tag_id)
{
    // Read header info and data
    vector<unsigned char> buf;
    const int data_size = unmarshallInt(inf);
    ASSERT(data_size >= 0);

    // Fetch data in one go
    buf.resize(data_size);
    inf.read(&buf[0], buf.size());

    // Ok, we have data now.
    reader th(buf, inf.getMinorVersion());
    switch (tag_id)
    {
    case TAG_YOU:
        tag_read_you(th);
        EAT_CANARY;
        tag_read_you_items(th);
        EAT_CANARY;
        tag_read_you_dungeon(th);
        EAT_CANARY;
        tag_read_lost_monsters(th);
        EAT_CANARY;
        tag_read_companions(th);

        // If somebody SIGHUP'ed out of the skill menu with all skills disabled.
        // Doing this here rather in tag_read_you() because you.can_train()
        // requires the player's equipment be loaded.
        init_can_train();
        check_selected_skills();
        break;
    case TAG_LEVEL:
        tag_read_level(th);
        EAT_CANARY;
        tag_read_level_items(th);
        // We have to do this here because tag_read_level_monsters()
        // might kill an elsewhere Ilsuiw follower, which ends up calling
        // terrain.cc:_dgn_check_terrain_items, which checks mitm.
        link_items();
        EAT_CANARY;
        tag_read_level_monsters(th);
        EAT_CANARY;
        _shunt_monsters_out_of_walls();
        // The Abyss needs to visit other levels during level gen, before
        // all cells have been filled. We mustn't crash when it returns
        // from those excursions, and generate_abyss will check_map_validity
        // itself after the grid is fully populated.
        if (!player_in_branch(BRANCH_ABYSS))
        {
            unwind_var<coord_def> you_pos(you.position, coord_def());
            check_map_validity();
        }
        tag_read_level_tiles(th);
        break;
    case TAG_GHOST:
        global_ghosts = tag_read_ghost(th);
        break;
    default:
        // I don't know how to read that!
        die("unknown tag type");
    }
}

static void tag_construct_char(writer &th)
{
    marshallByte(th, TAG_CHR_FORMAT);
    // Important: you may never remove or alter a field without bumping
    // CHR_FORMAT. Bumping it makes all saves invisible when browsed in an
    // older version.
    // Please keep this compatible even over major version breaks!

    // Appending fields is fine, but inserting new fields anywhere other than
    // the end of this function is not!

    marshallString2(th, you.your_name);
    marshallString2(th, Version::Long);

    marshallByte(th, you.species);
    marshallByte(th, you.char_class);
    marshallByte(th, you.experience_level);
    marshallString2(th, string(get_job_name(you.char_class)));
    marshallByte(th, you.religion);
    marshallString2(th, you.jiyva_second_name);

    // don't save wizmode suppression
    marshallByte(th, you.wizard || you.suppress_wizard);

    marshallByte(th, crawl_state.type);
    if (crawl_state.game_is_tutorial())
        marshallString2(th, crawl_state.map);

    marshallString2(th, species_name(you.species));
    marshallString2(th, you.religion ? god_name(you.religion) : "");

    // separate from the tutorial so we don't have to bump TAG_CHR_FORMAT
    marshallString2(th, crawl_state.map);

    marshallByte(th, you.explore);
}

/// is a custom scoring mechanism being stored?
static bool _calc_score_exists()
{
    lua_stack_cleaner clean(dlua);
    dlua.pushglobal("dgn.persist.calc_score");
    return !lua_isnil(dlua, -1);
}

static void tag_construct_you(writer &th)
{
    marshallInt(th, you.last_mid);
    marshallByte(th, you.piety);
    marshallShort(th, you.pet_target);

    marshallByte(th, you.max_level);
    marshallByte(th, you.where_are_you);
    marshallByte(th, you.depth);
    marshallByte(th, you.chapter);
    marshallByte(th, you.royal_jelly_dead);
    marshallByte(th, you.transform_uncancellable);
    marshallByte(th, you.berserk_penalty);
    marshallInt(th, you.abyss_speed);

    marshallInt(th, you.disease);
    ASSERT(you.hp > 0 || you.pending_revival);
    marshallShort(th, you.pending_revival ? 0 : you.hp);

    marshallShort(th, you.hunger);
    marshallBoolean(th, you.fishtail);
    _marshall_as_int(th, you.form);
    CANARY;

    // how many you.equip?
    marshallByte(th, NUM_EQUIP - EQ_FIRST_EQUIP);
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        marshallByte(th, you.equip[i]);
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
        marshallBoolean(th, you.melded[i]);

    ASSERT_RANGE(you.magic_points, 0, you.max_magic_points + 1);
    marshallUByte(th, you.magic_points);
    marshallByte(th, you.max_magic_points);

    COMPILE_CHECK(NUM_STATS == 3);
    for (int i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.base_stats[i]);
    for (int i = 0; i < NUM_STATS; ++i)
        marshallByte(th, you.stat_loss[i]);

    CANARY;

    marshallInt(th, you.hit_points_regeneration);
    marshallInt(th, you.magic_points_regeneration);

    marshallInt(th, you.experience);
    marshallInt(th, you.total_experience);
    marshallInt(th, you.gold);

    marshallInt(th, you.exp_available);

    marshallInt(th, you.zigs_completed);
    marshallByte(th, you.zig_max);

    marshallString(th, you.banished_by);

    marshallShort(th, you.hp_max_adj_temp);
    marshallShort(th, you.hp_max_adj_perm);
    marshallShort(th, you.mp_max_adj);

    marshallShort(th, you.pos().x);
    marshallShort(th, you.pos().y);

    marshallFixedBitVector<NUM_SPELLS>(th, you.spell_library);
    marshallFixedBitVector<NUM_SPELLS>(th, you.hidden_spells);

    // how many spells?
    marshallUByte(th, MAX_KNOWN_SPELLS);
    for (int i = 0; i < MAX_KNOWN_SPELLS; ++i)
        marshallShort(th, you.spells[i]);

    marshallByte(th, 52);
    for (int i = 0; i < 52; i++)
        marshallByte(th, you.spell_letter_table[i]);

    marshallByte(th, 52);
    for (int i = 0; i < 52; i++)
        marshallShort(th, you.ability_letter_table[i]);

    marshallUByte(th, you.old_vehumet_gifts.size());
    for (auto spell : you.old_vehumet_gifts)
        marshallShort(th, spell);

    marshallUByte(th, you.vehumet_gifts.size());
    for (auto spell : you.vehumet_gifts)
        marshallShort(th, spell);

    CANARY;

    // how many skills?
    marshallByte(th, NUM_SKILLS);
    for (int j = 0; j < NUM_SKILLS; ++j)
    {
        marshallUByte(th, you.skills[j]);
        marshallByte(th, you.train[j]);
        marshallByte(th, you.train_alt[j]);
        marshallInt(th, you.training[j]);
        marshallInt(th, you.skill_points[j]);
        marshallInt(th, you.ct_skill_points[j]);
        marshallByte(th, you.skill_order[j]);   // skills ordering
        marshallInt(th, you.training_targets[j]);
    }

    marshallBoolean(th, you.auto_training);
    marshallByte(th, you.exercises.size());
    for (auto sk : you.exercises)
        marshallInt(th, sk);

    marshallByte(th, you.exercises_all.size());
    for (auto sk : you.exercises_all)
        marshallInt(th, sk);

    marshallByte(th, you.skill_menu_do);
    marshallByte(th, you.skill_menu_view);

    marshallInt(th, you.transfer_from_skill);
    marshallInt(th, you.transfer_to_skill);
    marshallInt(th, you.transfer_skill_points);
    marshallInt(th, you.transfer_total_skill_points);

    CANARY;

    // how many durations?
    marshallUByte(th, NUM_DURATIONS);
    for (int j = 0; j < NUM_DURATIONS; ++j)
        marshallInt(th, you.duration[j]);

    // how many attributes?
    marshallByte(th, NUM_ATTRIBUTES);
    for (int j = 0; j < NUM_ATTRIBUTES; ++j)
        marshallInt(th, you.attribute[j]);

    // Event timers.
    marshallByte(th, NUM_TIMERS);
    for (int j = 0; j < NUM_TIMERS; ++j)
    {
        marshallInt(th, you.last_timer_effect[j]);
        marshallInt(th, you.next_timer_effect[j]);
    }

    // how many mutations/demon powers?
    marshallShort(th, NUM_MUTATIONS);
    for (int j = 0; j < NUM_MUTATIONS; ++j)
    {
        marshallByte(th, you.mutation[j]);
        marshallByte(th, you.innate_mutation[j]);
        marshallByte(th, you.temp_mutation[j]);
        marshallByte(th, you.sacrifices[j]);
    }

    marshallByte(th, you.demonic_traits.size());
    for (int j = 0; j < int(you.demonic_traits.size()); ++j)
    {
        marshallByte(th, you.demonic_traits[j].level_gained);
        marshallShort(th, you.demonic_traits[j].mutation);
    }

    // set up sacrifice piety by ability
    marshallShort(th, 1 + ABIL_FINAL_SACRIFICE - ABIL_FIRST_SACRIFICE);
    for (int j = ABIL_FIRST_SACRIFICE; j <= ABIL_FINAL_SACRIFICE; ++j)
        marshallByte(th, you.sacrifice_piety[j]);

    CANARY;

    // how many penances?
    marshallByte(th, NUM_GODS);
    for (god_iterator it; it; ++it)
        marshallByte(th, you.penance[*it]);

    // which gods have been worshipped by this character?
    for (god_iterator it; it; ++it)
        marshallByte(th, you.worshipped[*it]);

    // what is the extent of divine generosity?
    for (god_iterator it; it; ++it)
        marshallShort(th, you.num_current_gifts[*it]);
    for (god_iterator it; it; ++it)
        marshallShort(th, you.num_total_gifts[*it]);
    for (god_iterator it; it; ++it)
        marshallBoolean(th, you.one_time_ability_used[*it]);

    // how much piety have you achieved at highest with each god?
    for (god_iterator it; it; ++it)
        marshallByte(th, you.piety_max[*it]);

    marshallByte(th, you.gift_timeout);
    marshallUByte(th, you.saved_good_god_piety);
    marshallByte(th, you.previous_good_god);

    for (god_iterator it; it; ++it)
        marshallInt(th, you.exp_docked[*it]);
    for (god_iterator it; it; ++it)
        marshallInt(th, you.exp_docked_total[*it]);

    // elapsed time
    marshallInt(th, you.elapsed_time);

    // time of game start
    marshallInt(th, you.birth_time);

    handle_real_time();

    // TODO: maybe switch to marshalling real_time_ms.
    marshallInt(th, you.real_time());
    marshallInt(th, you.num_turns);
    marshallInt(th, you.exploration);

    marshallInt(th, you.magic_contamination);

    marshallUByte(th, you.transit_stair);
    marshallByte(th, you.entering_level);
    marshallBoolean(th, you.travel_ally_pace);

    marshallByte(th, you.deaths);
    marshallByte(th, you.lives);

    CANARY;

    marshallInt(th, you.dactions.size());
    for (daction_type da : you.dactions)
        marshallByte(th, da);

    marshallInt(th, you.level_stack.size());
    for (const level_pos &lvl : you.level_stack)
        lvl.save(th);

    // List of currently beholding monsters (usually empty).
    marshallShort(th, you.beholders.size());
    for (mid_t beh : you.beholders)
         _marshall_as_int(th, beh);

    marshallShort(th, you.fearmongers.size());
    for (mid_t monger : you.fearmongers)
        _marshall_as_int(th, monger);

    marshallByte(th, you.piety_hysteresis);

    you.m_quiver.save(th);

    CANARY;

    // Action counts.
    marshallShort(th, you.action_count.size());
    for (const auto &ac : you.action_count)
    {
        marshallShort(th, ac.first.first);
        marshallInt(th, ac.first.second);
        for (int k = 0; k < 27; k++)
            marshallInt(th, ac.second[k]);
    }

    marshallByte(th, NUM_BRANCHES);
    for (int i = 0; i < NUM_BRANCHES; i++)
        marshallBoolean(th, you.branches_left[i]);

    marshallCoord(th, abyssal_state.major_coord);
    marshallInt(th, abyssal_state.seed);
    marshallInt(th, abyssal_state.depth);
    marshallFloat(th, abyssal_state.phase);
    marshall_level_id(th, abyssal_state.level);


    _marshall_constriction(th, &you);

    marshallUByte(th, you.octopus_king_rings);

    marshallUnsigned(th, you.uncancel.size());
    for (const pair<uncancellable_type, int>& unc : you.uncancel)
    {
        marshallUByte(th, unc.first);
        marshallInt(th, unc.second);
    }

    marshallUnsigned(th, you.recall_list.size());
    for (mid_t recallee : you.recall_list)
        _marshall_as_int(th, recallee);

    marshallUByte(th, 1); // number of seeds: always 1
    marshallInt(th, you.game_seed);

    CANARY;

    // don't let vault caching errors leave a normal game with sprint scoring
    if (!crawl_state.game_is_sprint())
        ASSERT(!_calc_score_exists());

    if (!dlua.callfn("dgn_save_data", "u", &th))
        mprf(MSGCH_ERROR, "Failed to save Lua data: %s", dlua.error.c_str());

    CANARY;

    // Write a human-readable string out on the off chance that
    // we fail to be able to read this file back in using some later version.
    string revision = "Git:";
    revision += Version::Long;
    marshallString(th, revision);

    you.props.write(th);
}

static void tag_construct_you_items(writer &th)
{
    // how many inventory slots?
    marshallByte(th, ENDOFPACK);
    for (const auto &item : you.inv)
        marshallItem(th, item);

    marshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    marshallByte(th, you.obtainable_runes);

    // Item descrip for each type & subtype.
    // how many types?
    marshallUByte(th, NUM_IDESC);
    // how many subtypes?
    marshallUByte(th, MAX_SUBTYPES);
    for (int i = 0; i < NUM_IDESC; ++i)
        for (int j = 0; j < MAX_SUBTYPES; ++j)
            marshallInt(th, you.item_description[i][j]);

    marshallUByte(th, NUM_OBJECT_CLASSES);
    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (int j = 0; j < MAX_SUBTYPES; ++j)
            marshallBoolean(th, you.type_ids[i][j]);
    }

    CANARY;

    // how many unique items?
    marshallUByte(th, MAX_UNRANDARTS);
    for (int j = 0; j < MAX_UNRANDARTS; ++j)
        marshallByte(th,you.unique_items[j]);

    marshallShort(th, NUM_WEAPONS);
    for (int j = 0; j < NUM_WEAPONS; ++j)
        marshallInt(th,you.seen_weapon[j]);

    marshallShort(th, NUM_ARMOURS);
    for (int j = 0; j < NUM_ARMOURS; ++j)
        marshallInt(th,you.seen_armour[j]);

    marshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
        for (int j = 0; j < MAX_SUBTYPES; j++)
            marshallInt(th, you.force_autopickup[i][j]);
}

static void marshallPlaceInfo(writer &th, PlaceInfo place_info)
{
    marshallInt(th, place_info.branch);

    marshallInt(th, place_info.num_visits);
    marshallInt(th, place_info.levels_seen);

    marshallInt(th, place_info.mon_kill_exp);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        marshallInt(th, place_info.mon_kill_num[i]);

    marshallInt(th, place_info.turns_total);
    marshallInt(th, place_info.turns_explore);
    marshallInt(th, place_info.turns_travel);
    marshallInt(th, place_info.turns_interlevel);
    marshallInt(th, place_info.turns_resting);
    marshallInt(th, place_info.turns_other);

    marshallInt(th, place_info.elapsed_total);
    marshallInt(th, place_info.elapsed_explore);
    marshallInt(th, place_info.elapsed_travel);
    marshallInt(th, place_info.elapsed_interlevel);
    marshallInt(th, place_info.elapsed_resting);
    marshallInt(th, place_info.elapsed_other);
}

static void marshallLevelXPInfo(writer &th, LevelXPInfo xp_info)
{
    marshall_level_id(th, xp_info.level);

    marshallInt(th, xp_info.non_vault_xp);
    marshallInt(th, xp_info.non_vault_count);
    marshallInt(th, xp_info.vault_xp);
    marshallInt(th, xp_info.vault_count);
}

static void tag_construct_you_dungeon(writer &th)
{
    // how many unique creatures?
    marshallShort(th, NUM_MONSTERS);
    for (int j = 0; j < NUM_MONSTERS; ++j)
        marshallByte(th,you.unique_creatures[j]); // unique beasties

    // how many branches?
    marshallByte(th, NUM_BRANCHES);
    for (int j = 0; j < NUM_BRANCHES; ++j)
    {
        marshallInt(th, brdepth[j]);
        marshall_level_id(th, brentry[j]);
        marshallInt(th, branch_bribe[j]);
    }

    // Root of the dungeon; usually BRANCH_DUNGEON.
    marshallInt(th, root_branch);

    marshallMap(th, stair_level,
                _marshall_as_int<branch_type>, _marshall_level_id_set);
    marshallMap(th, shops_present,
                _marshall_level_pos, _marshall_as_int<shop_type>);
    marshallMap(th, altars_present,
                _marshall_level_pos, _marshall_as_int<god_type>);
    marshallMap(th, portals_present,
                _marshall_level_pos, _marshall_as_int<branch_type>);
    marshallMap(th, portal_notes,
                _marshall_level_pos, marshallString);
    marshallMap(th, level_annotations,
                marshall_level_id, marshallString);
    marshallMap(th, level_exclusions,
                marshall_level_id, marshallString);
    marshallMap(th, level_uniques,
            marshall_level_id, marshallString);
    marshallUniqueAnnotations(th);

    marshallPlaceInfo(th, you.global_info);
    vector<PlaceInfo> list = you.get_all_place_info();
    // How many different places we have info on?
    marshallShort(th, list.size());

    for (const PlaceInfo &place : list)
        marshallPlaceInfo(th, place);

    marshallLevelXPInfo(th, you.global_xp_info);

    vector<LevelXPInfo> xp_info_list = you.get_all_xp_info();
    // How many different levels do we have info on?
    marshallShort(th, xp_info_list.size());
    for (const auto info: xp_info_list)
        marshallLevelXPInfo(th, info);

    marshall_iterator(th, you.uniq_map_tags.begin(), you.uniq_map_tags.end(),
                      marshallString);
    marshall_iterator(th, you.uniq_map_names.begin(), you.uniq_map_names.end(),
                      marshallString);
    marshallMap(th, you.vault_list, marshall_level_id, marshallStringVector);

    write_level_connectivity(th);
}

static void marshall_follower(writer &th, const follower &f)
{
    ASSERT(!invalid_monster_type(f.mons.type));
    ASSERT(f.mons.alive());
    marshallMonster(th, f.mons);
    marshallInt(th, f.transit_start_time);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        marshallItem(th, f.items[i]);
}

static follower unmarshall_follower(reader &th)
{
    follower f;
    unmarshallMonster(th, f.mons);
        f.transit_start_time = unmarshallInt(th);
    for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        unmarshallItem(th, f.items[i]);
    return f;
}

static void marshall_companion(writer &th, const companion &c)
{
    marshall_follower(th, c.mons);
    marshall_level_id(th, c.level);
    marshallInt(th, c.timestamp);
}

static companion unmarshall_companion(reader &th)
{
    companion c;
    c.mons = unmarshall_follower(th);
    c.level = unmarshall_level_id(th);
    c.timestamp = unmarshallInt(th);
    return c;
}

static void marshall_follower_list(writer &th, const m_transit_list &mlist)
{
    marshallShort(th, mlist.size());

    for (const auto &follower : mlist)
        marshall_follower(th, follower);
}

static m_transit_list unmarshall_follower_list(reader &th)
{
    m_transit_list mlist;

    const int size = unmarshallShort(th);

    for (int i = 0; i < size; ++i)
    {
        follower f = unmarshall_follower(th);
        if (!f.mons.alive())
        {
            mprf(MSGCH_ERROR,
                 "Dead monster %s in transit list in saved game, ignoring.",
                 f.mons.name(DESC_PLAIN, true).c_str());
        }
        else
            mlist.push_back(f);
    }

    return mlist;
}


static void marshall_level_map_masks(writer &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        marshallInt(th, env.level_map_mask(*ri));
        marshallInt(th, env.level_map_ids(*ri));
    }
}

static void unmarshall_level_map_masks(reader &th)
{
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        env.level_map_mask(*ri) = unmarshallInt(th);
        env.level_map_ids(*ri)  = unmarshallInt(th);
    }
}

static void marshall_level_map_unique_ids(writer &th)
{
    marshallSet(th, env.level_uniq_maps, marshallString);
    marshallSet(th, env.level_uniq_map_tags, marshallString);
}

static void unmarshall_level_map_unique_ids(reader &th)
{
    unmarshallSet(th, env.level_uniq_maps, unmarshallString);
    unmarshallSet(th, env.level_uniq_map_tags, unmarshallString);
}

static void marshall_subvault_place(writer &th,
                                    const subvault_place &subvault_place);

static void marshall_mapdef(writer &th, const map_def &map)
{
    marshallString(th, map.name);
    map.write_index(th);
    map.write_maplines(th);
    marshallString(th, map.description);
    marshallMap(th, map.feat_renames,
                _marshall_as_int<dungeon_feature_type>, marshallString);
    marshall_iterator(th,
                      map.subvault_places.begin(),
                      map.subvault_places.end(),
                      marshall_subvault_place);
}

static void marshall_subvault_place(writer &th,
                                    const subvault_place &subvault_place)
{
    marshallCoord(th, subvault_place.tl);
    marshallCoord(th, subvault_place.br);
    marshall_mapdef(th, *subvault_place.subvault);
}

static subvault_place unmarshall_subvault_place(reader &th);
static map_def unmarshall_mapdef(reader &th)
{
    map_def map;
    map.name = unmarshallString(th);
    map.read_index(th);
    map.read_maplines(th);
    map.description = unmarshallString(th);
    unmarshallMap(th, map.feat_renames,
                  unmarshall_int_as<dungeon_feature_type>,
                  unmarshallString);
        unmarshall_vector(th, map.subvault_places, unmarshall_subvault_place);
    return map;
}

static subvault_place unmarshall_subvault_place(reader &th)
{
    subvault_place subvault;
    subvault.tl = unmarshallCoord(th);
    subvault.br = unmarshallCoord(th);
    subvault.set_subvault(unmarshall_mapdef(th));
    return subvault;
}

static void marshall_vault_placement(writer &th, const vault_placement &vp)
{
    marshallCoord(th, vp.pos);
    marshallCoord(th, vp.size);
    marshallShort(th, vp.orient);
    marshall_mapdef(th, vp.map);
    marshall_iterator(th, vp.exits.begin(), vp.exits.end(), marshallCoord);
    marshallByte(th, vp.seen);
}

static vault_placement unmarshall_vault_placement(reader &th)
{
    vault_placement vp;
    vp.pos = unmarshallCoord(th);
    vp.size = unmarshallCoord(th);
    vp.orient = static_cast<map_section_type>(unmarshallShort(th));
    vp.map = unmarshall_mapdef(th);
    unmarshall_vector(th, vp.exits, unmarshallCoord);
    vp.seen = !!unmarshallByte(th);

    return vp;
}

static void marshall_level_vault_placements(writer &th)
{
    marshallShort(th, env.level_vaults.size());
    for (unique_ptr<vault_placement> &vp : env.level_vaults)
        marshall_vault_placement(th, *vp);
}

static void unmarshall_level_vault_placements(reader &th)
{
    const int nvaults = unmarshallShort(th);
    ASSERT(nvaults >= 0);
    dgn_clear_vault_placements();
    for (int i = 0; i < nvaults; ++i)
    {
        env.level_vaults.emplace_back(
            new vault_placement(unmarshall_vault_placement(th)));
    }
}

static void marshall_level_vault_data(writer &th)
{
    marshallString(th, env.level_build_method);
    marshallSet(th, env.level_layout_types, marshallString);

    marshall_level_map_masks(th);
    marshall_level_map_unique_ids(th);
    marshall_level_vault_placements(th);
}

static void unmarshall_level_vault_data(reader &th)
{
    env.level_build_method = unmarshallString(th);
    unmarshallSet(th, env.level_layout_types, unmarshallString);

    unmarshall_level_map_masks(th);
    unmarshall_level_map_unique_ids(th);
    unmarshall_level_vault_placements(th);
}

static void marshall_shop(writer &th, const shop_struct& shop)
{
    marshallByte(th, shop.type);
    marshallByte(th, shop.keeper_name[0]);
    marshallByte(th, shop.keeper_name[1]);
    marshallByte(th, shop.keeper_name[2]);
    marshallByte(th, shop.pos.x);
    marshallByte(th, shop.pos.y);
    marshallByte(th, shop.greed);
    marshallByte(th, shop.level);
    marshallString(th, shop.shop_name);
    marshallString(th, shop.shop_type_name);
    marshallString(th, shop.shop_suffix_name);
    marshall_iterator(th, shop.stock.begin(), shop.stock.end(),
                          bind(marshallItem, placeholders::_1, placeholders::_2, false));
}

static void unmarshall_shop(reader &th, shop_struct& shop)
{
    shop.type  = static_cast<shop_type>(unmarshallByte(th));
    ASSERT(shop.type != SHOP_UNASSIGNED);
    shop.keeper_name[0] = unmarshallUByte(th);
    shop.keeper_name[1] = unmarshallUByte(th);
    shop.keeper_name[2] = unmarshallUByte(th);
    shop.pos.x = unmarshallByte(th);
    shop.pos.y = unmarshallByte(th);
    shop.greed = unmarshallByte(th);
    shop.level = unmarshallByte(th);
    shop.shop_name = unmarshallString(th);
    shop.shop_type_name = unmarshallString(th);
    shop.shop_suffix_name = unmarshallString(th);
    unmarshall_vector(th, shop.stock, [] (reader& r) -> item_def
                                      {
                                          item_def ret;
                                          unmarshallItem(r, ret);
                                          return ret;
                                      });
}

void ShopInfo::save(writer& outf) const
{
    marshall_shop(outf, shop);
}

void ShopInfo::load(reader& inf)
{
    unmarshall_shop(inf, shop);
}

static void tag_construct_lost_monsters(writer &th)
{
    marshallMap(th, the_lost_ones, marshall_level_id,
                 marshall_follower_list);
}

static void tag_construct_companions(writer &th)
{
    marshallMap(th, companion_list, _marshall_as_int<mid_t>,
                 marshall_companion);
}

// Save versions 30-32.26 are readable but don't store the names.
static const char* old_species[]=
{
    "Human", "High Elf", "Deep Elf", "Sludge Elf", "Mountain Dwarf", "Halfling",
    "Hill Orc", "Kobold", "Mummy", "Naga", "Ogre", "Troll",
    "Red Draconian", "White Draconian", "Green Draconian", "Yellow Draconian",
    "Grey Draconian", "Black Draconian", "Purple Draconian", "Mottled Draconian",
    "Pale Draconian", "Draconian", "Centaur", "Demigod", "Spriggan", "Minotaur",
    "Demonspawn", "Ghoul", "Tengu", "Merfolk", "Vampire", "Deep Dwarf", "Felid",
    "Octopode",
};

static const char* old_gods[]=
{
    "", "Zin", "The Shining One", "Kikubaaqudgha", "Yredelemnul", "Xom",
    "Vehumet", "Okawaru", "Makhleb", "Sif Muna", "Trog", "Nemelex Xobeh",
    "Elyvilon", "Lugonu", "Beogh", "Jiyva", "Fedhas", "Cheibriados",
    "Ashenzari",
};

void tag_read_char(reader &th, uint8_t format, uint8_t major, uint8_t minor)
{
    // Important: values out of bounds are good here, the save browser needs to
    // be forward-compatible. We validate them only on an actual restore.
    you.your_name         = unmarshallString2(th);
    you.prev_save_version = unmarshallString2(th);
    dprf("Saved character %s, version: %s", you.your_name.c_str(),
                                            you.prev_save_version.c_str());

    you.species           = static_cast<species_type>(unmarshallUByte(th));
    you.char_class        = static_cast<job_type>(unmarshallUByte(th));
    you.experience_level  = unmarshallByte(th);
    you.chr_class_name    = unmarshallString2(th);
    you.religion          = static_cast<god_type>(unmarshallUByte(th));
    you.jiyva_second_name = unmarshallString2(th);

    you.wizard            = unmarshallBoolean(th);

    // this was mistakenly inserted in the middle for a few tag versions - this
    // just makes sure that games generated in that time period are still
    // readable, but should not be used for new games
#if TAG_CHR_FORMAT == 0
    // TAG_MINOR_EXPLORE_MODE and TAG_MINOR_FIX_EXPLORE_MODE
    if (major == 34 && (minor >= 121 && minor < 130))
        you.explore = unmarshallBoolean(th);
#endif

    crawl_state.type = (game_type) unmarshallUByte(th);
    if (crawl_state.game_is_tutorial())
        crawl_state.map = unmarshallString2(th);
    else
        crawl_state.map = "";

    if (major > 32 || major == 32 && minor > 26)
    {
        you.chr_species_name = unmarshallString2(th);
        you.chr_god_name     = unmarshallString2(th);
    }
    else
    {
        if (you.species >= 0 && you.species < (int)ARRAYSZ(old_species))
            you.chr_species_name = old_species[you.species];
        else
            you.chr_species_name = "Yak";
        if (you.religion >= 0 && you.religion < (int)ARRAYSZ(old_gods))
            you.chr_god_name = old_gods[you.religion];
        else
            you.chr_god_name = "Marduk";
    }

    if (major > 34 || major == 34 && minor >= 29)
        crawl_state.map = unmarshallString2(th);

    if (major > 34 || major == 34 && minor >= 130)
        you.explore = unmarshallBoolean(th);
}

static void _cap_mutation_at(mutation_type mut, int cap)
{
    if (you.mutation[mut] > cap)
    {
        // Don't convert real mutation levels to temporary.
        int real_levels = you.get_base_mutation_level(mut, true, false, true);
        you.temp_mutation[mut] = max(cap - real_levels, 0);

        you.mutation[mut] = cap;
    }
    if (you.innate_mutation[mut] > cap)
        you.innate_mutation[mut] = cap;
}

static void tag_read_you(reader &th)
{
    int count;

    ASSERT_RANGE(you.species, 0, NUM_SPECIES);
    ASSERT_RANGE(you.char_class, 0, NUM_JOBS);
    ASSERT_RANGE(you.experience_level, 1, 28);
    ASSERT(you.religion < NUM_GODS);
    ASSERT_RANGE(crawl_state.type, GAME_TYPE_UNSPECIFIED + 1, NUM_GAME_TYPE);
    you.last_mid          = unmarshallInt(th);
    you.piety             = unmarshallUByte(th);
    ASSERT(you.piety <= MAX_PIETY);
    you.pet_target        = unmarshallShort(th);

    you.max_level         = unmarshallByte(th);
    you.where_are_you     = static_cast<branch_type>(unmarshallUByte(th));
    ASSERT(you.where_are_you < NUM_BRANCHES);
    you.depth             = unmarshallByte(th);
    ASSERT(you.depth > 0);
    you.chapter           = static_cast<game_chapter>(unmarshallUByte(th));
    ASSERT(you.chapter < NUM_CHAPTERS);

    you.royal_jelly_dead = unmarshallBoolean(th);
    you.transform_uncancellable = unmarshallBoolean(th);

    you.berserk_penalty   = unmarshallByte(th);

    you.abyss_speed = unmarshallInt(th);

    you.disease         = unmarshallInt(th);
    you.hp              = unmarshallShort(th);
    you.hunger          = unmarshallShort(th);
    you.fishtail        = unmarshallBoolean(th);
    you.form            = unmarshall_int_as<transformation>(th);
    ASSERT_RANGE(static_cast<int>(you.form), 0, NUM_TRANSFORMS);
    ASSERT(you.form != transformation::none || !you.transform_uncancellable);
    EAT_CANARY;


    // How many you.equip?
    count = unmarshallByte(th);
    ASSERT(count <= NUM_EQUIP);
    for (int i = EQ_FIRST_EQUIP; i < count; ++i)
    {
        you.equip[i] = unmarshallByte(th);
        ASSERT_RANGE(you.equip[i], -1, ENDOFPACK);
    }
    for (int i = count; i < NUM_EQUIP; ++i)
        you.equip[i] = -1;
    for (int i = 0; i < count; ++i)
        you.melded.set(i, unmarshallBoolean(th));
    for (int i = count; i < NUM_EQUIP; ++i)
        you.melded.set(i, false);

    you.magic_points              = unmarshallUByte(th);
    you.max_magic_points          = unmarshallByte(th);

    for (int i = 0; i < NUM_STATS; ++i)
        you.base_stats[i] = unmarshallByte(th);

    for (int i = 0; i < NUM_STATS; ++i)
        you.stat_loss[i] = unmarshallByte(th);

    EAT_CANARY;

    you.hit_points_regeneration   = unmarshallInt(th);
    you.magic_points_regeneration = unmarshallInt(th);

    you.experience                = unmarshallInt(th);
    you.total_experience = unmarshallInt(th);
    you.gold                      = unmarshallInt(th);
    you.exp_available             = unmarshallInt(th);
    you.zigs_completed            = unmarshallInt(th);
    you.zig_max                   = unmarshallByte(th);
        you.banished_by           = unmarshallString(th);

    you.hp_max_adj_temp           = unmarshallShort(th);
    you.hp_max_adj_perm           = unmarshallShort(th);
    you.mp_max_adj                = unmarshallShort(th);

    const int x = unmarshallShort(th);
    const int y = unmarshallShort(th);
    // SIGHUP during Step from Time/etc is ok.
    ASSERT(!x && !y || in_bounds(x, y));
    you.moveto(coord_def(x, y));


    unmarshallFixedBitVector<NUM_SPELLS>(th, you.spell_library);
    unmarshallFixedBitVector<NUM_SPELLS>(th, you.hidden_spells);
    // how many spells?
    you.spell_no = 0;
    count = unmarshallUByte(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count && i < MAX_KNOWN_SPELLS; ++i)
    {
        you.spells[i] = unmarshallSpellType(th);
        if (you.spells[i] != SPELL_NO_SPELL)
            you.spell_no++;
    }
    for (int i = MAX_KNOWN_SPELLS; i < count; ++i)
    {
            unmarshallShort(th);
    }

    count = unmarshallByte(th);
    ASSERT(count == (int)you.spell_letter_table.size());
    for (int i = 0; i < count; i++)
    {
        int s = unmarshallByte(th);
        ASSERT_RANGE(s, -1, MAX_KNOWN_SPELLS);
        you.spell_letter_table[i] = s;
    }

    count = unmarshallByte(th);
    ASSERT(count == (int)you.ability_letter_table.size());
    for (int i = 0; i < count; i++)
    {
        int a = unmarshallShort(th);
        ASSERT_RANGE(a, ABIL_NON_ABILITY, NUM_ABILITIES);
        ASSERT(a != 0);
        you.ability_letter_table[i] = static_cast<ability_type>(a);
    }

        count = unmarshallUByte(th);
        for (int i = 0; i < count; ++i)
            you.old_vehumet_gifts.insert(unmarshallSpellType(th));

            count = unmarshallUByte(th);
            for (int i = 0; i < count; ++i)
                you.vehumet_gifts.insert(unmarshallSpellType(th));
    EAT_CANARY;

    // how many skills?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_SKILLS);
    for (int j = 0; j < count; ++j)
    {
        you.skills[j]          = unmarshallUByte(th);
        ASSERT(you.skills[j] <= 27 || you.wizard);

        you.train[j]    = (training_status)unmarshallByte(th);
        you.train_alt[j]    = (training_status)unmarshallByte(th);
        you.training[j] = unmarshallInt(th);
        you.skill_points[j]    = unmarshallInt(th);
        you.ct_skill_points[j] = unmarshallInt(th);
        you.skill_order[j]     = unmarshallByte(th);
            you.training_targets[j] = unmarshallInt(th);
    }

    you.auto_training = unmarshallBoolean(th);

    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.exercises.push_back((skill_type)unmarshallInt(th));

    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.exercises_all.push_back((skill_type)unmarshallInt(th));

    you.skill_menu_do = static_cast<skill_menu_state>(unmarshallByte(th));
    you.skill_menu_view = static_cast<skill_menu_state>(unmarshallByte(th));
    you.transfer_from_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_from_skill == SK_NONE || you.transfer_from_skill < NUM_SKILLS);
    you.transfer_to_skill = static_cast<skill_type>(unmarshallInt(th));
    ASSERT(you.transfer_to_skill == SK_NONE || you.transfer_to_skill < NUM_SKILLS);
    you.transfer_skill_points = unmarshallInt(th);
    you.transfer_total_skill_points = unmarshallInt(th);

    // Set up you.skill_cost_level.
    you.skill_cost_level = 0;
    check_skill_cost_change();

    EAT_CANARY;

    // how many durations?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_DURATIONS < 256);
    for (int j = 0; j < count && j < NUM_DURATIONS; ++j)
        you.duration[j] = unmarshallInt(th);
    for (int j = NUM_DURATIONS; j < count; ++j)
        unmarshallInt(th);

    // how many attributes?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_ATTRIBUTES < 256);
    for (int j = 0; j < count && j < NUM_ATTRIBUTES; ++j)
    {
        you.attribute[j] = unmarshallInt(th);
    }
    for (int j = count; j < NUM_ATTRIBUTES; ++j)
        you.attribute[j] = 0;
    for (int j = NUM_ATTRIBUTES; j < count; ++j)
        unmarshallInt(th);



    int timer_count = 0;
    timer_count = unmarshallByte(th);
    ASSERT(timer_count <= NUM_TIMERS);
    for (int j = 0; j < timer_count; ++j)
    {
        you.last_timer_effect[j] = unmarshallInt(th);
        you.next_timer_effect[j] = unmarshallInt(th);
    }
    // We'll have to fix up missing/broken timer entries after
    // we unmarshall you.elapsed_time.

    // how many mutations/demon powers?
    count = unmarshallShort(th);
    ASSERT_RANGE(count, 0, NUM_MUTATIONS + 1);
    for (int j = 0; j < count; ++j)
    {
        you.mutation[j]         = unmarshallUByte(th);
        you.innate_mutation[j]  = unmarshallUByte(th);
        you.temp_mutation[j]    = unmarshallUByte(th);
        you.sacrifices[j]       = unmarshallUByte(th);
    }


    // mutation fixups happen below here.
    // *REMINDER*: if you fix up an innate mutation, remember to adjust both
    // `you.mutation` and `you.innate_mutation`.


    for (int j = count; j < NUM_MUTATIONS; ++j)
        you.mutation[j] = you.innate_mutation[j] = you.sacrifices[j];


    count = unmarshallUByte(th);
    you.demonic_traits.clear();
    for (int j = 0; j < count; ++j)
    {
        player::demon_trait dt;
        dt.level_gained = unmarshallByte(th);
        ASSERT_RANGE(dt.level_gained, 1, 28);
        dt.mutation = static_cast<mutation_type>(unmarshallShort(th));
        ASSERT_RANGE(dt.mutation, 0, NUM_MUTATIONS);
        you.demonic_traits.push_back(dt);
    }

    {
        const int num_saved = unmarshallShort(th);

        you.sacrifice_piety.init(0);
        for (int j = 0; j < num_saved; ++j)
        {
            const int idx = ABIL_FIRST_SACRIFICE + j;
            const uint8_t val = unmarshallUByte(th);
            if (idx <= ABIL_FINAL_SACRIFICE)
                you.sacrifice_piety[idx] = val;
        }
    }

    EAT_CANARY;

    // how many penances?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_GODS);
    for (int i = 0; i < count; i++)
    {
        you.penance[i] = unmarshallUByte(th);
        ASSERT(you.penance[i] <= MAX_PENANCE);
    }


    for (int i = 0; i < count; i++)
        you.worshipped[i] = unmarshallByte(th);

    for (int i = 0; i < count; i++)
        you.num_current_gifts[i] = unmarshallShort(th);
    for (int i = 0; i < count; i++)
        you.num_total_gifts[i] = unmarshallShort(th);
    for (int i = 0; i < count; i++)
        you.one_time_ability_used.set(i, unmarshallBoolean(th));
    for (int i = 0; i < count; i++)
        you.piety_max[i] = unmarshallByte(th);

    you.gift_timeout   = unmarshallByte(th);
    you.saved_good_god_piety = unmarshallUByte(th);
    you.previous_good_god = static_cast<god_type>(unmarshallByte(th));


    for (int i = 0; i < count; i++)
        you.exp_docked[i] = unmarshallInt(th);
    for (int i = 0; i < count; i++)
        you.exp_docked_total[i] = unmarshallInt(th);

    // elapsed time
    you.elapsed_time   = unmarshallInt(th);
    you.elapsed_time_at_last_input = you.elapsed_time;

    // Initialize new timers now that we know the time.
    const int last_20_turns = you.elapsed_time - (you.elapsed_time % 200);
    for (int j = timer_count; j < NUM_TIMERS; ++j)
    {
        you.last_timer_effect[j] = last_20_turns;
        you.next_timer_effect[j] = last_20_turns + 200;
    }

    // Verify that timers aren't scheduled for the past.
    for (int j = 0; j < NUM_TIMERS; ++j)
    {
        if (you.next_timer_effect[j] < you.elapsed_time)
        {
            die("Timer %d next trigger in the past [%d < %d]",
                j, you.next_timer_effect[j], you.elapsed_time);
        }
    }

    // time of character creation
    you.birth_time = unmarshallInt(th);

    const int real_time  = unmarshallInt(th);
    you.real_time_ms = chrono::milliseconds(real_time * 1000);
    you.num_turns  = unmarshallInt(th);
    you.exploration = unmarshallInt(th);

        you.magic_contamination = unmarshallInt(th);

    you.transit_stair  = unmarshallFeatureType(th);
    you.entering_level = unmarshallByte(th);
        you.travel_ally_pace = unmarshallBoolean(th);

    you.deaths = unmarshallByte(th);
    you.lives = unmarshallByte(th);


    you.pending_revival = !you.hp;

    EAT_CANARY;

    int n_dact = unmarshallInt(th);
    ASSERT_RANGE(n_dact, 0, 100000); // arbitrary, sanity check
    you.dactions.resize(n_dact, NUM_DACTIONS);
    for (int i = 0; i < n_dact; i++)
    {
        int a = unmarshallUByte(th);
        ASSERT(a < NUM_DACTIONS);
        you.dactions[i] = static_cast<daction_type>(a);
    }

    you.level_stack.clear();
    int n_levs = unmarshallInt(th);
    for (int k = 0; k < n_levs; k++)
    {
        level_pos pos;
        pos.load(th);
        you.level_stack.push_back(pos);
    }

    // List of currently beholding monsters (usually empty).
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count; i++)
    {
        you.beholders.push_back(unmarshall_int_as<mid_t>(th));
    }

    // Also usually empty.
    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count; i++)
    {
        you.fearmongers.push_back(unmarshall_int_as<mid_t>(th));
    }

    you.piety_hysteresis = unmarshallByte(th);

    you.m_quiver.load(th);


    EAT_CANARY;


    // Counts of actions made, by type.
    count = unmarshallShort(th);
    for (int i = 0; i < count; i++)
    {
        caction_type caction = (caction_type)unmarshallShort(th);
        int subtype = unmarshallInt(th);
        for (int j = 0; j < 27; j++)
            you.action_count[make_pair(caction, subtype)][j] = unmarshallInt(th);
    }

    count = unmarshallByte(th);
    for (int i = 0; i < count; i++)
        you.branches_left.set(i, unmarshallBoolean(th));

    abyssal_state.major_coord = unmarshallCoord(th);
            abyssal_state.seed = unmarshallInt(th);
        abyssal_state.depth = unmarshallInt(th);
        abyssal_state.destroy_all_terrain = false;
    abyssal_state.phase = unmarshallFloat(th);


    abyssal_state.level = unmarshall_level_id(th);
    _unmarshall_constriction(th, &you);

    you.octopus_king_rings = unmarshallUByte(th);

    count = unmarshallUnsigned(th);
    ASSERT_RANGE(count, 0, 16); // sanity check
    you.uncancel.resize(count);
    for (int i = 0; i < count; i++)
    {
        you.uncancel[i].first = (uncancellable_type)unmarshallUByte(th);
        you.uncancel[i].second = unmarshallInt(th);
    }
    count = unmarshallUnsigned(th);
    you.recall_list.resize(count);
    for (int i = 0; i < count; i++)
        you.recall_list[i] = unmarshall_int_as<mid_t>(th);
    count = unmarshallUByte(th);
    you.game_seed = count > 0 ? unmarshallInt(th) : get_uint32();
    for (int i = 1; i < count; i++)
        unmarshallInt(th);

    EAT_CANARY;

    if (!dlua.callfn("dgn_load_data", "u", &th))
    {
        mprf(MSGCH_ERROR, "Failed to load Lua persist table: %s",
             dlua.error.c_str());
    }

    EAT_CANARY;

    crawl_state.save_rcs_version = unmarshallString(th);

    you.props.clear();
    you.props.read(th);
}

static void tag_read_you_items(reader &th)
{
    int count, count2;

    // how many inventory slots?
    count = unmarshallByte(th);
    ASSERT(count == ENDOFPACK); // not supposed to change
    for (int i = 0; i < count; ++i)
    {
        item_def &it = you.inv[i];
        unmarshallItem(th, it);
    }

    // Initialize cache of equipped unrand functions
    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
    {
        const item_def *item = you.slot_item(static_cast<equipment_type>(i));

        if (item && is_unrandom_artefact(*item))
        {

            const unrandart_entry *entry = get_unrand_entry(item->unrand_idx);

            if (entry->world_reacts_func)
                you.unrand_reacts.set(i);
        }
    }

    unmarshallFixedBitVector<NUM_RUNE_TYPES>(th, you.runes);
    you.obtainable_runes = unmarshallByte(th);

    // Item descrip for each type & subtype.
    // how many types?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_IDESC);
    // how many subtypes?
    count2 = unmarshallUByte(th);
    ASSERT(count2 <= MAX_SUBTYPES);
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < count2; ++j)
                you.item_description[i][j] = unmarshallInt(th);
    for (int i = 0; i < count; ++i)
        for (int j = count2; j < MAX_SUBTYPES; ++j)
            you.item_description[i][j] = 0;
    int iclasses = unmarshallUByte(th);
    ASSERT(iclasses <= NUM_OBJECT_CLASSES);

    // Identification status.
    for (int i = 0; i < iclasses; ++i)
    {
        if (!item_type_has_ids((object_class_type)i))
            continue;
        for (int j = 0; j < count2; ++j)
        {
            you.type_ids[i][j] = unmarshallBoolean(th);
        }
        for (int j = count2; j < MAX_SUBTYPES; ++j)
            you.type_ids[i][j] = false;
    }


    EAT_CANARY;

    // how many unique items?
    count = unmarshallUByte(th);
    COMPILE_CHECK(NUM_UNRANDARTS <= 256);
    for (int j = 0; j < count && j < NUM_UNRANDARTS; ++j)
    {
        you.unique_items[j] =
            static_cast<unique_item_status_type>(unmarshallByte(th));
    }
    // # of unrandarts could certainly change.
    // If it does, the new ones won't exist yet - zero them out.
    for (int j = count; j < NUM_UNRANDARTS; j++)
        you.unique_items[j] = UNIQ_NOT_EXISTS;
    for (int j = NUM_UNRANDARTS; j < count; j++)
        unmarshallByte(th);


    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int j = 0; j < count && j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = unmarshallInt(th);
    for (int j = count; j < NUM_WEAPONS; ++j)
        you.seen_weapon[j] = 0;
    for (int j = NUM_WEAPONS; j < count; ++j)
        unmarshallInt(th);

    count = unmarshallShort(th);
    ASSERT(count >= 0);
    for (int j = 0; j < count && j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = unmarshallInt(th);
    for (int j = count; j < NUM_ARMOURS; ++j)
        you.seen_armour[j] = 0;
    for (int j = NUM_ARMOURS; j < count; ++j)
        unmarshallInt(th);
    unmarshallFixedBitVector<NUM_MISCELLANY>(th, you.seen_misc);

    for (int i = 0; i < iclasses; i++)
        for (int j = 0; j < count2; j++)
            you.force_autopickup[i][j] = unmarshallInt(th);
}

static PlaceInfo unmarshallPlaceInfo(reader &th)
{
    PlaceInfo place_info;

    place_info.branch      = static_cast<branch_type>(unmarshallInt(th));

    place_info.num_visits  = unmarshallInt(th);
    place_info.levels_seen = unmarshallInt(th);

    place_info.mon_kill_exp       = unmarshallInt(th);

    for (int i = 0; i < KC_NCATEGORIES; i++)
        place_info.mon_kill_num[i] = unmarshallInt(th);

    place_info.turns_total      = unmarshallInt(th);
    place_info.turns_explore    = unmarshallInt(th);
    place_info.turns_travel     = unmarshallInt(th);
    place_info.turns_interlevel = unmarshallInt(th);
    place_info.turns_resting    = unmarshallInt(th);
    place_info.turns_other      = unmarshallInt(th);

    place_info.elapsed_total      = unmarshallInt(th);
    place_info.elapsed_explore    = unmarshallInt(th);
    place_info.elapsed_travel     = unmarshallInt(th);
    place_info.elapsed_interlevel = unmarshallInt(th);
    place_info.elapsed_resting    = unmarshallInt(th);
    place_info.elapsed_other      = unmarshallInt(th);

    return place_info;
}

static LevelXPInfo unmarshallLevelXPInfo(reader &th)
{
    LevelXPInfo xp_info;

    xp_info.level = unmarshall_level_id(th);

    xp_info.non_vault_xp    = unmarshallInt(th);
    xp_info.non_vault_count = unmarshallInt(th);
    xp_info.vault_xp        = unmarshallInt(th);
    xp_info.vault_count     = unmarshallInt(th);

    return xp_info;
}


static void tag_read_you_dungeon(reader &th)
{
    // how many unique creatures?
    int count = unmarshallShort(th);
    you.unique_creatures.reset();
    for (int j = 0; j < count; ++j)
    {
        const bool created = unmarshallBoolean(th);

        if (j < NUM_MONSTERS)
            you.unique_creatures.set(j, created);
    }

    // how many branches?
    count = unmarshallUByte(th);
    ASSERT(count <= NUM_BRANCHES);
    for (int j = 0; j < count; ++j)
    {
        brdepth[j]    = unmarshallInt(th);
        ASSERT_RANGE(brdepth[j], -1, MAX_BRANCH_DEPTH + 1);
        brentry[j]    = unmarshall_level_id(th);
        branch_bribe[j] = unmarshallInt(th);
    }
    // Initialize data for any branches added after this save version.
    for (int j = count; j < NUM_BRANCHES; ++j)
    {
        brdepth[j] = branches[j].numlevels;
        brentry[j] = level_id(branches[j].parent_branch, branches[j].mindepth);
        branch_bribe[j] = 0;
    }


    ASSERT(you.depth <= brdepth[you.where_are_you]);

    // Root of the dungeon; usually BRANCH_DUNGEON.
    root_branch = static_cast<branch_type>(unmarshallInt(th));

    unmarshallMap(th, stair_level,
                  unmarshall_int_as<branch_type>,
                  _unmarshall_level_id_set);
    unmarshallMap(th, shops_present,
                  _unmarshall_level_pos, unmarshall_int_as<shop_type>);
    unmarshallMap(th, altars_present,
                  _unmarshall_level_pos, unmarshall_int_as<god_type>);
    unmarshallMap(th, portals_present,
                  _unmarshall_level_pos, unmarshall_int_as<branch_type>);
    unmarshallMap(th, portal_notes,
                  _unmarshall_level_pos, unmarshallString);
    unmarshallMap(th, level_annotations,
                  unmarshall_level_id, unmarshallString);
    unmarshallMap(th, level_exclusions,
                  unmarshall_level_id, unmarshallString);
    unmarshallMap(th, level_uniques,
                  unmarshall_level_id, unmarshallString);
    unmarshallUniqueAnnotations(th);

    PlaceInfo place_info = unmarshallPlaceInfo(th);
    ASSERT(place_info.is_global());
    you.set_place_info(place_info);

    unsigned short count_p = (unsigned short) unmarshallShort(th);

    auto places = you.get_all_place_info();
    // Use "<=" so that adding more branches or non-dungeon places
    // won't break save-file compatibility.
    ASSERT(count_p <= places.size());

    for (int i = 0; i < count_p; i++)
    {
        place_info = unmarshallPlaceInfo(th);
        if (place_info.is_global()
            && th.getMinorVersion() <= TAG_MINOR_DESOLATION_GLOBAL)
        {
            // This is to fix some crashing saves that didn't import
            // correctly, where the desolation slot occasionally gets marked
            // as global on conversion from pre-0.19 to post-0.19a.   This
            // assumes that the order in `logical_branch_order` (branch.cc)
            // hasn't changed since the save version (which is moderately safe).
            const branch_type branch_to_fix = places[i].branch;
            ASSERT(branch_to_fix == BRANCH_DESOLATION);
            place_info.branch = branch_to_fix;
        }
        else
        {
            // These should all be branch-specific, not global
            ASSERT(!place_info.is_global());
        }
        you.set_place_info(place_info);
    }


    auto xp_info = unmarshallLevelXPInfo(th);
    ASSERT(xp_info.is_global());
    you.set_level_xp_info(xp_info);

    count_p = (unsigned short) unmarshallShort(th);
    for (int i = 0; i < count_p; i++)
    {
        xp_info = unmarshallLevelXPInfo(th);
        ASSERT(!xp_info.is_global());
        you.set_level_xp_info(xp_info);
    }

    typedef pair<string_set::iterator, bool> ssipair;
    unmarshall_container(th, you.uniq_map_tags,
                         (ssipair (string_set::*)(const string &))
                         &string_set::insert,
                         unmarshallString);
    unmarshall_container(th, you.uniq_map_names,
                         (ssipair (string_set::*)(const string &))
                         &string_set::insert,
                         unmarshallString);
    unmarshallMap(th, you.vault_list, unmarshall_level_id,
                  unmarshallStringVector);

    read_level_connectivity(th);
}

static void tag_read_lost_monsters(reader &th)
{
    the_lost_ones.clear();
    unmarshallMap(th, the_lost_ones,
                  unmarshall_level_id, unmarshall_follower_list);
}


static void tag_read_companions(reader &th)
{
    companion_list.clear();

    unmarshallMap(th, companion_list, unmarshall_int_as<mid_t>,
                  unmarshall_companion);
}

template <typename Z>
static int _last_used_index(const Z &thinglist, int max_things)
{
    for (int i = max_things - 1; i >= 0; --i)
        if (thinglist[i].defined())
            return i + 1;
    return 0;
}

// ------------------------------- level tags ---------------------------- //

static void tag_construct_level(writer &th)
{
    marshallByte(th, env.floor_colour);
    marshallByte(th, env.rock_colour);

    marshallInt(th, you.on_current_level ? you.elapsed_time : env.elapsed_time);
    marshallCoord(th, you.pos());

    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallInt(th, env.turns_on_level);

    CANARY;

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallByte(th, grd[count_x][count_y]);
            marshallMapCell(th, env.map_knowledge[count_x][count_y]);
            marshallInt(th, env.pgrid[count_x][count_y].flags);
        }

    marshallBoolean(th, !!env.map_forgotten);
    if (env.map_forgotten)
        for (int x = 0; x < GXM; x++)
            for (int y = 0; y < GYM; y++)
                marshallMapCell(th, (*env.map_forgotten)[x][y]);

    _run_length_encode(th, marshallByte, env.grid_colours, GXM, GYM);

    CANARY;

    // how many clouds?
    marshallShort(th, env.cloud.size());
    for (const auto& entry : env.cloud)
    {
        const cloud_struct& cloud = entry.second;
        marshallByte(th, cloud.type);
        ASSERT(cloud.type != CLOUD_NONE);
        ASSERT_IN_BOUNDS(cloud.pos);
        marshallByte(th, cloud.pos.x);
        marshallByte(th, cloud.pos.y);
        marshallShort(th, cloud.decay);
        marshallByte(th, cloud.spread_rate);
        marshallByte(th, cloud.whose);
        marshallByte(th, cloud.killer);
        marshallInt(th, cloud.source);
        marshallInt(th, cloud.excl_rad);
    }

    CANARY;

    // how many shops?
    marshallShort(th, env.shop.size());
    for (const auto& entry : env.shop)
        marshall_shop(th, entry.second);

    CANARY;

    marshallCoord(th, env.sanctuary_pos);
    marshallByte(th, env.sanctuary_time);

    marshallInt(th, env.spawn_random_rate);

    env.markers.write(th);
    env.properties.write(th);

    marshallInt(th, you.dactions.size());

    // Save heightmap, if present.
    marshallByte(th, !!env.heightmap);
    if (env.heightmap)
    {
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            marshallShort(th, heightmap(*ri));
    }

    CANARY;

    marshallInt(th, env.forest_awoken_until);
    marshall_level_vault_data(th);
    marshallInt(th, env.density);

    marshallShort(th, env.sunlight.size());
    for (const auto &sunspot : env.sunlight)
    {
        marshallCoord(th, sunspot.first);
        marshallInt(th, sunspot.second);
    }
}

void marshallItem(writer &th, const item_def &item, bool iinfo)
{
    marshallByte(th, item.base_type);
    if (item.base_type == OBJ_UNASSIGNED)
        return;

    ASSERT(item.is_valid(iinfo));

    marshallByte(th, item.sub_type);
    marshallShort(th, item.plus);
    marshallShort(th, item.plus2);
    marshallInt(th, item.special);
    marshallShort(th, item.quantity);

    marshallByte(th, item.rnd);
    marshallShort(th, item.pos.x);
    marshallShort(th, item.pos.y);
    marshallInt(th, item.flags);

    marshallShort(th, item.link);
    if (item.pos.x >= 0 && item.pos.y >= 0)
        marshallShort(th, igrd(item.pos));  //  unused
    else
        marshallShort(th, -1); // unused

    marshallByte(th, item.slot);

    item.orig_place.save(th);
    marshallShort(th, item.orig_monnum);
    marshallString(th, item.inscription);

    item.props.write(th);
}


void unmarshallItem(reader &th, item_def &item)
{
    item.base_type   = static_cast<object_class_type>(unmarshallByte(th));
    if (item.base_type == OBJ_UNASSIGNED)
        return;
    item.sub_type    = unmarshallUByte(th);
    item.plus        = unmarshallShort(th);
    item.plus2       = unmarshallShort(th);
    item.special     = unmarshallInt(th);
    item.quantity    = unmarshallShort(th);

    item.rnd          = unmarshallUByte(th);

    item.pos.x       = unmarshallShort(th);
    item.pos.y       = unmarshallShort(th);
    item.flags       = unmarshallInt(th);
    item.link        = unmarshallShort(th);

    unmarshallShort(th);  // igrd[item.x][item.y] -- unused

    item.slot        = unmarshallByte(th);

    item.orig_place.load(th);

    item.orig_monnum = unmarshallShort(th);
    item.inscription = unmarshallString(th);

    item.props.clear();
    item.props.read(th);
    // Fixup artefact props to handle reloading items when the new version
    // of Crawl has more artefact props.
    if (is_artefact(item))
        artefact_fixup_props(item);


    if (is_unrandom_artefact(item))
        setup_unrandart(item, false);

    bind_item_tile(item);
}

#define MAP_SERIALIZE_FLAGS_MASK 3
#define MAP_SERIALIZE_FLAGS_8 1
#define MAP_SERIALIZE_FLAGS_16 2
#define MAP_SERIALIZE_FLAGS_32 3

#define MAP_SERIALIZE_FEATURE 4
#define MAP_SERIALIZE_FEATURE_COLOUR 8
#define MAP_SERIALIZE_ITEM 0x10
#define MAP_SERIALIZE_CLOUD 0x20
#define MAP_SERIALIZE_MONSTER 0x40

void marshallMapCell(writer &th, const map_cell &cell)
{
    unsigned flags = 0;

    if (cell.flags > 0xffff)
        flags |= MAP_SERIALIZE_FLAGS_32;
    else if (cell.flags > 0xff)
        flags |= MAP_SERIALIZE_FLAGS_16;
    else if (cell.flags)
        flags |= MAP_SERIALIZE_FLAGS_8;

    if (cell.feat() != DNGN_UNSEEN)
        flags |= MAP_SERIALIZE_FEATURE;

    if (cell.feat_colour())
        flags |= MAP_SERIALIZE_FEATURE_COLOUR;

    if (cell.cloud() != CLOUD_NONE)
        flags |= MAP_SERIALIZE_CLOUD;

    if (cell.item())
        flags |= MAP_SERIALIZE_ITEM;

    if (cell.monster() != MONS_NO_MONSTER)
        flags |= MAP_SERIALIZE_MONSTER;

    marshallUnsigned(th, flags);

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        marshallByte(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        marshallShort(th, cell.flags);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        marshallInt(th, cell.flags);
        break;
    }

    if (flags & MAP_SERIALIZE_FEATURE)
        marshallUByte(th, cell.feat());

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        marshallUnsigned(th, cell.feat_colour());

    if (feat_is_trap(cell.feat()))
        marshallByte(th, cell.trap());

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info* ci = cell.cloudinfo();
        marshallUnsigned(th, ci->type);
        marshallUnsigned(th, ci->colour);
        marshallUnsigned(th, ci->duration);
        marshallShort(th, ci->tile);
        marshallUByte(th, ci->killer);
    }

    if (flags & MAP_SERIALIZE_ITEM)
        marshallItem(th, *cell.item(), true);

    if (flags & MAP_SERIALIZE_MONSTER)
        marshallMonsterInfo(th, *cell.monsterinfo());
}

void unmarshallMapCell(reader &th, map_cell& cell)
{
    unsigned flags = unmarshallUnsigned(th);
    unsigned cell_flags = 0;
    trap_type trap = TRAP_UNASSIGNED;

    cell.clear();

    switch (flags & MAP_SERIALIZE_FLAGS_MASK)
    {
    case MAP_SERIALIZE_FLAGS_8:
        cell_flags = unmarshallByte(th);
        break;
    case MAP_SERIALIZE_FLAGS_16:
        cell_flags = unmarshallShort(th);
        break;
    case MAP_SERIALIZE_FLAGS_32:
        cell_flags = unmarshallInt(th);
        break;
    }

    dungeon_feature_type feature = DNGN_UNSEEN;
    unsigned feat_colour = 0;

    if (flags & MAP_SERIALIZE_FEATURE)
        feature = unmarshallFeatureType(th);

    if (flags & MAP_SERIALIZE_FEATURE_COLOUR)
        feat_colour = unmarshallUnsigned(th);

    if (feat_is_trap(feature))
    {
        trap = (trap_type)unmarshallByte(th);
    }

    cell.set_feature(feature, feat_colour, trap);

    if (flags & MAP_SERIALIZE_CLOUD)
    {
        cloud_info ci;
        ci.type = (cloud_type)unmarshallUnsigned(th);
        unmarshallUnsigned(th, ci.colour);
        unmarshallUnsigned(th, ci.duration);
        ci.tile = unmarshallShort(th);
        ci.killer = static_cast<killer_type>(unmarshallUByte(th));
        cell.set_cloud(ci);
    }

    if (flags & MAP_SERIALIZE_ITEM)
    {
        item_def item;
        unmarshallItem(th, item);
        cell.set_item(item, false);
    }

    if (flags & MAP_SERIALIZE_MONSTER)
    {
        monster_info mi;
        unmarshallMonsterInfo(th, mi);
        cell.set_monster(mi);
    }

    // set this last so the other sets don't override this
    cell.flags = cell_flags;
}

static void tag_construct_level_items(writer &th)
{
    // how many traps?
    marshallShort(th, env.trap.size());
    for (const auto& entry : env.trap)
    {
        const trap_def& trap = entry.second;
        marshallByte(th, trap.type);
        marshallCoord(th, trap.pos);
        marshallShort(th, trap.ammo_qty);
        marshallUByte(th, trap.skill_rnd);
    }

    // how many items?
    const int ni = _last_used_index(mitm, MAX_ITEMS);
    marshallShort(th, ni);
    for (int i = 0; i < ni; ++i)
        marshallItem(th, mitm[i]);
}

static void marshall_mon_enchant(writer &th, const mon_enchant &me)
{
    marshallShort(th, me.ench);
    marshallShort(th, me.degree);
    marshallShort(th, me.who);
    marshallInt(th, me.source);
    marshallShort(th, min(me.duration, INFINITE_DURATION));
    marshallShort(th, min(me.maxduration, INFINITE_DURATION));
}

static mon_enchant unmarshall_mon_enchant(reader &th)
{
    mon_enchant me;
    me.ench        = static_cast<enchant_type>(unmarshallShort(th));
    me.degree      = unmarshallShort(th);
    me.who         = static_cast<kill_category>(unmarshallShort(th));
    me.source      = unmarshallInt(th);
    me.duration    = unmarshallShort(th);
    me.maxduration = unmarshallShort(th);
    return me;
}

enum mon_part_t
{
    MP_GHOST_DEMON      = BIT(0),
    MP_CONSTRICTION     = BIT(1),
    MP_ITEMS            = BIT(2),
    MP_SPELLS           = BIT(3),
};

void marshallMonster(writer &th, const monster& m)
{
    if (!m.alive())
    {
        marshallShort(th, MONS_NO_MONSTER);
        return;
    }

    uint32_t parts = 0;
    if (mons_is_ghost_demon(m.type))
        parts |= MP_GHOST_DEMON;
    if (m.is_constricted() || m.is_constricting())
        parts |= MP_CONSTRICTION;
    for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        if (m.inv[i] != NON_ITEM)
            parts |= MP_ITEMS;
    if (m.spells.size() > 0)
        parts |= MP_SPELLS;

    marshallShort(th, m.type);
    marshallUnsigned(th, parts);
    ASSERT(m.mid > 0);
    marshallInt(th, m.mid);
    marshallString(th, m.mname);
    marshallByte(th, m.xp_tracking);
    marshallByte(th, m.get_experience_level());
    marshallByte(th, m.speed);
    marshallByte(th, m.speed_increment);
    marshallByte(th, m.behaviour);
    marshallByte(th, m.pos().x);
    marshallByte(th, m.pos().y);
    marshallByte(th, m.target.x);
    marshallByte(th, m.target.y);
    marshallCoord(th, m.firing_pos);
    marshallCoord(th, m.patrol_point);
    int help = m.travel_target;
    marshallByte(th, help);

    marshallShort(th, m.travel_path.size());
    for (coord_def pos : m.travel_path)
        marshallCoord(th, pos);

    marshallUnsigned(th, m.flags.flags);
    marshallInt(th, m.experience);

    marshallShort(th, m.enchantments.size());
    for (const auto &entry : m.enchantments)
        marshall_mon_enchant(th, entry.second);
    marshallByte(th, m.ench_countdown);

    marshallShort(th, min(m.hit_points, MAX_MONSTER_HP));
    marshallShort(th, min(m.max_hit_points, MAX_MONSTER_HP));
    marshallInt(th, m.number);
    marshallShort(th, m.base_monster);
    marshallShort(th, m.colour);
    marshallInt(th, m.summoner);

    if (parts & MP_ITEMS)
        for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
            marshallShort(th, m.inv[j]);
    if (parts & MP_SPELLS)
        marshallSpells(th, m.spells);
    marshallByte(th, m.god);
    marshallByte(th, m.attitude);
    marshallShort(th, m.foe);
    marshallInt(th, m.foe_memory);
    marshallShort(th, m.damage_friendly);
    marshallShort(th, m.damage_total);
    marshallByte(th, m.went_unseen_this_turn);
    marshallCoord(th, m.unseen_pos);

    if (parts & MP_GHOST_DEMON)
    {
        // *Must* have ghost field set.
        ASSERT(m.ghost);
        marshallGhost(th, *m.ghost);
    }

    if (parts & MP_CONSTRICTION)
        _marshall_constriction(th, &m);

    m.props.write(th);
}

static void _marshall_mi_attack(writer &th, const mon_attack_def &attk)
{
    marshallInt(th, attk.type);
    marshallInt(th, attk.flavour);
    marshallInt(th, attk.damage);
}

static mon_attack_def _unmarshall_mi_attack(reader &th)
{
    mon_attack_def attk;
    attk.type = static_cast<attack_type>(unmarshallInt(th));
    attk.flavour = static_cast<attack_flavour>(unmarshallInt(th));
    attk.damage = unmarshallInt(th);

    return attk;
}

void marshallMonsterInfo(writer &th, const monster_info& mi)
{
    marshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    marshallString(th, mi.mname);
    marshallShort(th, mi.type);
    marshallShort(th, mi.base_type);
    marshallUnsigned(th, mi.number);
    marshallInt(th, mi._colour);
    marshallUnsigned(th, mi.attitude);
    marshallUnsigned(th, mi.threat);
    marshallUnsigned(th, mi.dam);
    marshallUnsigned(th, mi.fire_blocker);
    marshallString(th, mi.description);
    marshallString(th, mi.quote);
    marshallUnsigned(th, mi.holi.flags);
    marshallUnsigned(th, mi.mintel);
    marshallUnsigned(th, mi.hd);
    marshallUnsigned(th, mi.ac);
    marshallUnsigned(th, mi.ev);
    marshallUnsigned(th, mi.base_ev);
    marshallInt(th, mi.mresists);
    marshallUnsigned(th, mi.mitemuse);
    marshallByte(th, mi.mbase_speed);
    marshallByte(th, mi.menergy.move);
    marshallByte(th, mi.menergy.swim);
    marshallByte(th, mi.menergy.attack);
    marshallByte(th, mi.menergy.missile);
    marshallByte(th, mi.menergy.spell);
    marshallByte(th, mi.menergy.special);
    marshallByte(th, mi.menergy.item);
    marshallByte(th, mi.menergy.pickup_percent);
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
        _marshall_mi_attack(th, mi.attack[i]);
    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (mi.inv[i])
        {
            marshallBoolean(th, true);
            marshallItem(th, *mi.inv[i], true);
        }
        else
            marshallBoolean(th, false);
    }
    if (mons_is_pghost(mi.type))
    {
        marshallUnsigned(th, mi.i_ghost.species);
        marshallUnsigned(th, mi.i_ghost.job);
        marshallUnsigned(th, mi.i_ghost.religion);
        marshallUnsigned(th, mi.i_ghost.best_skill);
        marshallShort(th, mi.i_ghost.best_skill_rank);
        marshallShort(th, mi.i_ghost.xl_rank);
        marshallShort(th, mi.i_ghost.damage);
        marshallShort(th, mi.i_ghost.ac);
    }

    mi.props.write(th);
}

void unmarshallMonsterInfo(reader &th, monster_info& mi)
{
    unmarshallFixedBitVector<NUM_MB_FLAGS>(th, mi.mb);
    mi.mname = unmarshallString(th);
    mi.type = unmarshallMonType(th);
    ASSERT(!invalid_monster_type(mi.type));
    mi.base_type = unmarshallMonType(th);
    unmarshallUnsigned(th, mi.number);
        mi._colour = unmarshallInt(th);
    unmarshallUnsigned(th, mi.attitude);
    unmarshallUnsigned(th, mi.threat);
    unmarshallUnsigned(th, mi.dam);
    unmarshallUnsigned(th, mi.fire_blocker);
    mi.description = unmarshallString(th);
    mi.quote = unmarshallString(th);

    uint64_t holi_flags = unmarshallUnsigned(th);
        mi.holi.flags = holi_flags;


    unmarshallUnsigned(th, mi.mintel);

        unmarshallUnsigned(th, mi.hd);
        unmarshallUnsigned(th, mi.ac);
        unmarshallUnsigned(th, mi.ev);
        unmarshallUnsigned(th, mi.base_ev);

    mi.mr = mons_class_res_magic(mi.type, mi.base_type);
    mi.can_see_invis = mons_class_sees_invis(mi.type, mi.base_type);

    mi.mresists = unmarshallInt(th);
    unmarshallUnsigned(th, mi.mitemuse);
    mi.mbase_speed = unmarshallByte(th);

    mi.menergy.move = unmarshallByte(th);
    mi.menergy.swim = unmarshallByte(th);
    mi.menergy.attack = unmarshallByte(th);
    mi.menergy.missile = unmarshallByte(th);
    mi.menergy.spell = unmarshallByte(th);
    mi.menergy.special = unmarshallByte(th);
    mi.menergy.item = unmarshallByte(th);
    mi.menergy.pickup_percent = unmarshallByte(th);

    // Some TAG_MAJOR_VERSION == 34 saves suffered data loss here, beware.
    // Should be harmless, hopefully.
    for (int i = 0; i < MAX_NUM_ATTACKS; ++i)
        mi.attack[i] = _unmarshall_mi_attack(th);

    for (unsigned int i = 0; i <= MSLOT_LAST_VISIBLE_SLOT; ++i)
    {
        if (unmarshallBoolean(th))
        {
            mi.inv[i].reset(new item_def());
            unmarshallItem(th, *mi.inv[i]);
        }
    }

    if (mons_is_pghost(mi.type))
    {
        unmarshallUnsigned(th, mi.i_ghost.species);
        unmarshallUnsigned(th, mi.i_ghost.job);
        unmarshallUnsigned(th, mi.i_ghost.religion);
        unmarshallUnsigned(th, mi.i_ghost.best_skill);
        mi.i_ghost.best_skill_rank = unmarshallShort(th);
        mi.i_ghost.xl_rank = unmarshallShort(th);
        mi.i_ghost.damage = unmarshallShort(th);
        mi.i_ghost.ac = unmarshallShort(th);
    }

    mi.props.clear();
    mi.props.read(th);


    if (mons_is_removed(mi.type))
    {
        mi.type = MONS_GHOST;
        mi.props.clear();
    }
}

static void tag_construct_level_monsters(writer &th)
{
    int nm = 0;
    for (int i = 0; i < MAX_MONS_ALLOC; ++i)
        if (env.mons_alloc[i] != MONS_NO_MONSTER)
            nm = i + 1;

    // how many mons_alloc?
    marshallByte(th, nm);
    for (int i = 0; i < nm; ++i)
        marshallShort(th, env.mons_alloc[i]);

    // how many monsters?
    nm = _last_used_index(menv, MAX_MONSTERS);
    marshallShort(th, nm);

    for (int i = 0; i < nm; i++)
    {
        monster& m(menv[i]);

#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
        if (m.type != MONS_NO_MONSTER)
        {
            if (invalid_monster_type(m.type))
            {
                mprf(MSGCH_ERROR, "Marshalled monster #%d %s",
                     i, m.name(DESC_PLAIN, true).c_str());
            }
            if (!in_bounds(m.pos()))
            {
                mprf(MSGCH_ERROR,
                     "Marshalled monster #%d %s out of bounds at (%d, %d)",
                     i, m.name(DESC_PLAIN, true).c_str(),
                     m.pos().x, m.pos().y);
            }
        }
#endif
        marshallMonster(th, m);
    }
}

void tag_construct_level_tiles(writer &th)
{
    // Map grids.
    // how many X?
    marshallShort(th, GXM);
    // how many Y?
    marshallShort(th, GYM);

    marshallShort(th, env.tile_names.size());
    for (const string &name : env.tile_names)
    {
        marshallString(th, name);
#ifdef DEBUG_TILE_NAMES
        mprf("Writing '%s' into save.", name.c_str());
#endif
    }

    // flavour
    marshallShort(th, env.tile_default.wall_idx);
    marshallShort(th, env.tile_default.floor_idx);

    marshallShort(th, env.tile_default.wall);
    marshallShort(th, env.tile_default.floor);
    marshallShort(th, env.tile_default.special);

    for (int count_x = 0; count_x < GXM; count_x++)
        for (int count_y = 0; count_y < GYM; count_y++)
        {
            marshallShort(th, env.tile_flv[count_x][count_y].wall_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].floor_idx);
            marshallShort(th, env.tile_flv[count_x][count_y].feat_idx);

            marshallShort(th, env.tile_flv[count_x][count_y].wall);
            marshallShort(th, env.tile_flv[count_x][count_y].floor);
            marshallShort(th, env.tile_flv[count_x][count_y].feat);
            marshallShort(th, env.tile_flv[count_x][count_y].special);
        }

    marshallInt(th, TILE_WALL_MAX);
}

static void tag_read_level(reader &th)
{
    env.floor_colour = unmarshallUByte(th);
    env.rock_colour  = unmarshallUByte(th);


    env.elapsed_time = unmarshallInt(th);
    env.old_player_pos = unmarshallCoord(th);
    env.absdepth0 = absdungeon_depth(you.where_are_you, you.depth);

    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);
    ASSERT(gx == GXM);
    ASSERT(gy == GYM);

    env.turns_on_level = unmarshallInt(th);

    EAT_CANARY;

    env.map_seen.reset();
    for (int i = 0; i < gx; i++)
        for (int j = 0; j < gy; j++)
        {
            dungeon_feature_type feat = unmarshallFeatureType(th);
            grd[i][j] = feat;
            ASSERT(feat < NUM_FEATURES);

            unmarshallMapCell(th, env.map_knowledge[i][j]);
            // Fixup positions
            if (env.map_knowledge[i][j].monsterinfo())
                env.map_knowledge[i][j].monsterinfo()->pos = coord_def(i, j);
            if (env.map_knowledge[i][j].cloudinfo())
                env.map_knowledge[i][j].cloudinfo()->pos = coord_def(i, j);

            env.map_knowledge[i][j].flags &= ~MAP_VISIBLE_FLAG;
            if (env.map_knowledge[i][j].seen())
                env.map_seen.set(i, j);
            env.pgrid[i][j].flags = unmarshallInt(th);

            mgrd[i][j] = NON_MONSTER;
        }

    if (unmarshallBoolean(th))
    {
        MapKnowledge *f = new MapKnowledge();
        for (int x = 0; x < GXM; x++)
            for (int y = 0; y < GYM; y++)
                unmarshallMapCell(th, (*f)[x][y]);
        env.map_forgotten.reset(f);
    }
    else
        env.map_forgotten.reset();

    env.grid_colours.init(BLACK);
    _run_length_decode(th, unmarshallByte, env.grid_colours, GXM, GYM);

    EAT_CANARY;

    env.cloud.clear();
    // how many clouds?
    const int num_clouds = unmarshallShort(th);
    cloud_struct cloud;
    for (int i = 0; i < num_clouds; i++)
    {
        cloud.type  = static_cast<cloud_type>(unmarshallByte(th));
        ASSERT(cloud.type != CLOUD_NONE);
        cloud.pos.x = unmarshallByte(th);
        cloud.pos.y = unmarshallByte(th);
        ASSERT_IN_BOUNDS(cloud.pos);
        cloud.decay = unmarshallShort(th);
        cloud.spread_rate = unmarshallUByte(th);
        cloud.whose = static_cast<kill_category>(unmarshallUByte(th));
        cloud.killer = static_cast<killer_type>(unmarshallUByte(th));
        cloud.source = unmarshallInt(th);
        cloud.excl_rad = unmarshallInt(th);

            env.cloud[cloud.pos] = cloud;
    }

    EAT_CANARY;

    // how many shops?
    const int num_shops = unmarshallShort(th);
    shop_struct shop;
    for (int i = 0; i < num_shops; i++)
    {
        unmarshall_shop(th, shop);
        if (shop.type == SHOP_UNASSIGNED)
            continue;
        env.shop[shop.pos] = shop;
    }

    EAT_CANARY;

    env.sanctuary_pos  = unmarshallCoord(th);
    env.sanctuary_time = unmarshallByte(th);

    env.spawn_random_rate = unmarshallInt(th);

    env.markers.read(th);

    env.properties.clear();
    env.properties.read(th);

    env.dactions_done = unmarshallInt(th);

    // Restore heightmap
    env.heightmap.reset(nullptr);
    const bool have_heightmap = unmarshallBoolean(th);
    if (have_heightmap)
    {
        env.heightmap.reset(new grid_heightmap);
        grid_heightmap &heightmap(*env.heightmap);
        for (rectangle_iterator ri(0); ri; ++ri)
            heightmap(*ri) = unmarshallShort(th);
    }

    EAT_CANARY;

    env.forest_awoken_until = unmarshallInt(th);
    unmarshall_level_vault_data(th);
    env.density = unmarshallInt(th);

    int num_lights = unmarshallShort(th);
    ASSERT(num_lights >= 0);
    env.sunlight.clear();
    while (num_lights-- > 0)
    {
        coord_def c = unmarshallCoord(th);
        env.sunlight.emplace_back(c, unmarshallInt(th));
    }
}


static void tag_read_level_items(reader &th)
{
    unwind_bool dont_scan(crawl_state.crash_debug_scans_safe, false);
    env.trap.clear();
    // how many traps?
    const int trap_count = unmarshallShort(th);
    trap_def trap;
    for (int i = 0; i < trap_count; ++i)
    {
        trap.type = static_cast<trap_type>(unmarshallUByte(th));
        ASSERT(trap.type != TRAP_UNASSIGNED);
        trap.pos      = unmarshallCoord(th);
        trap.ammo_qty = unmarshallShort(th);
        trap.skill_rnd = unmarshallUByte(th);
        env.trap[trap.pos] = trap;
    }


    // how many items?
    const int item_count = unmarshallShort(th);
    ASSERT_RANGE(item_count, 0, MAX_ITEMS + 1);
    for (int i = 0; i < item_count; ++i)
        unmarshallItem(th, mitm[i]);
    for (int i = item_count; i < MAX_ITEMS; ++i)
        mitm[i].clear();

#ifdef DEBUG_ITEM_SCAN
    // There's no way to fix this, even with wizard commands, so get
    // rid of it when restoring the game.
    for (int i = 0; i < item_count; ++i)
    {
        if (mitm[i].defined() && mitm[i].pos.origin())
        {
            debug_dump_item(mitm[i].name(DESC_PLAIN).c_str(), i, mitm[i],
                                        "Fixing up unlinked temporary item:");
            mitm[i].clear();
        }
    }
#endif
}

void unmarshallMonster(reader &th, monster& m)
{
    m.reset();

    m.type           = unmarshallMonType(th);
    if (m.type == MONS_NO_MONSTER)
        return;

    ASSERT(!invalid_monster_type(m.type));

    uint32_t parts    = unmarshallUnsigned(th);
    m.mid             = unmarshallInt(th);
    ASSERT(m.mid > 0);
    m.mname           = unmarshallString(th);
    m.xp_tracking     = static_cast<xp_tracking_type>(unmarshallUByte(th));
    m.set_hit_dice(     unmarshallByte(th));
    ASSERT(m.get_experience_level() > 0);
    m.speed           = unmarshallByte(th);
    // Avoid sign extension when loading files (Elethiomel's hang)
    m.speed_increment = unmarshallUByte(th);
    m.behaviour       = static_cast<beh_type>(unmarshallUByte(th));
    int x             = unmarshallByte(th);
    int y             = unmarshallByte(th);
    m.set_position(coord_def(x,y));
    m.target.x        = unmarshallByte(th);
    m.target.y        = unmarshallByte(th);

    m.firing_pos      = unmarshallCoord(th);
    m.patrol_point    = unmarshallCoord(th);

    int help = unmarshallByte(th);
    m.travel_target = static_cast<montravel_target_type>(help);

    const int len = unmarshallShort(th);
    for (int i = 0; i < len; ++i)
        m.travel_path.push_back(unmarshallCoord(th));

    m.flags.flags = unmarshallUnsigned(th);
    m.experience  = unmarshallInt(th);

    m.enchantments.clear();
    const int nenchs = unmarshallShort(th);
    for (int i = 0; i < nenchs; ++i)
    {
        mon_enchant me = unmarshall_mon_enchant(th);
        m.enchantments[me.ench] = me;
        m.ench_cache.set(me.ench, true);
    }
    m.ench_countdown = unmarshallByte(th);

    m.hit_points     = unmarshallShort(th);
    m.max_hit_points = unmarshallShort(th);
    m.number         = unmarshallInt(th);
    m.base_monster   = unmarshallMonType(th);
    m.colour         = unmarshallShort(th);
    m.summoner       = unmarshallInt(th);

    if (parts & MP_ITEMS)
        for (int j = 0; j < NUM_MONSTER_SLOTS; j++)
            m.inv[j] = unmarshallShort(th);

    if (parts & MP_SPELLS)
    {
        unmarshallSpells(th, m.spells
                         );
        else if (slot.spell == SPELL_CORRUPT_BODY)
        {
            slot.spell = SPELL_CORRUPTING_PULSE;
            m.spells.push_back(slot);
        }
    }
#endif
    }

    m.god      = static_cast<god_type>(unmarshallByte(th));
    m.attitude = static_cast<mon_attitude_type>(unmarshallByte(th));
    m.foe      = unmarshallShort(th);
    m.foe_memory = unmarshallInt(th);

    m.damage_friendly = unmarshallShort(th);
    m.damage_total = unmarshallShort(th);

    m.went_unseen_this_turn = unmarshallByte(th);
    m.unseen_pos = unmarshallCoord(th);

    if (parts & MP_GHOST_DEMON)
        m.set_ghost(unmarshallGhost(th));


    if (parts & MP_CONSTRICTION)
        _unmarshall_constriction(th, &m);

    m.props.clear();
    m.props.read(th);

    if (m.props.exists("monster_tile_name"))
    {
        string tile = m.props["monster_tile_name"].get_string();
        tileidx_t index;
        if (!tile_player_index(tile.c_str(), &index))
        {
            // If invalid tile name, complain and discard the props.
            dprf("bad tile name: \"%s\".", tile.c_str());
            m.props.erase("monster_tile_name");
            if (m.props.exists("monster_tile"))
                m.props.erase("monster_tile");
        }
        else // Update monster tile.
            m.props["monster_tile"] = short(index);
    }


    if (m.type != MONS_PROGRAM_BUG && mons_species(m.type) == MONS_PROGRAM_BUG)
    {
        m.type = MONS_GHOST;
        m.props.clear();
    }

    // If an upgrade synthesizes ghost_demon, please mark it in "parts" above.
    ASSERT(parts & MP_GHOST_DEMON || !mons_is_ghost_demon(m.type));

    m.check_speed();
}

static void tag_read_level_monsters(reader &th)
{
    unwind_bool dont_scan(crawl_state.crash_debug_scans_safe, false);
    int count;

    reset_all_monsters();

    // how many mons_alloc?
    count = unmarshallByte(th);
    ASSERT(count >= 0);
    for (int i = 0; i < count && i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = unmarshallMonType(th);
    for (int i = MAX_MONS_ALLOC; i < count; ++i)
        unmarshallShort(th);
    for (int i = count; i < MAX_MONS_ALLOC; ++i)
        env.mons_alloc[i] = MONS_NO_MONSTER;

    // how many monsters?
    count = unmarshallShort(th);
    ASSERT_RANGE(count, 0, MAX_MONSTERS + 1);

    for (int i = 0; i < count; i++)
    {
        monster& m = menv[i];
        unmarshallMonster(th, m);

        // place monster
        if (!m.alive())
            continue;

        monster *dup_m = monster_by_mid(m.mid);


        // companion_is_elsewhere checks the mid cache
        env.mid_cache[m.mid] = i;
        if (m.is_divine_companion() && companion_is_elsewhere(m.mid))
        {
            dprf("Killed elsewhere companion %s(%d) on %s",
                    m.name(DESC_PLAIN, true).c_str(), m.mid,
                    level_id::current().describe(false, true).c_str());
            monster_die(m, KILL_RESET, -1, true, false);
            // avoid "mid cache bogosity" if there's an unhandled clone bug
            if (dup_m && dup_m->alive())
            {
                mprf(MSGCH_ERROR, "elsewhere companion has duplicate mid %d: %s",
                    dup_m->mid, dup_m->full_name(DESC_PLAIN).c_str());
                env.mid_cache[dup_m->mid] = dup_m->mindex();
            }
            continue;
        }

#if defined(DEBUG) || defined(DEBUG_MONS_SCAN)
        if (invalid_monster_type(m.type))
        {
            mprf(MSGCH_ERROR, "Unmarshalled monster #%d %s",
                 i, m.name(DESC_PLAIN, true).c_str());
        }
        if (!in_bounds(m.pos()))
        {
            mprf(MSGCH_ERROR,
                 "Unmarshalled monster #%d %s out of bounds at (%d, %d)",
                 i, m.name(DESC_PLAIN, true).c_str(),
                 m.pos().x, m.pos().y);
        }
        int midx = mgrd(m.pos());
        if (midx != NON_MONSTER)
        {
            mprf(MSGCH_ERROR, "(%d, %d) for %s already occupied by %s",
                 m.pos().x, m.pos().y,
                 m.name(DESC_PLAIN, true).c_str(),
                 menv[midx].name(DESC_PLAIN, true).c_str());
        }
#endif
        mgrd(m.pos()) = i;
    }
}

static void _debug_count_tiles()
{
#ifdef DEBUG_DIAGNOSTICS
# ifdef USE_TILE
    map<int,bool> found;
    int t, cnt = 0;
    for (int i = 0; i < GXM; i++)
        for (int j = 0; j < GYM; j++)
        {
            t = env.tile_bk_bg[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
            t = env.tile_bk_fg[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
            t = env.tile_bk_cloud[i][j];
            if (!found.count(t))
                cnt++, found[t] = true;
        }
    dprf("Unique tiles found: %d", cnt);
# endif
#endif
}

void tag_read_level_tiles(reader &th)
{
    // Map grids.
    // how many X?
    const int gx = unmarshallShort(th);
    // how many Y?
    const int gy = unmarshallShort(th);

    env.tile_names.clear();
    unsigned int num_tilenames = unmarshallShort(th);
    for (unsigned int i = 0; i < num_tilenames; ++i)
    {
#ifdef DEBUG_TILE_NAMES
        string temp = unmarshallString(th);
        mprf("Reading tile_names[%d] = %s", i, temp.c_str());
        env.tile_names.push_back(temp);
#else
        env.tile_names.push_back(unmarshallString(th));
#endif
    }

    // flavour
    env.tile_default.wall_idx  = unmarshallShort(th);
    env.tile_default.floor_idx = unmarshallShort(th);
    env.tile_default.wall      = unmarshallShort(th);
    env.tile_default.floor     = unmarshallShort(th);
    env.tile_default.special   = unmarshallShort(th);

    for (int x = 0; x < gx; x++)
        for (int y = 0; y < gy; y++)
        {
            env.tile_flv[x][y].wall_idx  = unmarshallShort(th);
            env.tile_flv[x][y].floor_idx = unmarshallShort(th);
            env.tile_flv[x][y].feat_idx  = unmarshallShort(th);

            // These get overwritten by _regenerate_tile_flavour
            env.tile_flv[x][y].wall    = unmarshallShort(th);
            env.tile_flv[x][y].floor   = unmarshallShort(th);
            env.tile_flv[x][y].feat    = unmarshallShort(th);
            env.tile_flv[x][y].special = unmarshallShort(th);
        }

    _debug_count_tiles();

    _regenerate_tile_flavour();

    // Draw remembered map
    _draw_tiles();
}

static tileidx_t _get_tile_from_vector(const unsigned int idx)
{
    if (idx <= 0 || idx > env.tile_names.size())
    {
#ifdef DEBUG_TILE_NAMES
        mprf("Index out of bounds: idx = %d - 1, size(tile_names) = %d",
            idx, env.tile_names.size());
#endif
        return 0;
    }
    string tilename = env.tile_names[idx - 1];

    tileidx_t tile;
    if (!tile_dngn_index(tilename.c_str(), &tile))
    {
#ifdef DEBUG_TILE_NAMES
        mprf("tilename %s (index %d) not found",
             tilename.c_str(), idx - 1);
#endif
        return 0;
    }
#ifdef DEBUG_TILE_NAMES
    mprf("tilename %s (index %d) resolves to tile %d",
         tilename.c_str(), idx - 1, (int) tile);
#endif

    return tile;
}

static void _regenerate_tile_flavour()
{
    /* Remember the wall_idx and floor_idx; tile_init_default_flavour
       sets them to 0 */
    tileidx_t default_wall_idx = env.tile_default.wall_idx;
    tileidx_t default_floor_idx = env.tile_default.floor_idx;
    tile_init_default_flavour();
    if (default_wall_idx)
    {
        tileidx_t new_wall = _get_tile_from_vector(default_wall_idx);
        if (new_wall)
        {
            env.tile_default.wall_idx = default_wall_idx;
            env.tile_default.wall = new_wall;
        }
    }
    if (default_floor_idx)
    {
        tileidx_t new_floor = _get_tile_from_vector(default_floor_idx);
        if (new_floor)
        {
            env.tile_default.floor_idx = default_floor_idx;
            env.tile_default.floor = new_floor;
        }
    }

    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        tile_flavour &flv = env.tile_flv(*ri);
        flv.wall = 0;
        flv.floor = 0;
        flv.feat = 0;
        flv.special = 0;

        if (flv.wall_idx)
        {
            tileidx_t new_wall = _get_tile_from_vector(flv.wall_idx);
            if (!new_wall)
                flv.wall_idx = 0;
            else
                flv.wall = new_wall;
        }
        if (flv.floor_idx)
        {
            tileidx_t new_floor = _get_tile_from_vector(flv.floor_idx);
            if (!new_floor)
                flv.floor_idx = 0;
            else
                flv.floor = new_floor;
        }
        if (flv.feat_idx)
        {
            tileidx_t new_feat = _get_tile_from_vector(flv.feat_idx);
            if (!new_feat)
                flv.feat_idx = 0;
            else
                flv.feat = new_feat;
        }
    }

    tile_new_level(true, false);
}

static void _draw_tiles()
{
#ifdef USE_TILE
    for (rectangle_iterator ri(coord_def(0, 0), coord_def(GXM-1, GYM-1));
         ri; ++ri)
    {
        tile_draw_map_cell(*ri);
    }
#endif
}
// ------------------------------- ghost tags ---------------------------- //

static void marshallSpells(writer &th, const monster_spells &spells)
{
    const uint8_t spellsize = spells.size();
    marshallByte(th, spellsize);
    for (int j = 0; j < spellsize; ++j)
    {
        marshallShort(th, spells[j].spell);
        marshallByte(th, spells[j].freq);
        marshallShort(th, spells[j].flags.flags);
    }
}


static void unmarshallSpells(reader &th, monster_spells &spells
                            )
{
    const uint8_t spellsize =
        unmarshallByte(th);
    spells.clear();
    spells.resize(spellsize);
    for (int j = 0; j < spellsize; ++j)
    {
        spells[j].spell = unmarshallSpellType(th

            );
        spells[j].freq = unmarshallByte(th);
        spells[j].flags.flags = unmarshallShort(th);
    }

}

static void marshallGhost(writer &th, const ghost_demon &ghost)
{
    // save compat changes with minor tags here must be added to bones_minor_tags
    marshallString(th, ghost.name);

    marshallShort(th, ghost.species);
    marshallShort(th, ghost.job);
    marshallByte(th, ghost.religion);
    marshallShort(th, ghost.best_skill);
    marshallShort(th, ghost.best_skill_level);
    marshallShort(th, ghost.xl);
    marshallShort(th, ghost.max_hp);
    marshallShort(th, ghost.ev);
    marshallShort(th, ghost.ac);
    marshallShort(th, ghost.damage);
    marshallShort(th, ghost.speed);
    marshallShort(th, ghost.move_energy);
    marshallByte(th, ghost.see_invis);
    marshallShort(th, ghost.brand);
    marshallShort(th, ghost.att_type);
    marshallShort(th, ghost.att_flav);
    marshallInt(th, ghost.resists);
    marshallByte(th, ghost.colour);
    marshallBoolean(th, ghost.flies);

    marshallSpells(th, ghost.spells);
}

static ghost_demon unmarshallGhost(reader &th)
{
    // save compat changes with minor tags here must be added to bones_minor_tags
    ghost_demon ghost;

    ghost.name             = unmarshallString(th);
    ghost.species          = static_cast<species_type>(unmarshallShort(th));
    ghost.job              = static_cast<job_type>(unmarshallShort(th));
    ghost.religion         = static_cast<god_type>(unmarshallByte(th));
    ghost.best_skill       = static_cast<skill_type>(unmarshallShort(th));
    ghost.best_skill_level = unmarshallShort(th);
    ghost.xl               = unmarshallShort(th);
    ghost.max_hp           = unmarshallShort(th);
    ghost.ev               = unmarshallShort(th);
    ghost.ac               = unmarshallShort(th);
    ghost.damage           = unmarshallShort(th);
    ghost.speed            = unmarshallShort(th);
    ghost.move_energy      = unmarshallShort(th);
    // fix up ghost_demons that forgot to have move_energy initialized
    if (ghost.move_energy < FASTEST_PLAYER_MOVE_SPEED
        || ghost.move_energy > 15) // Ponderous naga
    {
        ghost.move_energy = 10;
    }
    ghost.see_invis        = unmarshallByte(th);
    ghost.brand            = static_cast<brand_type>(unmarshallShort(th));
    ghost.att_type = static_cast<attack_type>(unmarshallShort(th));
    ghost.att_flav = static_cast<attack_flavour>(unmarshallShort(th));
    ghost.resists          = unmarshallInt(th);
    ghost.colour           = unmarshallByte(th);

    ghost.flies        = unmarshallBoolean(th);

    unmarshallSpells(th, ghost.spells
                    );

    return ghost;
}

static void tag_construct_ghost(writer &th, vector<ghost_demon> &ghosts)
{
    // How many ghosts?
    marshallShort(th, ghosts.size());

    for (const ghost_demon &ghost : ghosts)
        marshallGhost(th, ghost);
}

static vector<ghost_demon> tag_read_ghost(reader &th)
{
    vector<ghost_demon> result;
    int nghosts = unmarshallShort(th);

    if (nghosts < 1 || nghosts > MAX_GHOSTS)
    {
        string error = "Bones file has an invalid ghost count (" +
                                                    to_string(nghosts) + ")";
        throw corrupted_save(error);
    }

    for (int i = 0; i < nghosts; ++i)
        result.push_back(unmarshallGhost(th));
    return result;
}

vector<ghost_demon> tag_read_ghosts(reader &th)
{
    global_ghosts.clear();
    tag_read(th, TAG_GHOST);
    return global_ghosts; // should use copy semantics?
}

void tag_write_ghosts(writer &th, const vector<ghost_demon> &ghosts)
{
    global_ghosts = ghosts;
    tag_write(TAG_GHOST, th);
}
