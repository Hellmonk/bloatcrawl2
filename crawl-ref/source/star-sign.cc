#include "AppHdr.h"

#include "star-sign.h"
#include "star-sign-data.h"

static const star_sign_def& _get_star_sign(star_sign sign)
{
    ASSERT(sign != star_sign::none);
    ASSERT(sign != star_sign::COUNT);
    return star_signs.at(sign);
}

star_sign random_star_sign()
{
    int tries = 0;
    star_sign sign;
    do {
        tries++;
        sign = static_cast<star_sign>(random_range(0, NUM_STAR_SIGNS - 1));
        dprf("Considering star sign %s", _get_star_sign(sign).name);
    } while (tries < 100 && !_get_star_sign(sign).valid(you.species, you.char_class));
    if (tries == 100)
    {
        mprf(MSGCH_ERROR, "Couldn't find a valid star sign!");
        return star_sign::none;
    }
    dprf("Picked star sign %d", sign);
    return sign;
}

void apply_star_sign()
{
    dprf("Checking star sign %d", static_cast<int>(you.star_sign));
    if (you.star_sign == star_sign::none)
        return;

    auto sign = _get_star_sign(you.star_sign);
    mprf(MSGCH_PLAIN, "<lightblue>"
            "You were born under the sign of %s, which gives you %s."
            "</lightblue>",
            sign.name, sign.effect);
    sign.apply();
}

const char* star_sign_name(star_sign sign)
{
    return _get_star_sign(sign).name;
}
