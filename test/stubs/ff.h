// Host-test stub for ChaN's FatFs, as vendored by no-OS-FatFS-SD-SDIO-SPI.
//
// The part of the API that sd_log.cpp and kst_files.cpp call, with FatFs's own
// names, values and argument order. sd_log.cpp only typechecks against it.
// kst_files.cpp is linked into the suite against fake_fatfs.cpp, an in-memory
// volume; what FatFs itself does on a card is exercised on hardware.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef unsigned int UINT;
typedef uint8_t BYTE;
typedef uint32_t FSIZE_t;

typedef enum {
	FR_OK = 0, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE,
	FR_NO_PATH, FR_INVALID_NAME, FR_DENIED, FR_EXIST, FR_INVALID_OBJECT,
	FR_WRITE_PROTECTED, FR_INVALID_DRIVE, FR_NOT_ENABLED, FR_NO_FILESYSTEM,
	FR_MKFS_ABORTED, FR_TIMEOUT, FR_LOCKED, FR_NOT_ENOUGH_CORE,
	FR_TOO_MANY_OPEN_FILES, FR_INVALID_PARAMETER,
} FRESULT;

#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_CREATE_NEW    0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS   0x10

#define AM_RDO 0x01
#define AM_DIR 0x10

typedef struct { int dummy; } FATFS;
typedef struct { int handle; FSIZE_t pos; bool writing; } FIL;
typedef struct { int handle; UINT next; } DIR;
typedef struct {
	FSIZE_t fsize;
	BYTE    fattrib;
	char    fname[256];
} FILINFO;

FRESULT f_mount(FATFS *fs, const char *path, uint8_t opt);
FRESULT f_unmount(const char *path);
FRESULT f_open(FIL *fp, const char *path, uint8_t mode);
FRESULT f_close(FIL *fp);
FRESULT f_read(FIL *fp, void *buf, UINT btr, UINT *br);
FRESULT f_write(FIL *fp, const void *buf, UINT btw, UINT *bw);
FRESULT f_sync(FIL *fp);
FRESULT f_truncate(FIL *fp);
FRESULT f_expand(FIL *fp, FSIZE_t fsz, uint8_t opt);
FRESULT f_stat(const char *path, FILINFO *fno);
FRESULT f_opendir(DIR *dp, const char *path);
FRESULT f_readdir(DIR *dp, FILINFO *fno);
FRESULT f_closedir(DIR *dp);
FRESULT f_mkdir(const char *path);
FRESULT f_unlink(const char *path);
FRESULT f_rename(const char *path_old, const char *path_new);
