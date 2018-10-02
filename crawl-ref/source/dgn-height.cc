/**
 * @file
 * @brief Dungeon heightmap routines.
**/

#include "AppHdr.h"

#include "dgn-height.h"

#include "coord.h"
#include "coordit.h"
#include "dungeon.h"

void dgn_initialise_heightmap(int height)
{
    env.heightmap.reset(new grid_heightmap);
    for (rectangle_iterator ri(0); ri; ++ri)
        dgn_height_at(*ri) = height;
}

void dgn_height_set_at(const coord_def &c, int height)
{
    if (env.heightmap)
        dgn_height_at(c) = height;
}

int resolve_range(int_range range, int nrolls)
{
    return random_range(range.first, range.second, nrolls);
}

void dgn_island_centred_at(const coord_def &c,
                           int n_points,
                           int radius,
                           int_range height_delta_range,
                           int border_margin,
                           bool make_atoll)
{
    if (make_atoll)
    {
        dgn_island_centred_at(c, n_points, radius,
                              int_range(-height_delta_range.second,
                                        -height_delta_range.first),
                              border_margin,
                              false);
        radius += 3;
    }
    for (int i = 0; i < n_points; ++i)
    {
        const int thisrad = random2(1 + radius);
        const coord_def p = dgn_random_point_from(c, thisrad, border_margin);
        if (!p.origin())
            dgn_height_at(p) += resolve_range(height_delta_range);
    }
}

void dgn_smooth_height_at(coord_def c, int radius, int max_height)
{
    if (!in_bounds(c))
        return;

    const int height = dgn_height_at(c);
    if (max_height != DGN_UNDEFINED_HEIGHT && height > max_height)
        return;

    const int max_delta = radius * radius * 2 + 2;
    int divisor = 0;
    int total = 0;
    for (int y = c.y - radius; y <= c.y + radius; ++y)
    {
        for (int x = c.x - radius; x <= c.x + radius; ++x)
        {
            const coord_def p(x, y);
            if (!in_bounds(p))
                continue;
            const int nheight = dgn_height_at(p);
            if (max_height != DGN_UNDEFINED_HEIGHT && nheight > max_height)
                continue;
            const coord_def off = c - p;
            const int weight = max_delta - off.abs();
            divisor += weight;
            total += nheight * weight;
        }
    }
    // Can't actually be zero currently unless someone passes a negative
    // or overflowing radius, but this avoids a static analysis warning.
    if (divisor)
        dgn_height_at(c) = total / divisor;
}

void dgn_smooth_heights(int radius, int npasses)
{
    for (int i = 0; i < npasses; ++i)
    {
        const int xspan = GXM / 2, yspan = GYM / 2;
        for (int y = yspan - 1; y >= 0; --y)
            for (int x = xspan - 1; x >= 0; --x)
            {
                dgn_smooth_height_at(coord_def(x, y), radius);
                dgn_smooth_height_at(coord_def(2 * xspan - x - 1, y), radius);
                dgn_smooth_height_at(coord_def(x, 2 * yspan - y - 1), radius);
                dgn_smooth_height_at(coord_def(2 * xspan - x - 1,
                                               2 * yspan - y - 1),
                                     radius);
            }
    }
}

//////////////////////////////////////////////////////////////////////
// dgn_island_plan

void dgn_island_plan::build(int nislands)
{
    for (int i = 0; i < nislands; ++i)
        build_island();
}

coord_def dgn_island_plan::pick_island_spot()
{
    coord_def c;
    // Try to find a spot that's not too close to other islands; this
    // is not a guarantee, though.
    for (int i = 0; i < 15; ++i)
    {
        // Primary island centres should have a little clearance
        // around them, so use 2x the actual margin.
        c = dgn_random_point_in_margin(level_border_depth * 2);

        bool collides = false;
        for (const coord_def island : islands)
        {
            const coord_def dist = island - c;
            if (dist.abs() < island_separation_dist2)
            {
                collides = true;
                break;
            }
        }
        if (!collides)
            break;
    }
    islands.push_back(c);
    return c;
}

void dgn_island_plan::build_island()
{
    const coord_def c = pick_island_spot();
    dgn_island_centred_at(c, resolve_range(n_island_centre_delta_points),
                          resolve_range(island_centre_radius_range),
                          island_centre_point_height_increment,
                          level_border_depth,
                          x_chance_in_y(atoll_roll, 100));

    const int additional_heights = resolve_range(n_aux_centres);
    for (int i = 0; i < additional_heights; ++i)
    {
        const int addition_offset = resolve_range(aux_centre_offset_range);

        const coord_def offsetC =
            dgn_random_point_from(c, addition_offset, level_border_depth);
        if (!offsetC.origin())
        {
            dgn_island_centred_at(
                offsetC, resolve_range(n_island_aux_delta_points),
                resolve_range(island_aux_radius_range),
                island_aux_point_height_increment,
                level_border_depth,
                x_chance_in_y(atoll_roll, 100));
        }
    }
}

coord_def dgn_island_plan::pick_and_remove_random_island()
{
    if (islands.empty())
        return coord_def(0, 0);

    const int lucky_island = random2(islands.size());
    const coord_def c = islands[lucky_island];
    islands.erase(islands.begin() + lucky_island);
    return c;
}
