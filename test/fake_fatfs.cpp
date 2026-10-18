/**
 * @file fake_fatfs.cpp
 * @brief An in-memory FAT volume. @see fake_fatfs.h
 */

#include "fake_fatfs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>

namespace {

struct Entry {
	std::string path;
	bool        dir;
	std::string data;
	bool        used;
	bool        readOnly;
	int         opens;   /**< Open file objects: FF_FS_LOCK's share count. */
	int         writers; /**< Of those, opened for writing. */
};

struct Failure {
	bool     armed;
	unsigned skip;
	FRESULT  fr;
};

std::vector<Entry> g_entries;
uint32_t g_capacity = 0;
uint32_t g_dirLimit = 0xFFFFFFFFu;
bool     g_protected = false;
int      g_open = 0;
int      g_openDirs = 0;
Failure  g_fail[(unsigned)FakeFatCall::Count];
FRESULT  g_renameLate = FR_OK;
FRESULT  g_renameHalf = FR_OK;
unsigned g_calls[(unsigned)FakeFatCall::Count];

/** @brief Count the call; the armed failure for it, or FR_OK. */
FRESULT enter(FakeFatCall c) {
	g_calls[(unsigned)c]++;
	Failure &f = g_fail[(unsigned)c];
	if (!f.armed) return FR_OK;
	if (f.skip) {
		f.skip--;
		return FR_OK;
	}
	f.armed = false;
	return f.fr;
}

int find(const char *path) {
	for (size_t i = 0; i < g_entries.size(); i++)
		if (g_entries[i].used && strcasecmp(g_entries[i].path.c_str(), path) == 0) return (int)i;
	return -1;
}

std::string parentOf(const char *path) {
	const char *slash = strrchr(path, '/');
	return slash ? std::string(path, (size_t)(slash - path)) : std::string();
}

/** @brief FR_OK when the parent directory of @p path exists. */
FRESULT parentOk(const char *path) {
	std::string p = parentOf(path);
	if (p.empty()) return FR_OK;
	int i = find(p.c_str());
	return (i >= 0 && g_entries[(size_t)i].dir) ? FR_OK : FR_NO_PATH;
}

uint32_t used() {
	uint32_t n = 0;
	for (const Entry &e : g_entries)
		if (e.used) n += (uint32_t)e.data.size();
	return n;
}

unsigned childrenOf(const std::string &dir) {
	unsigned n = 0;
	for (const Entry &e : g_entries)
		if (e.used && strcasecmp(parentOf(e.path.c_str()).c_str(), dir.c_str()) == 0) n++;
	return n;
}

int add(const char *path, bool dir) {
	g_entries.push_back(Entry{ path, dir, std::string(), true, false, 0, 0 });
	return (int)g_entries.size() - 1;
}

}  // namespace

void fakeFatReset(uint32_t capacity) {
	g_entries.clear();
	g_capacity = capacity;
	g_dirLimit = 0xFFFFFFFFu;
	g_protected = false;
	g_open = 0;
	g_openDirs = 0;
	memset(g_fail, 0, sizeof(g_fail));
	g_renameLate = FR_OK;
	g_renameHalf = FR_OK;
	memset(g_calls, 0, sizeof(g_calls));
}

void fakeFatWriteProtect(bool on) { g_protected = on; }
void fakeFatDirLimit(uint32_t entries) { g_dirLimit = entries; }

void fakeFatFail(FakeFatCall call, FRESULT fr, unsigned skip) {
	g_fail[(unsigned)call] = Failure{ true, skip, fr };
}

void fakeFatRenameFailsLate(FRESULT fr) { g_renameLate = fr; }
void fakeFatRenameFailsHalf(FRESULT fr) { g_renameHalf = fr; }

void fakeFatPut(const char *path, const char *data, size_t len) {
	int i = find(path);
	if (i < 0) i = add(path, false);
	g_entries[(size_t)i].data.assign(data, len);
}

void fakeFatMkdir(const char *path) {
	if (find(path) < 0) add(path, true);
}

void fakeFatReadOnly(const char *path) {
	int i = find(path);
	if (i >= 0) g_entries[(size_t)i].readOnly = true;
}

bool fakeFatExists(const char *path) { return find(path) >= 0; }

