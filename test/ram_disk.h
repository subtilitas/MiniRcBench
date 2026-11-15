/**
 * @file ram_disk.h
 * @brief The card under FatFs R0.15 in the `make lognum` programs: sparse
 *        512-byte sectors in RAM, two card images to swap between, and the
 *        faults a card shows.
 *
 * ram_disk.cpp defines FatFs's disk_* functions and get_fattime() over it,
 * 2026-01-01. A sector of zeros is not stored, so a 1 GiB volume costs only
 * the sectors written.
 */

#pragma once

#include "ff.h"

#include <stdint.h>

#include <map>
#include <vector>

/** @brief One card image: sector number to its 512 bytes. */
typedef std::map<LBA_t, std::vector<BYTE>> RamImage;

/** @brief The card in the slot and its faults. */
struct RamDisk {
	RamImage images[2];  /**< Two cards. */
	int      card;       /**< The one in the slot, 0 or 1. */
	LBA_t    sectors;    /**< The size every image reports. */
	/** @brief disk_status() reports the card uninitialised until the next
	 *         disk_initialize(), as the SDIO glue does after a card fault. */
	bool     noInit;
	bool     protect;    /**< Write-protected: writes fail with RES_WRPRT. */
	/** @brief A write fails with RES_ERROR, writing nothing, when one of its
	 *         sectors begins with the @ref failLen bytes here; nullptr for
	 *         none. */
	const void *failPrefix;
	size_t   failLen;    /**< Bytes of @ref failPrefix compared. */
	bool     failWrites; /**< Every write fails with RES_ERROR. */
	/** @brief A clock in µs that every disk_write() call advances by
	 *         @ref writeUs, a failed one included; nullptr for none. */
	uint64_t *clockUs;
	uint32_t writeUs;    /**< The time one disk_write() call takes, µs. */
};

/** @brief The disk FatFs sees. */
extern RamDisk g_disk;

/**
 * @brief Format both images with @p fmt, card 0 in the slot, no faults, no
 *        clock, and mount card 0 on @p fs. Exits the program when FatFs refuses.
 * @param fs      The volume object.
 * @param fmt     FM_FAT32 or FM_EXFAT.
 * @param sectors The volume's size in sectors.
 * @param au      The cluster size in bytes.
 */
void ramDiskFormat(FATFS *fs, BYTE fmt, LBA_t sectors, DWORD au);
