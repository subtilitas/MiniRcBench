// Host-test stub for pico/bootrom.h.
//
// The boot types of the SDK 2.1.1 header and rom_get_last_boot_type(), which
// fakes.cpp answers with what a test set through fakeSetBootType().
#pragma once

#define BOOT_TYPE_NORMAL       0
#define BOOT_TYPE_BOOTSEL      2
#define BOOT_TYPE_RAM_IMAGE    3
#define BOOT_TYPE_FLASH_UPDATE 4
#define BOOT_TYPE_PC_SP        0xd

int rom_get_last_boot_type(void);