int32_t fakeFatGet(const char *path, char *buf, size_t cap) {
	int i = find(path);
	if (i < 0 || g_entries[(size_t)i].dir) return -1;
	const std::string &d = g_entries[(size_t)i].data;
	memcpy(buf, d.data(), d.size() < cap ? d.size() : cap);
	return (int32_t)d.size();
}

unsigned fakeFatEntries() {
	unsigned n = 0;
	for (const Entry &e : g_entries) n += e.used;
	return n;
}

int fakeFatOpenFiles() { return g_open; }
int fakeFatOpenDirs() { return g_openDirs; }
unsigned fakeFatCalls(FakeFatCall call) { return g_calls[(unsigned)call]; }

// ---------------------------------------------------------------------------
// FatFs
// ---------------------------------------------------------------------------

FRESULT f_open(FIL *fp, const char *path, uint8_t mode) {
	FRESULT fr = enter(FakeFatCall::Open);
	if (fr != FR_OK) return fr;
	// In f_open()'s order: the volume, the path, the lock (chk_share(): a
	// write open needs no other open, a read open no writer), then what is
	// in the way.
	if ((mode & FA_WRITE) && g_protected) return FR_WRITE_PROTECTED;
	if ((fr = parentOk(path)) != FR_OK) return fr;
	int i = find(path);
	bool create = mode & (FA_CREATE_NEW | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS);
	bool writing = mode & ~FA_READ;
	if (i >= 0 && (writing ? g_entries[(size_t)i].opens : g_entries[(size_t)i].writers))
		return FR_LOCKED;
	if (i >= 0 && create) {
		const Entry &e = g_entries[(size_t)i];
		if (e.dir || e.readOnly) return FR_DENIED;
		if (mode & FA_CREATE_NEW) return FR_EXIST;
	}
	if (i >= 0 && g_entries[(size_t)i].dir) return FR_NO_FILE;
	if (i >= 0 && (mode & FA_WRITE) && g_entries[(size_t)i].readOnly) return FR_DENIED;
	if (i < 0) {
		if (!create) return FR_NO_FILE;
		if (childrenOf(parentOf(path)) >= g_dirLimit) return FR_DENIED;
		i = add(path, false);
	} else if (mode & FA_CREATE_ALWAYS) {
		g_entries[(size_t)i].data.clear();
	}
	fp->handle = i;
	fp->pos = 0;
	fp->writing = writing;
	g_entries[(size_t)i].opens++;
	g_entries[(size_t)i].writers += writing;
	g_open++;
	return FR_OK;
}

// As FatFs: only a close that succeeds frees the file object and its lock
// entry. A failure injected here stands for the sync inside f_close().
FRESULT f_close(FIL *fp) {
	if (fp->handle < 0) return FR_INVALID_OBJECT;
	FRESULT fr = enter(FakeFatCall::Close);
	if (fr != FR_OK) return fr;
	g_entries[(size_t)fp->handle].opens--;
	g_entries[(size_t)fp->handle].writers -= fp->writing;
	fp->handle = -1;
	g_open--;
	return FR_OK;
}

FRESULT f_read(FIL *fp, void *buf, UINT btr, UINT *br) {
	*br = 0;
	FRESULT fr = enter(FakeFatCall::Read);
	if (fr != FR_OK) return fr;
	const std::string &d = g_entries[(size_t)fp->handle].data;
	UINT n = fp->pos >= d.size() ? 0 : (UINT)(d.size() - fp->pos);
	if (n > btr) n = btr;
	memcpy(buf, d.data() + fp->pos, n);
	fp->pos += n;
	*br = n;
	return FR_OK;
}

FRESULT f_write(FIL *fp, const void *buf, UINT btw, UINT *bw) {
	*bw = 0;
	FRESULT fr = enter(FakeFatCall::Write);
	if (fr != FR_OK) return fr;
	uint32_t room = g_capacity > used() ? g_capacity - used() : 0;
	UINT n = btw < room ? btw : (UINT)room;
	std::string &d = g_entries[(size_t)fp->handle].data;
	d.replace(fp->pos, n, (const char *)buf, n);
	fp->pos += n;
	*bw = n;
	return FR_OK;
}

FRESULT f_sync(FIL *) { return enter(FakeFatCall::Sync); }

