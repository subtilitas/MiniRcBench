/**
 * @file
 * @brief Adapted from rcbench shared/link/include/link_pages.h, commit
 *        3e29dc1b, MIT.
 *
 * The 5 values of rcbench's link page map that test_hitec_session.c holds
 * the Hitec session's values to, and nothing else. rcbench's header is the
 * page map of the link between its panel and its IOmcu (input/output
 * microcontroller), 2,734 lines, which MiniRcBench does not port;
 * test_hitec_session.c stays byte for byte rcbench's and includes this file
 * in its place.
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RCBENCH_LINK_PAGES_H
#define RCBENCH_LINK_PAGES_H

/** @brief The longest off time of a POWER, ms. */
#define LINK_HITEC_OFF_MS_MAX 10000u

/** @brief How the page decodes a capture. */
typedef enum {
    LINK_HITEC_DEC_NONE = 0,
    LINK_HITEC_DEC_C = 1,       /**< hitec_c_decode() */
    LINK_HITEC_DEC_D = 2,       /**< hitec_d_decode() */
    LINK_HITEC_DEC_LISTEN = 3,  /**< hitec_listen_decode() */
} link_hitec_dec_t;

#endif
