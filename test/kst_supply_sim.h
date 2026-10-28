/**
 * @file kst_supply_sim.h
 * @brief A servo on the switched supply, around rcbench's simulated servo
 *        (`kst_sim.h`, copied unchanged).
 *
 * The model follows the level the bench drives: the servo is powered one
 * start-up time after the switch-on and loses power, and programming mode,
 * at the switch-off. A servo with a power-on window answers a sync only
 * within that window after it is powered; once in the mode it stays
 * powered until the switch-off.
 */

#pragma once

#include <stdint.h>

#include "rcbench/kst_sim.h"

/** @brief The switched supply and the servo behind it. */
struct SupplySim {
	sim_t   *sim;        /**< The servo. */
	uint32_t startupUs;  /**< Switch-on to the servo powered. */
	uint32_t windowUs;   /**< How long after power-on a sync enters; 0 for no limit. */
	bool     level;      /**< The level the bench drives. */
	uint32_t levelUs;    /**< When it last changed. */
	unsigned switchOns;  /**< Switch-ons seen. */
	uint32_t lastOnUs;   /**< The last switch-on. */
	uint32_t lastOffUs;  /**< The last switch-off. */
	uint32_t minOffUs;   /**< Shortest time off before a switch-on; UINT32_MAX for none. */
};

/** @brief The servo unpowered, the supply off since @p nowUs. */
static inline void supplySimInit(SupplySim *m, sim_t *sim, uint32_t nowUs) {
	m->sim = sim;
	m->startupUs = 0;
	m->windowUs = 0;
	m->level = false;
	m->levelUs = nowUs;
	m->switchOns = 0;
	m->lastOnUs = 0;
	m->lastOffUs = nowUs;
	m->minOffUs = UINT32_MAX;
	sim->powered = false;
	sim->in_mode = false;
}

/** @brief The bench drives @p level at @p nowUs: the servo follows. */
static inline void supplySimUpdate(SupplySim *m, bool level, uint32_t nowUs) {
	if (level != m->level) {
		m->level = level;
		m->levelUs = nowUs;
		if (level) {
			uint32_t off = nowUs - m->lastOffUs;
			if (off < m->minOffUs) m->minOffUs = off;
			m->switchOns++;
			m->lastOnUs = nowUs;
		} else {
			m->lastOffUs = nowUs;
		}
	}
	sim_t *s = m->sim;
	if (!level) {
		s->powered = false;
		s->in_mode = false;
		return;
	}
	uint32_t since = nowUs - m->levelUs;
	bool up = since >= m->startupUs;
	bool inWindow = m->windowUs == 0 || since - m->startupUs < m->windowUs;
	s->powered = up && (s->in_mode || inWindow);
}