FRESULT f_stat(const char *path, FILINFO *fno) {
	FRESULT fr = enter(FakeFatCall::Stat);
	if (fr != FR_OK) return fr;
	if ((fr = parentOk(path)) != FR_OK) return fr;
	int i = find(path);
	if (i < 0) return FR_NO_FILE;
	const Entry &e = g_entries[(size_t)i];
	fno->fsize = (FSIZE_t)e.data.size();
	fno->fattrib = e.dir ? AM_DIR : 0;
	const char *slash = strrchr(e.path.c_str(), '/');
	snprintf(fno->fname, sizeof(fno->fname), "%s", slash ? slash + 1 : e.path.c_str());
	return FR_OK;
}

FRESULT f_opendir(DIR *dp, const char *path) {
	FRESULT fr = enter(FakeFatCall::OpenDir);
	if (fr != FR_OK) return fr;
	int i = find(path);
	if (i < 0 || !g_entries[(size_t)i].dir) return FR_NO_PATH;
	dp->handle = i;
	dp->next = 0;
	g_openDirs++;
	return FR_OK;
}

FRESULT f_readdir(DIR *dp, FILINFO *fno) {
	FRESULT fr = enter(FakeFatCall::ReadDir);
	if (fr != FR_OK) return fr;
	const std::string &dir = g_entries[(size_t)dp->handle].path;
	while (dp->next < g_entries.size()) {
		const Entry &e = g_entries[dp->next++];
		if (!e.used || strcasecmp(parentOf(e.path.c_str()).c_str(), dir.c_str()) != 0) continue;
		fno->fsize = (FSIZE_t)e.data.size();
		fno->fattrib = e.dir ? AM_DIR : 0;
		snprintf(fno->fname, sizeof(fno->fname), "%s", strrchr(e.path.c_str(), '/') + 1);
		return FR_OK;
	}
	fno->fname[0] = '\0';
	return FR_OK;
}

FRESULT f_closedir(DIR *dp) {
	if (dp->handle < 0) return FR_INVALID_OBJECT;
	dp->handle = -1;
	g_openDirs--;
	return FR_OK;
}

FRESULT f_mkdir(const char *path) {
	FRESULT fr = enter(FakeFatCall::MkDir);
	if (fr != FR_OK) return fr;
	if (g_protected) return FR_WRITE_PROTECTED;
	if ((fr = parentOk(path)) != FR_OK) return fr;
	if (find(path) >= 0) return FR_EXIST;
	if (childrenOf(parentOf(path)) >= g_dirLimit) return FR_DENIED;
	add(path, true);
	return FR_OK;
}

FRESULT f_unlink(const char *path) {
	FRESULT fr = enter(FakeFatCall::Unlink);
	if (fr != FR_OK) return fr;
	if (g_protected) return FR_WRITE_PROTECTED;
	if ((fr = parentOk(path)) != FR_OK) return fr;
	int i = find(path);
	if (i < 0) return FR_NO_FILE;
	Entry &e = g_entries[(size_t)i];
	if (e.opens) return FR_LOCKED;
	if (e.readOnly || (e.dir && childrenOf(e.path))) return FR_DENIED;
	e.used = false;
	return FR_OK;
}

FRESULT f_rename(const char *pathOld, const char *pathNew) {
	FRESULT fr = enter(FakeFatCall::Rename);
	if (fr != FR_OK) return fr;
	if (g_protected) return FR_WRITE_PROTECTED;
	if ((fr = parentOk(pathOld)) != FR_OK) return fr;
	int i = find(pathOld);
	if (i < 0) return FR_NO_FILE;
	if (g_entries[(size_t)i].opens) return FR_LOCKED;
	if (find(pathNew) >= 0) return FR_EXIST;
	if ((fr = parentOk(pathNew)) != FR_OK) return fr;
	// FatFs registers the new name before it removes the old one, in the
	// same directory too: a full directory refuses any rename.
	if (childrenOf(parentOf(pathNew)) >= g_dirLimit) return FR_DENIED;
	if (g_renameHalf != FR_OK) {
		Entry copy = g_entries[(size_t)i];
		copy.path = pathNew;
		copy.opens = copy.writers = 0;
		g_entries.push_back(copy);
		fr = g_renameHalf;
		g_renameHalf = FR_OK;
		return fr;
	}
	g_entries[(size_t)i].path = pathNew;
	fr = g_renameLate;
	g_renameLate = FR_OK;
	return fr;
}
