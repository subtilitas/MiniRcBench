/*
 * Adapted from rcbench test/host/test_outputs.c, commit 453cf334, MIT.
 *
 * The PWM (pulse-width modulation) fold of out_pwm_map.h: which slice and
 * channel each GPIO (general-purpose input/output) drives, which pins share
 * a compare register, and the longest pulse a frame rate carries.  The
 * out_pwm_map cases of the original, as they are; the outputs layer and the
 * link pages they sat beside are not part of this project, so the lines of
 * the longest-pulse case that checked the link pages' ceiling are left out.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>

#include "greatest.h"

#include "out_pwm_map.h"

TEST_CASE(the_pwm_fold_puts_each_pin_on_a_named_slice_and_channel)
{
    static const struct { uint8_t pin, slice, chan; } k[] = {
        {  0u,  0u, 0u }, {  1u,  0u, 1u }, {  2u,  1u, 0u }, {  3u,  1u, 1u },
        { 14u,  7u, 0u }, { 15u,  7u, 1u },
        /* The fold: GP16 lands back on slice 0 channel A, where GP0 is. */
        { 16u,  0u, 0u }, { 17u,  0u, 1u }, { 22u,  3u, 0u }, { 28u,  6u, 0u },
        { 31u,  7u, 1u },
        /* Above GP31 four slices serve sixteen pins, so it folds at eight. */
        { 32u,  8u, 0u }, { 33u,  8u, 1u }, { 39u, 11u, 1u },
        { 40u,  8u, 0u }, { 47u, 11u, 1u },
    };
    for (unsigned i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
        CHECK_EQ(out_pwm_slice_of(k[i].pin), k[i].slice);
        CHECK_EQ(out_pwm_channel_of(k[i].pin), k[i].chan);
    }
    /* Twelve slices of two channels is what the arithmetic has to reach. */
    CHECK_EQ(out_pwm_slice_of(38u), OUT_PWM_SLICES - 1u);
    CHECK_EQ(out_pwm_channel_of(39u), OUT_PWM_CHANNELS - 1u);
}

/*
 * The pairs that share one compare register, which is one pulse width.
 *
 * Every pair named here is two free pins of the coprocessor's own header, so
 * an operator ticking pins on the OUTPUTS page can reach all six.
 */
TEST_CASE(two_header_pins_sixteen_apart_are_one_compare_register)
{
    static const uint8_t k_collide[][2] = {
        {  0u, 16u }, {  1u, 17u }, {  2u, 18u },
        {  4u, 20u }, {  5u, 21u }, {  6u, 22u },
    };
    for (unsigned i = 0; i < sizeof(k_collide) / sizeof(k_collide[0]); ++i) {
        CHECK(out_pwm_same_compare(k_collide[i][0], k_collide[i][1]));
        CHECK(out_pwm_same_compare(k_collide[i][1], k_collide[i][0]));
        CHECK(out_pwm_same_slice(k_collide[i][0], k_collide[i][1]));
    }

    /* The two channels of one slice: one wrap, two compare registers, so a
     * shared frame rate and two independent pulse widths. */
    static const uint8_t k_slice_only[][2] = {
        {  0u,  1u }, {  2u, 19u }, {  6u,  7u }, { 13u, 28u }, { 26u, 27u },
    };
    for (unsigned i = 0; i < sizeof(k_slice_only) / sizeof(k_slice_only[0]);
         ++i) {
        CHECK(out_pwm_same_slice(k_slice_only[i][0], k_slice_only[i][1]));
        CHECK(!out_pwm_same_compare(k_slice_only[i][0], k_slice_only[i][1]));
    }

    /* Different slices share nothing at all. */
    CHECK(!out_pwm_same_slice(0u, 2u));
    CHECK(!out_pwm_same_compare(0u, 2u));
    CHECK(!out_pwm_same_slice(31u, 32u));

    /* A pin against itself is the same register, which is what makes
     * rebinding a pin already bound a question about the pin, not the fold. */
    CHECK(out_pwm_same_compare(7u, 7u));
}

/*
 * The whole fold as one rule: below GP32 a collision is exactly sixteen pins
 * apart, at and above it exactly eight, and nothing crosses between the two
 * ranges.  Stated as the distance rather than as the slice arithmetic, so a
 * wrong divisor or a wrong mask fails here instead of agreeing with itself.
 */
TEST_CASE(the_only_pins_sharing_a_compare_register_are_the_folded_pairs)
{
    for (unsigned a = 0; a < OUT_PWM_GPIOS; ++a) {
        for (unsigned b = a + 1u; b < OUT_PWM_GPIOS; ++b) {
            const bool folded = (b < 32u) ? (b - a == 16u)
                                          : (a >= 32u && b - a == 8u);
            if (out_pwm_same_compare((uint8_t)a, (uint8_t)b) != folded) {
                T_FAIL("GP%u and GP%u: compare sharing is %d, want %d",
                       a, b, (int)out_pwm_same_compare((uint8_t)a, (uint8_t)b),
                       (int)folded);
            }
        }
    }
}

/*
 * The longest pulse a frame carries is the frame's last count: one
 * microsecond under the period, rounded down.  0 for a rate of 0 and for a
 * period the sixteen-bit counter cannot hold.
 */
TEST_CASE(the_longest_pulse_is_one_count_under_the_period)
{
    static const struct {
        uint16_t hz;
        uint16_t longest;
    } k[] = {
        { 560u, 1784u },  { 500u, 1999u },  { 400u, 2499u },
        { 333u, 3002u },  { 50u, 19999u },  { 40u, 24999u },
        { 16u, 62499u },  { 15u, 0u },      { 0u, 0u },
        { 65535u, 14u },
    };
    for (unsigned i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
        CHECK_EQ(out_pwm_pulse_max(k[i].hz), k[i].longest);
    }
}

/* A pin the part does not have reaches no register, so it shares none. */
TEST_CASE(a_pin_past_the_bank_reaches_no_slice)
{
    CHECK_EQ(out_pwm_slice_of((uint8_t)OUT_PWM_GPIOS), OUT_PWM_NONE);
    CHECK_EQ(out_pwm_channel_of((uint8_t)OUT_PWM_GPIOS), OUT_PWM_NONE);
    CHECK_EQ(out_pwm_slice_of(255u), OUT_PWM_NONE);
    CHECK(!out_pwm_same_slice((uint8_t)OUT_PWM_GPIOS, 0u));
    CHECK(!out_pwm_same_slice(0u, (uint8_t)OUT_PWM_GPIOS));
    CHECK(!out_pwm_same_compare((uint8_t)OUT_PWM_GPIOS,
                                (uint8_t)OUT_PWM_GPIOS));
    CHECK(!out_pwm_same_compare(255u, 0u));
}

int main(void)
{
    RUN(the_pwm_fold_puts_each_pin_on_a_named_slice_and_channel);
    RUN(two_header_pins_sixteen_apart_are_one_compare_register);
    RUN(the_only_pins_sharing_a_compare_register_are_the_folded_pairs);
    RUN(the_longest_pulse_is_one_count_under_the_period);
    RUN(a_pin_past_the_bank_reaches_no_slice);
    return test_summary("out_pwm_map");
}
