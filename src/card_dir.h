/**
 * @file card_dir.h
 * @brief The SD (Secure Digital) card for the tools that keep files on it:
 *        the mount rule, FatFs's current directory for a scope, and the file
 *        calls the KST files and the device backups share.
 *
 * Core0 only. Every call touches FatFs, so a caller checks cardMounted()
 * first: while a USB (Universal Serial Bus) host holds the card, no call
 * here may run.
 *
 * Paths. A directory is named from the root without a slash, `KST` or
 * `BACKUP`. cardPath() gives the path of a file in it: the name alone while
 * that directory is FatFs's current one, `/<dir>/<name>` otherwise. A
 * `CardDirScope` makes a directory current for its life and the root again
 * at its end, so the lookup of the directory in the root runs once per
 * scope, and the blackbox logger's paths, which start at the root with `/`,
 * are unchanged.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** @brief Room for `/`, a directory of up to 8 characters, `/`, and a file name. */
#define CARD_PATH_MAX 56

/** @brief What a file call reports. */
enum class CardIo : uint8_t {
	Ok,
	Exists,   /**< The name is taken; nothing was written. */
	Full,     /**< No space, or a full directory; a new file is removed again. */
	Error,    /**< Any other failure; a new file is removed again. */
	Denied,   /**< Write protected, or a file or directory in the way. */
};

/**
 * @brief Whether a card is mounted and not held by a USB host: the logger has
 *        it mounted (SdLogState Idle or Logging) and the USB card reader is
 *        off (MscState::Idle).
 */
bool cardMounted();

/**
 * @brief FatFs's current directory set to @p dir for the object's life.
 *
 * Scopes nest. The outermost one changes directory; an inner one for the
 * same directory does nothing, and an inner one for another directory leaves
 * the current one, so its paths start at the root. Without @p dir on the
 * card, or with the change refused, nothing changes and every path starts at
 * the root. Pass @p on false, for a card that is not mounted, and the scope
 * does nothing at all.
 */
class CardDirScope {
public:
	/** @brief Make @p dir current when @p on and no outer scope has changed directory. */
	explicit CardDirScope(const char *dir, bool on = true);
	/** @brief Back to the root when this scope changed directory. */
	~CardDirScope();
	CardDirScope(const CardDirScope &) = delete;
	CardDirScope &operator=(const CardDirScope &) = delete;
private:
	bool m_on;
};

/** @brief Whether @p dir is FatFs's current directory. */
bool cardDirCurrent(const char *dir);

/** @brief The path of @p name in @p dir into @p out, `CARD_PATH_MAX` bytes. */
void cardPath(const char *dir, const char *name, char *out);

/** @brief The path that opens @p dir itself: empty while it is current. */
const char *cardDirPath(const char *dir);

/**
 * @brief Make @p dir when it is missing; a file of that name is Denied.
 * @param dir       The directory, from the root.
 * @param[out] made True when this call made it.
 */
CardIo cardMakeDir(const char *dir, bool *made);

/**
 * @brief A new file @p name in @p dir with @p len bytes of @p data, synced and
 *        closed; the directory is made first when missing. A failure leaves
 *        no new file and no directory this call made, except a file whose
 *        close failed: FatFs keeps it open and refuses to remove it.
 */
CardIo cardCreate(const char *dir, const char *name, const char *data, size_t len);

/**
 * @brief Write @p len bytes over the existing file @p name in @p dir, in place:
 *        the file keeps its first cluster. A failure leaves part of the new
 *        bytes in the file.
 */
CardIo cardOverwrite(const char *dir, const char *name, const char *data, size_t len);

/** @brief Size of the file @p name in @p dir in bytes; -1 when no file has that name. */
int32_t cardSize(const char *dir, const char *name);

/** @brief Up to @p cap bytes from the start of the file; the count, or -1 on a failure. */
int32_t cardRead(const char *dir, const char *name, char *buf, size_t cap);

/** @brief Receives one directory entry; @p name does not outlive the call. */
typedef void (*CardVisit)(const char *name, bool isDir, void *ctx);

/**
 * @brief Call @p visit for every entry of @p dir. A directory that does not
 *        exist has no entry. False when the directory does not read to its
 *        end.
 */
bool cardWalk(const char *dir, CardVisit visit, void *ctx);
