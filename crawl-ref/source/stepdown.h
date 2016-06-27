/**
 * @file
 * @brief Stepdown functions.
**/

#ifndef STEPDOWN_H
#define STEPDOWN_H

// stepdowns

enum rounding_type
{
    ROUND_DOWN,
    ROUND_CLOSE,
    ROUND_RANDOM
};

int qpow(int value, int num, int denom, int power, bool random_rounding = true);
double fpow(double value, double num, double denom, double power);
int stepup(int value, int multiplier = 1, int base = 2, int divisor = 1);
int stepup2(double value, int divisor, int exponent, int multiplier = 1);
double stepdown(double value, double step, double base = 2);
int stepdown(int value, int step, rounding_type = ROUND_CLOSE, int max = 0, double base = 2);
int stepdown_value(int base_value, int stepping, int first_step,
                   int /*last_step*/, int ceiling_value);

#endif
