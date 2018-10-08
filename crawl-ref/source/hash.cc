/**
 * @file
 * @brief Hashes.
**/

#include "AppHdr.h"

#include "hash.h"

#include "endianness.h"

// MurmurHash2, by Austin Appleby
uint32_t hash32(const void *data, int len)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;

    // Initialize the hash to a 'random' value
    uint32_t h = len;

    const uint8_t *d = (const uint8_t*)data;
    // Mix 4 bytes at a time into the hash
    while (len >= 4)
    {
        uint32_t k = htole32(*(uint32_t *)d);

        k *= m;
        k ^= k >> 24;
        k *= m;

        h *= m;
        h ^= k;

        d += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch (len)
    {
    case 3: h ^= (uint32_t)d[2] << 16;
    case 2: h ^= (uint32_t)d[1] << 8;
    case 1: h ^= (uint32_t)d[0];
            h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

unsigned int hash_with_seed(int x, uint32_t seed, uint32_t id)
{
    if (x < 2)
        return 0;
    uint32_t data[2] = {seed, id};
    return hash32(data, sizeof(data)) % x;
}
