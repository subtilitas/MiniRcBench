/**
 * @file test_hub.cpp
 * @brief The hub, `ui_hub`, the one widget under ESC CONFIG, SERVO and RX
 *        SIGNALS: slices 1, 2 and 4 of menu structure C.
 *
 * Through the real UI: each of the three screens reports BACK and then one
 * control per entry that shows, at the entry's visible rectangle and
 * enabled as the hub draws it, is drawn again when it opens a second time,
 * and only SERVO's frame follows SERVO SUPPLY; HITEC CLASSIC, the one entry
 * with a gate, reports disabled without the switched supply and takes no
 * tap, and with it reports enabled and takes a press. Each entry opens the
 * screen of menu-c.md's table and returns to its hub.
 *
 * SERVO, the hub of five entries, scrolls: its range is 146 px; at scroll 0
 * its fourth entry is listed and does not act; at 146 px HITEC CLASSIC's
 * third line ends at y 316; a tool it opened returns to it at its scroll.
 *
 * On the widget itself, with a table of this file: the lines under an entry
 * in their order, the plain lines, then the power line or the lines that
 * say why; a repaint when a gate, the strip or the supply changes.
 *
 * hubEntriesFit(), on tables of this file: each rule of the fit check
 * passes at its limit and fails one step past it.
 */

#include "check.h"
#include "fakes.h"
#include "settings.h"
#include "ui.h"
#include "ui_angle.h"
#include "ui_esc_config.h"
#include "ui_hitec.h"
#include "ui_hub.h"
#include "ui_list.h"
#include "ui_menu.h"
#include "ui_registry.h"
#include "ui_rx_signals.h"
#include "ui_screen.h"
#include "ui_servo_hub.h"
#include "ui_kst.h"
#include "ui_am32.h"
#include "ui_bridge.h"
#include "ui_esc_cmd.h"
#include "ui_servo.h"
#include "ui_sbus.h"
#include "ui_sbus_out.h"
#include "ui_sdlog.h"
#include "servo_supply.h"
#include "sd_log.h"
#include "usb_msc.h"
#include "pin_claim.h"
#include "core1_job.h"
#include "esc_task.h"
#include "gfx.h"
#include "plat.h"
#include "touch.h"

#include <stdio.h>
#include <string.h>

/** @brief @p n UI frames of 25 ms, with core1's passes before each. */
static void hubFrames(int n) {
	for (int i = 0; i < n; i++) { fakeAdvance(25); fakePumpPoll(); core1JobPoll(); uiTick(); }
}
#define UI_IDS_FRAMES(n) hubFrames(n)
#include "ui_ids.h"

/** @brief The tester, from a fresh boot with the compiled defaults. */
static void freshTester() {
	// A claim outlives uiInitTester(); let the last test's claim end first.
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) hubFrames(1);
	fakeFlashClear(); settingsLoad(); uiInitTester();
	SdLogStatus st;
	memset(&st, 0, sizeof(st));
	st.state = SdLogState::NoCard;
	fakeSdLogSet(&st);
	fakeMscSet(MscState::Idle, 0);
	hubFrames(4);
}

/** @brief SERVO SUPPLY at @p mode: 0 OWN, 1 GP28, 2 GP29. */
static void setSupply(uint8_t mode) {
	settings()->servoSupply = mode;
	servoSupplyConfigure(mode);
}

/** @brief Whether the line at @p y reads @p text from x 8 in @p col, and nothing else is on its rows. */
static bool lineIs(int y, const char *text, uint16_t col) {
	int x0 = 0, x1 = 0;
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1) && fakeInkExtent(y, 7, C_BG, &x0, &x1) &&
	       x0 >= UI_MARGIN && x1 < UI_MARGIN + gfxTextW(text, 1);
}

/** @brief Whether the rows from @p y, @p h of them, hold nothing. */
static bool blank(int y, int h) {
	int x0 = 0, x1 = 0;
	return !fakeInkExtent(y, h, C_BG, &x0, &x1);
}

/** @brief Whether @p label is drawn on the button @p r, live or disabled. */
static bool buttonIs(const UiRect &r, const char *label, bool live) {
	return fakeTextIs(r.x + (r.w - gfxTextW(label, 2)) / 2, r.y + (r.h - 14) / 2, label,
	                  live ? C_TEXT : C_GRID, live ? C_PANEL : C_BG, 2);
}

// ---------------------------------------------------------------------------
// the three screens, through the real UI
// ---------------------------------------------------------------------------

/** @brief One of the three screens and its hub. */
struct HubCase {
	const char     *name;
	const UiScreen *screen;
	const UiHub    *hub;
	UiScreenId      id;
	int             count;
};

static const HubCase HUB_CASES[] = {
	{ "ESC CONFIG", &UI_ESC_CONFIG, &UI_ESC_CFG_HUB, UiScreenId::EscConfig, 3 },
	{ "SERVO", &UI_SERVO_HUB_SCREEN, &UI_SERVO_HUB, UiScreenId::ServoHub, 5 },
	{ "RX SIGNALS", &UI_RX_SIGNALS, &UI_RX_HUB, UiScreenId::RxSignals, 2 },
};

/**
 * @brief The control table of each hub at scroll 0: BACK, then each entry
 *        that shows, at its visible rectangle, 228 x 46 px at
 *        (6, 62 + 80 x i) cut at the panel's edge, enabled as
 *        uiHubEntryActs() says while 40 px of it show, and drawn so.
 */
static void testHubTables() {
	section("hub: the control tables of the three screens");
	for (const HubCase &c : HUB_CASES) {
		freshTester();
		setSupply(0);
		tapId(UI_FIX(TesterCtl::Menu));
		hubFrames(1);
		tapId(menuRow(c.screen));
		hubFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s opens, its title the hub's", c.name);
		checkTrue(what, uiCurrentScreen() == c.id && strcmp(c.hub->title, c.screen->title) == 0 &&
		                fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, c.hub->title, C_TEXT, C_PANEL, 2));
		snprintf(what, sizeof(what), "%s: its screen is the hub uiHubOf() names", c.name);
		checkTrue(what, uiHubOf(c.screen) == c.hub);
		snprintf(what, sizeof(what), "%s: entries in its table", c.name);
		checkInt(what, c.hub->count, c.count);
		// At scroll 0 entry i shows while its top, 62 + 80 x i, is on the
		// panel: every entry of a hub that fills the page, four of SERVO's.
		int shown = 0;
		for (int i = 0; i < c.hub->count; i++) shown += 62 + 80 * i < GFX_H;
		snprintf(what, sizeof(what), "%s: opens at scroll 0", c.name);
		checkInt(what, uiHubScroll(), 0);
		UiControl t[UI_CONTROLS_MAX];
		const uint8_t n = uiControls(t, UI_CONTROLS_MAX);
		snprintf(what, sizeof(what), "%s: BACK and one control per entry that shows", c.name);
		checkInt(what, n, shown + 1);
		const UiRect back = uiBackRect();
		snprintf(what, sizeof(what), "%s: BACK first, at its rectangle, acting", c.name);
		checkTrue(what, n > 0 && t[0].id == UI_FIX(UI_BACK) && t[0].enabled && t[0].rect.x == back.x &&
		                t[0].rect.y == back.y && t[0].rect.w == back.w && t[0].rect.h == back.h);
		for (int i = 0; i < shown && i + 1 < n; i++) {
			const UiControl &e = t[i + 1];
			const UiRect r = uiHubEntryRect(i);
			const char *label = c.hub->entries[i].label;
			const int top = 62 + 80 * i, h = top + 46 <= GFX_H ? 46 : GFX_H - top;
			snprintf(what, sizeof(what), "%s: %s is control %d, HUB_ENTRY(%d)", c.name, label, i + 1, i);
			checkTrue(what, e.id == HUB_ENTRY(i) && e.id == UI_FIX(1 + i));
			snprintf(what, sizeof(what), "%s: %s at (6, %d), 228 x %d px", c.name, label, top, h);
			checkTrue(what, e.rect.x == 6 && e.rect.y == top && e.rect.w == 228 && e.rect.h == h &&
			                r.x == e.rect.x && r.y == e.rect.y && r.w == e.rect.w && r.h == 46);
			const bool acts = uiHubEntryActs(*c.hub, i);
			if (h < 46) {
				// SERVO's fourth entry: 18 px show, less than the 40 px
				// press floor.
				snprintf(what, sizeof(what), "%s: %s, cut to %d px, is listed and does not act", c.name, label, h);
				checkTrue(what, acts && !e.enabled && h < UI_LIST_TAP_MIN);
				continue;
			}
			snprintf(what, sizeof(what), "%s: %s reports %s and is drawn so", c.name, label,
			         acts ? "enabled" : "disabled");
			checkTrue(what, e.enabled == acts && buttonIs(e.rect, label, acts));
			snprintf(what, sizeof(what), "%s: %s has no gate and acts", c.name, label);
			checkTrue(what, acts && c.hub->entries[i].enabled == nullptr);
		}
		checkControlTable(c.name);

		// Only SERVO's programmers carry a power line, and only SERVO's
		// strip names the supply: the other two screens draw the same
		// frame whatever SERVO SUPPLY is.
		bool power = false;
		for (int i = 0; i < c.hub->count; i++)
			power = power || c.hub->entries[i].powerOwn || c.hub->entries[i].powerSwitchedFmt;
		snprintf(what, sizeof(what), "%s: %s", c.name,
		         c.id == UiScreenId::ServoHub ? "its entries carry a power line" : "no entry carries a power line");
		checkTrue(what, power == (c.id == UiScreenId::ServoHub));
		const uint32_t own = fakeRegionHash(0, 0, GFX_W, GFX_H);
		setSupply(1);
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: with SERVO SUPPLY at GP28 the frame %s", c.name,
		         power ? "changes" : "is the same");
		checkTrue(what, (fakeRegionHash(0, 0, GFX_W, GFX_H) != own) == power);
		setSupply(0);
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: back at OWN, the first frame again", c.name);
		checkTrue(what, fakeRegionHash(0, 0, GFX_W, GFX_H) == own);

		tapId(UI_FIX(UI_BACK));
		snprintf(what, sizeof(what), "%s: BACK returns to MENU", c.name);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
		// The three screens share the widget's repaint key: the same hub
		// opened again, with nothing changed, is drawn again over MENU.
		hubFrames(1);
		tapId(menuRow(c.screen));
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: opened again, it is drawn again, the same frame", c.name);
		checkTrue(what, uiCurrentScreen() == c.id && fakeRegionHash(0, 0, GFX_W, GFX_H) == own);
		// A tap that starts and ends between two frames leaves through BACK
		// with no pressed frame drawn, so the repaint key is the frame's
		// own: the screen's entry point forces the next repaint.
		fakePress(back.x + back.w / 2, back.y + back.h / 2);
		fakeRelease();
		hubFrames(2);
		snprintf(what, sizeof(what), "%s: a tap within one frame on BACK returns to MENU", c.name);
		checkTrue(what, uiCurrentScreen() == UiScreenId::Menu);
		tapId(menuRow(c.screen));
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: opened after it, the same frame is drawn", c.name);
		checkTrue(what, uiCurrentScreen() == c.id && fakeRegionHash(0, 0, GFX_W, GFX_H) == own);
		tapId(UI_FIX(UI_BACK));
	}
}

/**
 * @brief HITEC CLASSIC, the entry with a gate: without the switched supply
 *        it reports disabled, is drawn disabled and takes no tap; with the
 *        supply it reports enabled and takes a press.
 */
static void testHubDisabledEntry() {
	section("hub: a disabled entry reports disabled and takes no tap");
	freshTester();
	setSupply(0);
	openChooser();
	const int cl = progEntryOf(uiHitecEnterClassic);
	checkTrue("SERVO is open, HITEC CLASSIC its last entry",
	          uiCurrentScreen() == UiScreenId::ServoHub && cl == UI_SERVO_ENTRIES_COUNT - 1);
	checkTrue("at scroll 0 it is not on the panel and not in the table", !ctlPresent(HUB_ENTRY(cl)));
	revealEntry(cl);
	checkTrue("dragged whole, by the 108 px its button lay below the panel",
	          uiHubScroll() == 108 && uiHubEntryRect(cl).y + uiHubEntryRect(cl).h == GFX_H);
	// To the end of the range, so that a slide up the panel scrolls nothing.
	hubScrollEnd();
	const UiRect r = uiHubEntryRect(cl);
	checkTrue("at the hub's end: its button at y 236", uiHubScroll() == 146 && r.y == 236 && r.h == 46);
	UiControl c;
	checkTrue("OWN: its gate fails", !uiHubEntryActs(UI_SERVO_HUB, cl) && !servoSupplyFitted());
	checkTrue("OWN: the table lists it at its rectangle, disabled",
	          ctlFind(HUB_ENTRY(cl), &c) && !c.enabled && c.rect.x == r.x && c.rect.y == r.y &&
	          c.rect.w == r.w && c.rect.h == r.h);
	checkTrue("OWN: it is drawn disabled", buttonIs(r, "HITEC CLASSIC", false));
	fakePress(r.x + r.w / 2, r.y + r.h / 2);
	hubFrames(1);
	checkTrue("OWN: a press draws no pressed look", buttonIs(r, "HITEC CLASSIC", false));
	fakeRelease();
	hubFrames(2);
	checkTrue("OWN: the tap opens nothing and claims nothing",
	          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended());
	checkTrue("the entries above it act", ctlEnabled(HUB_ENTRY(cl - 2)) && ctlEnabled(HUB_ENTRY(cl - 1)));

	setSupply(1);
	hubFrames(1);
	checkTrue("GP28: its gate holds", uiHubEntryActs(UI_SERVO_HUB, cl) && servoSupplyFitted());
	checkTrue("GP28: the table lists it enabled, on the next frame",
	          ctlFind(HUB_ENTRY(cl), &c) && c.enabled && c.rect.y == r.y);
	checkTrue("GP28: it is drawn live", buttonIs(r, "HITEC CLASSIC", true));
	{
		// Its tap opening HITEC SERVO is test_hitec_ui.cpp's; here the press
		// is taken and, slid off before the lift, opens nothing.
		const uint32_t rest = fakeRegionHash(r.x, r.y, r.w, r.h);
		fakePress(r.x + r.w / 2, r.y + r.h / 2);
		hubFrames(1);
		checkTrue("GP28: a press draws it pressed",
		          fakeRegionHash(r.x, r.y, r.w, r.h) != rest && uiCurrentScreen() == UiScreenId::ServoHub);
		fakeHold(r.x + r.w / 2, 30);
		hubFrames(1);
		fakeRelease();
		hubFrames(1);
		checkTrue("GP28: slid off before the lift, it opens nothing",
		          uiCurrentScreen() == UiScreenId::ServoHub && pinClaimState() == ClaimState::Free &&
		          fakeRegionHash(r.x, r.y, r.w, r.h) == rest);
	}
	tapId(UI_FIX(UI_BACK));
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	setSupply(0);
	freshTester();
}

// ---------------------------------------------------------------------------
// the widget, on a table of this file
// ---------------------------------------------------------------------------

static bool s_gate = false;
static bool gate() { return s_gate; }
static const char *s_strip = "STRIP ONE";
static void testStrip(char *buf, size_t n) { snprintf(buf, n, "%s", s_strip); }

/**
 * @brief Three entries: two plain lines; one line, a gate, a power line and
 *        one line that says why, in the second place; one line, a gate and
 *        two lines that say why.
 */
static constexpr UiHubEntry T_ENTRIES[] = {
	{ "ALPHA", { "FIRST PLAIN LINE", "SECOND PLAIN LINE" }, &UI_ANGLE, nullptr, nullptr,
	  { nullptr, nullptr }, nullptr, nullptr },
	{ "BRAVO", { "BRAVO MODELS", nullptr }, &UI_ANGLE, nullptr, gate, { nullptr, "BRAVO WHY" },
	  "POWER AT OWN.", "SWITCHED ON GP%u." },
	{ "CHARLIE", { "CHARLIE MODELS", nullptr }, &UI_ANGLE, nullptr, gate,
	  { "CHARLIE WHY ONE", "CHARLIE WHY TWO" }, nullptr, nullptr },
};
static constexpr UiHub T_HUB = { "TEST HUB", testStrip, T_ENTRIES, 3 };
static_assert(hubEntriesFit(T_HUB), "the test hub does not fit");

/** @brief One frame of the test hub, with no touch. */
static void hubTick() { checkTrue("the hub stays open", uiHubTick(T_HUB, nullptr)); }

/**
 * @brief The lines under an entry: the plain lines from 50 px under the
 *        entry's top at a 12 px pitch, then the power line by SERVO SUPPLY,
 *        or the amber lines that say why while the gate fails. A change of
 *        the gate, the strip or the supply repaints.
 */
static void testHubLines() {
	section("hub: the lines under an entry, by its gate and SERVO SUPPLY");
	freshTester();
	setSupply(0);
	s_gate = false;
	s_strip = "STRIP ONE";
	uiHubEnter(T_HUB);
	hubTick();
	checkTrue("the title at scale 2 from x 8",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "TEST HUB", C_TEXT, C_PANEL, 2));
	checkTrue("the strip, dim", fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "STRIP ONE", C_DIM, C_BG, 1));
	UiControl t[UI_CONTROLS_MAX];
	checkInt("BACK and three entries", uiHubControls(T_HUB, t, UI_CONTROLS_MAX), 4);
	checkTrue("ALPHA acts; BRAVO and CHARLIE, their gate failing, report disabled",
	          t[1].enabled && !t[2].enabled && !t[3].enabled && t[2].id == HUB_ENTRY(1) &&
	          t[3].rect.y == 222);
	checkInt("a table of two holds BACK and ALPHA", uiHubControls(T_HUB, t, 2), 2);
	checkTrue("ALPHA live, BRAVO and CHARLIE drawn disabled",
	          buttonIs(uiHubEntryRect(0), "ALPHA", true) && buttonIs(uiHubEntryRect(1), "BRAVO", false) &&
	          buttonIs(uiHubEntryRect(2), "CHARLIE", false));
	checkTrue("ALPHA: its two plain lines at y 112 and 124, dim",
	          lineIs(112, "FIRST PLAIN LINE", C_DIM) && lineIs(124, "SECOND PLAIN LINE", C_DIM) &&
	          blank(132, 10));
	checkTrue("BRAVO disabled: its line at y 192, dim, its why line at y 204, amber, no power line",
	          lineIs(192, "BRAVO MODELS", C_DIM) && lineIs(204, "BRAVO WHY", C_AMBER) && blank(212, 10));
	checkTrue("CHARLIE disabled: its line at y 272, its two why lines at y 284 and 296, to y 302",
	          lineIs(272, "CHARLIE MODELS", C_DIM) && lineIs(284, "CHARLIE WHY ONE", C_AMBER) &&
	          lineIs(296, "CHARLIE WHY TWO", C_AMBER) && blank(303, GFX_H - 303));

	s_gate = true;
	hubTick();
	checkInt("the gate holds: three entries", uiHubControls(T_HUB, t, UI_CONTROLS_MAX), 4);
	checkTrue("all three report enabled and are drawn live on the next frame",
	          t[1].enabled && t[2].enabled && t[3].enabled && buttonIs(uiHubEntryRect(1), "BRAVO", true) &&
	          buttonIs(uiHubEntryRect(2), "CHARLIE", true));
	checkTrue("BRAVO at OWN: the power line of OWN at y 204, dim, in the why line's place",
	          lineIs(192, "BRAVO MODELS", C_DIM) && lineIs(204, "POWER AT OWN.", C_DIM));
	checkTrue("CHARLIE, with no power line: its line and nothing under it",
	          lineIs(272, "CHARLIE MODELS", C_DIM) && blank(280, GFX_H - 280));
	checkInt("no amber is left", fakeCountColour(C_AMBER), 0);

	setSupply(1);
	hubTick();
	{
		char want[32];
		snprintf(want, sizeof(want), "SWITCHED ON GP%u.", (unsigned)servoSupplyPinOf(1));
		checkTrue("BRAVO at GP28: the switched power line with the pin, repainted",
		          lineIs(204, want, C_DIM));
	}
	checkTrue("ALPHA and CHARLIE gain no power line", blank(132, 10) && blank(280, GFX_H - 280));
	setSupply(2);
	hubTick();
	{
		char want[32];
		snprintf(want, sizeof(want), "SWITCHED ON GP%u.", (unsigned)servoSupplyPinOf(2));
		checkTrue("BRAVO at GP29: the line follows the pin", lineIs(204, want, C_DIM));
	}

	s_strip = "STRIP TWO";
	hubTick();
	checkTrue("a new strip text repaints the strip",
	          fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, "STRIP TWO", C_DIM, C_BG, 1));

	// An unchanged frame is not drawn again: a mark on the panel stays.
	gfxRect(100, 150, 4, 4, C_AMBER);
	hubTick();
	checkInt("nothing changed: no repaint", fakeCountColour(C_AMBER), 16);
	uiHubEnter(T_HUB);
	hubTick();
	checkInt("uiHubEnter() forces one", fakeCountColour(C_AMBER), 0);

	{
		// BACK, tapped, leaves; a press on it draws it pressed.
		const UiRect b = uiBackRect();
		TouchState p = {};
		p.down = true;
		p.x = p.downX = (int16_t)(b.x + b.w / 2);
		p.y = p.downY = (int16_t)(b.y + b.h / 2);
		const uint32_t rest = fakeRegionHash(b.x, b.y, b.w, b.h);
		checkTrue("a press on BACK keeps the hub open and draws BACK pressed",
		          uiHubTick(T_HUB, &p) && fakeRegionHash(b.x, b.y, b.w, b.h) != rest);
		p.down = false;
		p.released = true;
		checkTrue("the lift on BACK leaves the hub", !uiHubTick(T_HUB, &p));
	}
	setSupply(0);
	s_gate = false;
	freshTester();
}

// ---------------------------------------------------------------------------
// hubEntriesFit()
// ---------------------------------------------------------------------------

#define L18 "ABCDEFGHIJKLMNOPQR"
#define L37 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789."
#define NONE2 { nullptr, nullptr }

/** @brief A plain entry labelled @p label with the lines @p a and @p b. */
static constexpr UiHubEntry plain(const char *label, const char *a, const char *b) {
	return UiHubEntry{ label, { a, b }, &UI_ANGLE, nullptr, nullptr, NONE2, nullptr, nullptr };
}

/** @brief Whether a hub of the one entry @p e fits. */
static constexpr bool fits1(const UiHubEntry &e) {
	const UiHubEntry t[1] = { e };
	return hubEntriesFit(UiHub{ "T", nullptr, t, 1 });
}

/** @brief Whether a hub of @p e and then a plain entry fits: @p e has an entry under it. */
static constexpr bool fits2(const UiHubEntry &e) {
	const UiHubEntry t[2] = { e, plain("LAST", nullptr, nullptr) };
	return hubEntriesFit(UiHub{ "T", nullptr, t, 2 });
}

/** @brief Whether a hub of @p n plain entries fits. */
static constexpr bool fitsN(int n) {
	const UiHubEntry e = plain("A", "X", "Y");
	const UiHubEntry t[9] = { e, e, e, e, e, e, e, e, e };
	return hubEntriesFit(UiHub{ "T", nullptr, t, (uint8_t)n });
}

static bool yes() { return true; }

/** @brief Each rule of hubEntriesFit() at its limit and one step past it. */
static void testHubFit() {
	section("hub: hubEntriesFit(), each rule at its limit");
	checkTrue("the three tables fit", hubEntriesFit(UI_SERVO_HUB) && hubEntriesFit(UI_ESC_CFG_HUB) &&
	          hubEntriesFit(UI_RX_HUB));
	checkTrue("the limits: 18 and 37 characters", sizeof(L18) - 1 == UI_HUB_LABEL_CHARS &&
	          sizeof(L37) - 1 == UI_HUB_LINE_CHARS && gfxTextW(L18, 2) == 214 && gfxTextW(L37, 1) == 221);

	checkTrue("1 to 8 entries fit, more than 3 as a list; 0 and 9 do not",
	          fitsN(1) && fitsN(2) && fitsN(3) && fitsN(4) && fitsN(5) && fitsN(8) && !fitsN(0) &&
	          !fitsN(9) && UI_HUB_MAX_ENTRIES == 8);
	checkTrue("the content: 80 px an entry and 4 px, 244 px for three and 404 px for five",
	          uiHubContentPx(3) == 244 && uiHubContentPx(5) == 404 && uiHubContentPx(3) <= GFX_H - 62 &&
	          uiHubContentPx(4) > GFX_H - 62);
	checkTrue("a label of 18 characters fits, of 19 not",
	          fits1(plain(L18, nullptr, nullptr)) && !fits1(plain(L18 "S", nullptr, nullptr)));
	checkTrue("an entry with no label, or an empty one, does not fit",
	          !fits1(plain(nullptr, nullptr, nullptr)) && !fits1(plain("", nullptr, nullptr)));
	checkTrue("an entry with no screen does not fit",
	          !fits1(UiHubEntry{ "A", NONE2, nullptr, nullptr, nullptr, NONE2, nullptr, nullptr }));
	checkTrue("a first line of 37 characters fits, of 38 not",
	          fits1(plain("A", L37, nullptr)) && !fits1(plain("A", L37 "X", nullptr)));
	checkTrue("a second line of 37 characters fits, of 38 not",
	          fits1(plain("A", "X", L37)) && !fits1(plain("A", "X", L37 "X")));
	checkTrue("a second line with no first does not fit", !fits1(plain("A", nullptr, "X")));

	const UiHubEntry why37 = { "A", NONE2, &UI_ANGLE, nullptr, yes, { L37, L37 }, nullptr, nullptr };
	const UiHubEntry why38a = { "A", NONE2, &UI_ANGLE, nullptr, yes, { L37 "X", nullptr }, nullptr, nullptr };
	const UiHubEntry why38b = { "A", NONE2, &UI_ANGLE, nullptr, yes, { "X", L37 "X" }, nullptr, nullptr };
	checkTrue("why lines of 37 characters fit, a first or a second of 38 not",
	          fits1(why37) && !fits1(why38a) && !fits1(why38b));
	const UiHubEntry gateNoWhy = { "A", NONE2, &UI_ANGLE, nullptr, yes, NONE2, nullptr, nullptr };
	const UiHubEntry gateWhy2 = { "A", NONE2, &UI_ANGLE, nullptr, yes, { nullptr, "X" }, nullptr, nullptr };
	checkTrue("a gate with no line that says why does not fit; with one, in either place, it does",
	          !fits1(gateNoWhy) && fits1(gateWhy2));

	const UiHubEntry pow37 = { "A", NONE2, &UI_ANGLE, nullptr, nullptr, NONE2, L37, L37 };
	const UiHubEntry powOwn38 = { "A", NONE2, &UI_ANGLE, nullptr, nullptr, NONE2, L37 "X", L37 };
	const UiHubEntry powSw38 = { "A", NONE2, &UI_ANGLE, nullptr, nullptr, NONE2, L37, L37 "X" };
	const UiHubEntry powOwnOnly = { "A", NONE2, &UI_ANGLE, nullptr, nullptr, NONE2, "X", nullptr };
	const UiHubEntry powSwOnly = { "A", NONE2, &UI_ANGLE, nullptr, nullptr, NONE2, nullptr, "X" };
	checkTrue("power lines of 37 characters fit, either of 38 not",
	          fits1(pow37) && !fits1(powOwn38) && !fits1(powSw38));
	checkTrue("a power line at OWN needs one for the switched supply, not the reverse",
	          !fits1(powOwnOnly) && fits1(powSwOnly));

	// How many lines end where: line k of an entry ends 50 + 12 x k + 7 px
	// under its top, the next entry starts at 80, and the content ends
	// 84 px under the last entry's top.
	const UiHubEntry two = plain("A", "X", "Y");
	const UiHubEntry onePower = { "A", { "X", nullptr }, &UI_ANGLE, nullptr, nullptr, NONE2, "P", "P" };
	const UiHubEntry twoPower = { "A", { "X", "Y" }, &UI_ANGLE, nullptr, nullptr, NONE2, "P", "P" };
	const UiHubEntry oneWhy2 = { "A", { "X", nullptr }, &UI_ANGLE, nullptr, yes, { "W", "W" }, nullptr, nullptr };
	const UiHubEntry oneWhy2Power = { "A", { "X", nullptr }, &UI_ANGLE, nullptr, yes, { "W", "W" }, nullptr, "P" };
	const UiHubEntry oneWhy1Power = { "A", { "X", nullptr }, &UI_ANGLE, nullptr, yes, { "W", nullptr }, "P", "P" };
	const UiHubEntry twoWhy2 = { "A", { "X", "Y" }, &UI_ANGLE, nullptr, yes, { "W", "W" }, nullptr, nullptr };
	checkTrue("two lines fit over another entry: plain, or one and the power line",
	          fits2(two) && fits2(onePower) && fits2(oneWhy1Power));
	checkTrue("three lines do not fit over another entry", !fits2(twoPower) && !fits2(oneWhy2) &&
	          !fits2(oneWhy2Power));
	checkTrue("three lines fit under the last entry", fits1(twoPower) && fits1(oneWhy2) &&
	          fits1(oneWhy2Power));
	{
		// The last of three: the third line ends at y 222 + 50 + 24 + 7 = 303.
		const UiHubEntry t[3] = { two, two, oneWhy2 };
		const UiHubEntry u[3] = { two, oneWhy2, two };
		checkTrue("three lines under the third entry fit, under the second not",
		          hubEntriesFit(UiHub{ "T", nullptr, t, 3 }) && !hubEntriesFit(UiHub{ "T", nullptr, u, 3 }));
	}
	{
		// The last of five, a list: its third line ends 81 px under its
		// top, inside the content's 84 px; under the fourth it does not fit.
		const UiHubEntry t[5] = { two, two, two, two, oneWhy2 };
		const UiHubEntry u[5] = { two, two, two, oneWhy2, two };
		checkTrue("three lines under the fifth entry of a list fit, under the fourth not",
		          hubEntriesFit(UiHub{ "T", nullptr, t, 5 }) && !hubEntriesFit(UiHub{ "T", nullptr, u, 5 }));
	}
	{
		// A fourth line would end 93 px under the last entry's top, past
		// the content's 84 px: a scrolling hub could not show it.
		const UiHubEntry t[3] = { two, two, twoWhy2 };
		checkTrue("four lines fit under no entry, the last among them",
		          !fits1(twoWhy2) && !fits2(twoWhy2) && !hubEntriesFit(UiHub{ "T", nullptr, t, 3 }));
	}
}

/**
 * @brief docs/design/menu-c.md slice 2: a hub opens while the pin claim
 *        is not Free, and draws every entry whose tool claims, or opens only
 *        while the pins are free, disabled: each reports disabled, takes no
 *        tap and keeps its own lines, and the strip says why in amber. Once
 *        Free, each entry acts again by its own gate.
 */
static void testHubClaimGate() {
	section("hub: opened while the claim is not Free, its entries drawn disabled");
	for (const HubCase &c : HUB_CASES) {
		char what[128];
		freshTester();
		setSupply(1);
		tapId(UI_FIX(TesterCtl::Menu));
		hubFrames(1);
		checkTrue("another claim taken under MENU",
		          pinClaimBegin(PinUser::Am32, PIN_SIG) == ClaimRefusal::None);
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: its MENU row acts while the claim is not Free", c.name);
		checkTrue(what, !c.screen->freePins && c.screen->pinUser == PinUser::None &&
		                ctlEnabled(menuRow(c.screen)));
		tapId(menuRow(c.screen));
		hubFrames(1);
		snprintf(what, sizeof(what), "%s opens: MENU, %s", c.name, c.name);
		checkTrue(what, navIsHub(c.id) && pinClaimState() != ClaimState::Free);
		char busy[40];
		snprintf(busy, sizeof(busy), UI_MENU_RESTARTING_FMT, (unsigned)settings()->dshotPin);
		snprintf(what, sizeof(what), "%s: the strip reads %s, amber", c.name, busy);
		checkTrue(what, fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, busy, UI_COL_WARN, C_BG, 1));
		for (int i = 0; i < c.hub->count; i++) {
			const UiHubEntry &e = c.hub->entries[i];
			snprintf(what, sizeof(what), "%s: %s is refused by uiPinsRefuse()", c.name, e.label);
			checkTrue(what, uiPinsRefuse(e.screen));
			// A scrolling hub: the entry dragged whole first, and to the
			// hub's end where its lines would lie under the panel's edge.
			revealEntry(i);
			if (uiHubEntryRect(i).y + UI_HUB_LINE_DY + 3 * UI_HUB_LINE_H > GFX_H) hubScrollEnd();
			UiControl ctl;
			snprintf(what, sizeof(what), "%s: %s reports disabled and is drawn so", c.name, e.label);
			checkTrue(what, ctlFind(HUB_ENTRY(i), &ctl) && !ctl.enabled &&
			                buttonIs(uiHubEntryRect(i), e.label, false));
			const int y = uiHubEntryRect(i).y + UI_HUB_LINE_DY;
			if (e.lines[0]) {
				snprintf(what, sizeof(what), "%s: %s keeps its first line, dim", c.name, e.label);
				checkTrue(what, fakeTextIs(UI_MARGIN, y, e.lines[0], C_DIM, C_BG, 1));
			}
			if (e.why[0]) {
				// HITEC CLASSIC: its own gate holds with the supply, so no
				// line says it needs the converter.
				const int wy = y + (e.lines[0] ? UI_HUB_LINE_H : 0);
				snprintf(what, sizeof(what), "%s: %s draws no why line: the claim, not its gate, refuses",
				         c.name, e.label);
				checkTrue(what, !fakeTextIs(UI_MARGIN, wy, e.why[0], UI_COL_WARN, C_BG, 1));
			}
			tapId(HUB_ENTRY(i), IdExpect::Disabled);
			hubFrames(1);
			snprintf(what, sizeof(what), "%s: a tap on %s opens nothing", c.name, e.label);
			checkTrue(what, navIsHub(c.id) && pinClaimHolder() == PinUser::Am32);
		}
		checkControlTable(c.name);
		pinClaimEnd(PinUser::Am32);
		for (int k = 0; k < 40 && pinClaimState() != ClaimState::Free; k++) hubFrames(1);
		hubFrames(1);
		for (int i = 0; i < c.hub->count; i++) {
			snprintf(what, sizeof(what), "%s, the claim Free: %s acts again", c.name,
			         c.hub->entries[i].label);
			checkTrue(what, ctlEnabled(hubEntry(i)) && uiHubEntryActs(*c.hub, i));
		}
		char strip[48];
		c.hub->strip(strip, sizeof(strip));
		snprintf(what, sizeof(what), "%s: the strip its own again, dim", c.name);
		checkTrue(what, fakeTextIs(UI_MARGIN, UI_STRIP_Y + 4, strip, C_DIM, C_BG, 1));
		checkTrue("BACK: MENU, then the tester", backToTester());
	}
	setSupply(0);
	freshTester();
}

/**
 * @brief The stack holds at most three screens, MENU, the root, at the
 *        bottom: MENU, a hub, its tool. A tool that asks to open another
 *        screen at that depth is refused and stays open.
 */
static void testNavDepth() {
	section("navigation: at most three screens, MENU at the bottom");
	freshTester();
	checkTrue("the tester: over MENU, two screens",
	          uiNavDepth() == 2 && uiNavAt(0) == UiScreenId::Menu && uiNavAt(1) == UiScreenId::Tester);
	openChooser();
	tapId(hubEntry(progEntryOf(uiKstEnter)));
	hubFrames(2);
	checkTrue("MENU, SERVO, KST SERVO: three, the limit",
	          uiNavDepth() == UI_NAV_MAX && navIsTool(UiScreenId::ServoHub, UiScreenId::Kst));
	uiChooseTool(&UI_SDLOG, nullptr);
	hubFrames(1);
	checkTrue("a fourth screen asked for is refused: KST SERVO stays open",
	          uiNavDepth() == UI_NAV_MAX && uiCurrentScreen() == UiScreenId::Kst);
	checkTrue("uiNavAt() past the top reads MENU, the root", uiNavAt(UI_NAV_MAX) == UiScreenId::Menu);
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) {
		if (ctlEnabled(UI_FIX(KstCtl::Leave))) tapId(UI_FIX(KstCtl::Leave));
		if (ctlEnabled(UI_FIX(KstCtl::Restart))) tapId(UI_FIX(KstCtl::Restart));
		hubFrames(1);
	}
	checkTrue("KST SERVO left: SERVO", navIsHub(UiScreenId::ServoHub));
	checkTrue("then MENU and the tester", backToTester());
	freshTester();
}

// ---------------------------------------------------------------------------
// SERVO, the hub that scrolls (docs/design/menu-c.md, slice 4)
// ---------------------------------------------------------------------------

/** @brief Whether any pixel of the rectangle is not `C_BG`. */
static bool inkIn(int x, int y, int w, int h) {
	const uint16_t *fb = gfxBuffer();
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			if (fb[j * GFX_W + i] != C_BG) return true;
	return false;
}

/** @brief Whether the hub's line at @p y reads @p text from x 8 in @p col. */
static bool hubLine(int y, const char *text, uint16_t col) {
	return fakeTextIs(UI_MARGIN, y, text, col, C_BG, 1);
}

/** @brief One drag on the viewport: the finger down at y 190, moved by @p dy, lifted. */
static void dragHub(int dy) {
	fakePress(120, 190); hubFrames(1);
	fakeHold(120, 190 + dy); hubFrames(1);
	fakeRelease(); hubFrames(1);
}

/**
 * @brief Drag the open hub to @p px of scroll. The list follows the finger
 *        1:1 from the press point once the axis locks, after 6 px of
 *        travel, so a shorter move goes 20 px away first.
 */
static void scrollHubTo(int px) {
	for (int pass = 0; pass < 8 && uiHubScroll() != px; pass++) {
		int dy = uiHubScroll() - px;
		if (dy >= -UI_AXIS_LOCK_PX && dy <= UI_AXIS_LOCK_PX) dy += px >= 20 ? 20 : -20;
		if (dy > 120) dy = 120;
		if (dy < -120) dy = -120;
		dragHub(dy);
	}
}

/**
 * @brief SERVO: five entries, 404 px of content in the 258 px under the
 *        strip, a range of 146 px. At scroll 0 three entries are whole and
 *        HITEC D SERIES shows 18 px, listed and not acting; it acts from
 *        22 px of scroll, is whole from 28 px, HITEC CLASSIC from 108 px;
 *        at 146 px HITEC CLASSIC's third line ends at y 316. A drag opens
 *        nothing, and a tool SERVO opened returns to it at its scroll.
 */
static void testServoScroll() {
	section("hub: SERVO scrolls 146 px on ui_list");
	freshTester();
	setSupply(0);
	openChooser();
	hubFrames(1);
	checkTrue("SERVO is open over MENU", navIsHub(UiScreenId::ServoHub));
	checkInt("five entries", UI_SERVO_ENTRIES_COUNT, 5);
	checkInt("the content: 5 x 80 + 4 px", uiHubContentPx(UI_SERVO_ENTRIES_COUNT), 404);
	checkInt("the scroll range: 404 - 258 px", uiHubMaxScroll(), 146);
	checkInt("it opens at scroll 0", uiHubScroll(), 0);
	checkTrue("the title SERVO, 58 px at scale 2 from x 8, ends at x 66, 4 px or more before BACK",
	          gfxTextW(UI_SERVO_HUB_TITLE, 2) == 58 && UI_MARGIN + 58 + 4 <= UI_BACK_X &&
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SERVO", C_TEXT, C_PANEL, 2));
	checkTrue("the strip at OWN: SIG GP0  AUX GP1  SERVO SUPPLY OWN, dim",
	          hubLine(UI_STRIP_Y + 4, "SIG GP0  AUX GP1  SERVO SUPPLY OWN", C_DIM));
	checkTrue("the strip at its widest, 35 characters, 209 px, ends at x 217",
	          strlen("SIG GP0  AUX GP1  SERVO SUPPLY GP28") == 35 &&
	          gfxTextW("SIG GP0  AUX GP1  SERVO SUPPLY GP28", 1) == 209 && UI_MARGIN + 209 == 217);

	// Scroll 0: the picture of menu-c.md.
	checkTrue("scroll 0: SERVO TESTER at y 62, its line at y 112, 34 characters, 203 px",
	          buttonIs(UiRect{ 6, 62, 228, 46 }, "SERVO TESTER", true) &&
	          hubLine(112, "ONE SERVO ON SIG, OR TWO WITH AUX.", C_DIM) &&
	          gfxTextW("ONE SERVO ON SIG, OR TWO WITH AUX.", 1) == 203);
	checkTrue("scroll 0: ANGLE METER at y 142, its line at y 192",
	          buttonIs(UiRect{ 6, 142, 228, 46 }, "ANGLE METER", true) &&
	          hubLine(192, "INCIDENCE, DECALAGE, DIHEDRAL, THROW", C_DIM));
	checkTrue("scroll 0: KST / CHASERVO at y 222, its power line at y 272",
	          buttonIs(UiRect{ 6, 222, 228, 46 }, "KST / CHASERVO", true) &&
	          hubLine(272, "POWER THE SERVO UP AFTER THE TAP.", C_DIM));
	UiControl c;
	checkTrue("scroll 0: entry 3, HITEC D SERIES, is listed at (6, 302), 228 x 18 px, and does not act",
	          ctlFind(HUB_ENTRY(3), &c) && c.rect.x == 6 && c.rect.y == 302 && c.rect.w == 228 &&
	          c.rect.h == 18 && !c.enabled && uiHubEntryActs(UI_SERVO_HUB, 3));
	checkTrue("its 18 px are drawn: the button's frame at y 302, its fill under it",
	          gfxBuffer()[302 * GFX_W + 120] == C_GRID && gfxBuffer()[312 * GFX_W + 12] == C_PANEL);
	checkTrue("scroll 0: entry 4, HITEC CLASSIC, is not in the table", !ctlPresent(HUB_ENTRY(4)));
	checkTrue("the three whole entries act", ctlEnabled(HUB_ENTRY(0)) && ctlEnabled(HUB_ENTRY(1)) &&
	          ctlEnabled(HUB_ENTRY(2)));
	{
		// The bar: 3 px at x 235, 258 x 258 / 404 = 164 px long, from y 62.
		const uint16_t *fb = gfxBuffer();
		checkTrue("the scroll bar at x 235 to 237, y 62 to 225, C_GRID",
		          fb[62 * GFX_W + 235] == C_GRID && fb[225 * GFX_W + 237] == C_GRID &&
		          fb[226 * GFX_W + 236] == C_BG && fb[100 * GFX_W + 234] == C_BG &&
		          fb[100 * GFX_W + 238] == C_BG);
	}
	checkControlTable("SERVO at scroll 0");
	fakeDumpFrame("shot_servo_hub.ppm");
	{
		// A press on the 18 px that show: no pressed look, and nothing opens.
		const uint32_t rest = fakeRegionHash(0, 0, GFX_W, GFX_H);
		fakePress(120, 310);
		hubFrames(2);
		checkTrue("a press on the cut entry draws nothing", fakeRegionHash(0, 0, GFX_W, GFX_H) == rest);
		fakeRelease();
		hubFrames(2);
		checkTrue("and its tap opens nothing", navIsHub(UiScreenId::ServoHub) &&
		          pinClaimState() == ClaimState::Free && uiHubScroll() == 0);
	}
	{
		// A drag that begins on an entry scrolls and opens nothing.
		fakePress(120, 245);
		hubFrames(1);
		checkTrue("a press on KST / CHASERVO draws it pressed",
		          !buttonIs(UiRect{ 6, 222, 228, 46 }, "KST / CHASERVO", true));
		fakeHold(120, 245 - 5);
		hubFrames(1);
		checkInt("5 px of travel: no scroll yet", uiHubScroll(), 0);
		fakeHold(120, 245 - 30);
		hubFrames(1);
		checkInt("30 px of travel: the hub follows the finger, 30 px", uiHubScroll(), 30);
		checkTrue("the pressed look is gone: the press became a drag",
		          buttonIs(UiRect{ 6, 222 - 30, 228, 46 }, "KST / CHASERVO", true));
		fakeRelease();
		hubFrames(2);
		checkTrue("the lift opens nothing and claims nothing", navIsHub(UiScreenId::ServoHub) &&
		          pinClaimState() == ClaimState::Free && uiHubScroll() == 30);
	}

	// The press floor and the whole entries, by the scroll.
	scrollHubTo(21);
	checkTrue("at 21 px HITEC D SERIES shows 39 px and does not act",
	          uiHubScroll() == 21 && ctlFind(HUB_ENTRY(3), &c) && c.rect.h == 39 && !c.enabled);
	scrollHubTo(22);
	checkTrue("at 22 px it shows 40 px, the press floor, and acts",
	          uiHubScroll() == 22 && ctlFind(HUB_ENTRY(3), &c) && c.rect.h == 40 && c.enabled);
	scrollHubTo(27);
	checkTrue("at 27 px it is cut by 1 px", ctlFind(HUB_ENTRY(3), &c) && c.rect.h == 45);
	scrollHubTo(28);
	checkTrue("at 28 px it is whole: its button ends at y 320",
	          ctlFind(HUB_ENTRY(3), &c) && c.rect.h == 46 && c.rect.y + c.rect.h == GFX_H && c.enabled);
	scrollHubTo(107);
	checkTrue("at 107 px HITEC CLASSIC is cut by 1 px", ctlFind(HUB_ENTRY(4), &c) && c.rect.h == 45);
	scrollHubTo(108);
	checkTrue("at 108 px it is whole, drawn disabled at OWN",
	          ctlFind(HUB_ENTRY(4), &c) && c.rect.h == 46 && c.rect.y + c.rect.h == GFX_H && !c.enabled);

	// The full scroll: a drag past the end stops at 146 px.
	fakePress(120, 300); hubFrames(1);
	fakeHold(120, 60); hubFrames(1);
	fakeRelease(); hubFrames(2);
	checkInt("a drag past the end stops at the range, 146 px", uiHubScroll(), 146);
	checkTrue("146 px: SERVO TESTER and ANGLE METER are above, not in the table",
	          !ctlPresent(HUB_ENTRY(0)) && !ctlPresent(HUB_ENTRY(1)) && !inkIn(0, 62, 235, 14));
	checkTrue("146 px: KST / CHASERVO at y 76, its power line at y 126",
	          ctlFind(HUB_ENTRY(2), &c) && c.rect.y == 76 && c.rect.h == 46 && c.enabled &&
	          buttonIs(c.rect, "KST / CHASERVO", true) && hubLine(126, "POWER THE SERVO UP AFTER THE TAP.", C_DIM));
	checkTrue("146 px: HITEC D SERIES at y 156, its lines at y 206 and 218",
	          ctlFind(HUB_ENTRY(3), &c) && c.rect.y == 156 && c.enabled &&
	          buttonIs(c.rect, "HITEC D SERIES", true) && hubLine(206, "D485HW, D645MW AND THE OTHERS", C_DIM) &&
	          hubLine(218, "POWER THE SERVO UP AFTER THE TAP.", C_DIM));
	checkTrue("146 px: HITEC CLASSIC at y 236, disabled at OWN, its lines at y 286, 298 and 310",
	          ctlFind(HUB_ENTRY(4), &c) && c.rect.y == 236 && c.rect.h == 46 && !c.enabled &&
	          buttonIs(c.rect, "HITEC CLASSIC", false) && hubLine(286, "HS-5XXX, HS-7XXX", C_DIM) &&
	          hubLine(298, "NEEDS THE CONVERTER OF WIRING PART C", C_AMBER) &&
	          hubLine(310, UI_SERVO_ENTRIES[4].why[1], C_AMBER));
	checkTrue("146 px: HITEC CLASSIC's third line ends at y 316, with 3 px free under it",
	          inkIn(UI_MARGIN, 316, 221, 1) && !inkIn(0, 317, 235, 3));
	{
		// The bar at its end: (258 - 164) px down, to the viewport's last row.
		const uint16_t *fb = gfxBuffer();
		checkTrue("the scroll bar at y 156 to 319", fb[155 * GFX_W + 236] == C_BG &&
		          fb[156 * GFX_W + 236] == C_GRID && fb[319 * GFX_W + 236] == C_GRID);
	}
	checkTrue("the header and the strip stay: the list is clipped under them",
	          fakeTextIs(UI_MARGIN, (UI_HDR_H - 14) / 2, "SERVO", C_TEXT, C_PANEL, 2) &&
	          hubLine(UI_STRIP_Y + 4, "SIG GP0  AUX GP1  SERVO SUPPLY OWN", C_DIM));
	checkControlTable("SERVO at 146 px");
	fakeDumpFrame("shot_servo_hub_scrolled.ppm");
	tapId(HUB_ENTRY(4), IdExpect::Disabled);
	checkTrue("HITEC CLASSIC at OWN, whole: a tap opens nothing", navIsHub(UiScreenId::ServoHub));

	// A tool opened from the scrolled hub returns to the hub as it was left.
	const uint32_t left = fakeRegionHash(0, 0, GFX_W, GFX_H);
	tapId(HUB_ENTRY(2));
	hubFrames(2);
	checkTrue("KST / CHASERVO, tapped at 146 px, opens KST SERVO",
	          navIsTool(UiScreenId::ServoHub, UiScreenId::Kst));
	tapId(UI_FIX(UI_BACK));
	for (int k = 0; k < 200 && (uiCurrentScreen() == UiScreenId::Kst || pinClaimState() != ClaimState::Free); k++) {
		if (ctlEnabled(UI_FIX(KstCtl::Leave))) tapId(UI_FIX(KstCtl::Leave));
		if (ctlEnabled(UI_FIX(KstCtl::Restart))) tapId(UI_FIX(KstCtl::Restart));
		hubFrames(1);
	}
	hubFrames(1);
	checkTrue("KST SERVO left: SERVO, the claim Free", navIsHub(UiScreenId::ServoHub) &&
	          pinClaimState() == ClaimState::Free);
	checkInt("SERVO kept its scroll, 146 px", uiHubScroll(), 146);
	checkTrue("the entry that was tapped is where it was, and the frame is the one that was left",
	          ctlFind(HUB_ENTRY(2), &c) && c.rect.y == 76 && fakeRegionHash(0, 0, GFX_W, GFX_H) == left);

	// A drag down past the top stops at 0; MENU opens SERVO at 0 again.
	fakePress(120, 100); hubFrames(1);
	fakeHold(120, 310); hubFrames(1);
	fakeRelease(); hubFrames(2);
	checkInt("a drag past the top stops at 0 px", uiHubScroll(), 0);
	scrollHubTo(90);
	tapId(UI_FIX(UI_BACK));
	hubFrames(1);
	checkTrue("BACK: MENU", uiCurrentScreen() == UiScreenId::Menu);
	tapId(menuRow(&UI_SERVO_HUB_SCREEN));
	hubFrames(1);
	checkTrue("SERVO opened from MENU again starts at scroll 0", navIsHub(UiScreenId::ServoHub) &&
	          uiHubScroll() == 0 && ctlEnabled(HUB_ENTRY(0)));

	// With the switched supply the strip names its pin and HITEC CLASSIC acts.
	setSupply(1);
	hubFrames(1);
	{
		char strip[48];
		snprintf(strip, sizeof(strip), "SIG GP0  AUX GP1  SERVO SUPPLY GP%u", (unsigned)servoSupplyPinOf(1));
		checkTrue("the strip names the supply's pin", hubLine(UI_STRIP_Y + 4, strip, C_DIM));
	}
	revealEntry(4);
	checkTrue("HITEC CLASSIC with the supply: whole, live, its power line in the why lines' place",
	          ctlEnabled(HUB_ENTRY(4)) && buttonIs(uiHubEntryRect(4), "HITEC CLASSIC", true) &&
	          fakeCountColour(C_AMBER) == 0);
	checkTrue("BACK: MENU, then the tester", backToTester());

	// The two hubs that fill the page do not scroll.
	static const struct { const UiScreen *screen; UiScreenId id; int content; } FLAT[] = {
		{ &UI_ESC_CONFIG, UiScreenId::EscConfig, 244 },
		{ &UI_RX_SIGNALS, UiScreenId::RxSignals, 164 },
	};
	for (const auto &f : FLAT) {
		tapId(UI_FIX(TesterCtl::Menu));
		hubFrames(1);
		tapId(menuRow(f.screen));
		hubFrames(1);
		char what[96];
		snprintf(what, sizeof(what), "%s: %d px of content, no scroll range", f.screen->title, f.content);
		checkTrue(what, navIsHub(f.id) && uiHubMaxScroll() == 0 &&
		                uiHubContentPx(uiHubOf(f.screen)->count) == f.content);
		const uint32_t rest = fakeRegionHash(0, 0, GFX_W, GFX_H);
		fakePress(120, 300); hubFrames(1);
		fakeHold(120, 200); hubFrames(1);
		fakeRelease(); hubFrames(2);
		snprintf(what, sizeof(what), "%s: a drag moves nothing and draws no bar", f.screen->title);
		checkTrue(what, uiHubScroll() == 0 && fakeRegionHash(0, 0, GFX_W, GFX_H) == rest &&
		                !inkIn(UI_HUB_BAR_X, 62, GFX_W - UI_HUB_BAR_X, 258));
		if (f.id == UiScreenId::EscConfig) {
			// A hub that fills the page takes a tap as a button does: a
			// press that moves 10 px, more than the list's 6 px axis lock,
			// and lifts inside the button still opens the entry.
			const UiRect r = uiHubEntryRect(0);
			fakePress(r.x + r.w / 2, r.y + 12); hubFrames(1);
			fakeHold(r.x + r.w / 2, r.y + 22); hubFrames(1);
			checkTrue("ESC CONFIG: a press moved 10 px inside ESC COMMANDS stays a press, drawn pressed",
			          navIsHub(UiScreenId::EscConfig) && !buttonIs(r, "ESC COMMANDS", true));
			fakeRelease(); hubFrames(2);
			checkTrue("and its lift inside the button opens ESC COMMANDS",
			          navIsTool(UiScreenId::EscConfig, UiScreenId::EscCmd));
			tapId(UI_FIX(UI_BACK));
			hubFrames(1);
			checkTrue("BACK: ESC CONFIG", navIsHub(UiScreenId::EscConfig));
		}
		checkTrue("BACK: MENU, then the tester", backToTester());
	}
	{
		// The same move on SERVO, which scrolls, is a drag: it scrolls 10 px
		// and opens nothing.
		tapId(UI_FIX(TesterCtl::Menu));
		hubFrames(1);
		tapId(menuRow(&UI_SERVO_HUB_SCREEN));
		hubFrames(1);
		const UiRect r = uiHubEntryRect(1);
		fakePress(r.x + r.w / 2, r.y + 22); hubFrames(1);
		fakeHold(r.x + r.w / 2, r.y + 12); hubFrames(1);
		fakeRelease(); hubFrames(2);
		checkTrue("SERVO: a press moved 10 px inside ANGLE METER scrolls 10 px and opens nothing",
		          navIsHub(UiScreenId::ServoHub) && uiHubScroll() == 10);
		checkTrue("BACK: MENU, then the tester", backToTester());
	}
	setSupply(0);
	freshTester();
}

/**
 * @brief Each hub's entries open the screens of menu-c.md's table "Every
 *        screen, old place and new place", each over its hub, and each
 *        returns to its hub.
 */
static void testHubOpens() {
	section("hub: each entry opens its screen and returns to its hub");
	static const struct {
		const UiScreen *hub;
		UiScreenId      hubId;
		int             entry;
		const char     *label;
		const UiScreen *tool;
		void          (*enter)();
		int             leave;    /**< LEAVE on the tool's leave page, or -1. */
		int             restart;  /**< RESTART DSHOT there, or -1. */
	} WANT[] = {
		{ &UI_ESC_CONFIG, UiScreenId::EscConfig, 0, "ESC COMMANDS", &UI_ESC_CMD, nullptr, -1, -1 },
		{ &UI_ESC_CONFIG, UiScreenId::EscConfig, 1, "ESC SETTINGS", &UI_AM32, nullptr,
		  (int)Am32Ctl::Leave, (int)Am32Ctl::Restart },
		{ &UI_ESC_CONFIG, UiScreenId::EscConfig, 2, "USB BRIDGE", &UI_BRIDGE, nullptr, -1, (int)BridgeCtl::Restart },
		{ &UI_SERVO_HUB_SCREEN, UiScreenId::ServoHub, 0, "SERVO TESTER", &UI_SERVO, nullptr, -1,
		  (int)ServoCtl::Restart },
		{ &UI_SERVO_HUB_SCREEN, UiScreenId::ServoHub, 1, "ANGLE METER", &UI_ANGLE, nullptr, -1, -1 },
		{ &UI_SERVO_HUB_SCREEN, UiScreenId::ServoHub, 2, "KST / CHASERVO", &UI_KST, uiKstEnter,
		  (int)KstCtl::Leave, (int)KstCtl::Restart },
		{ &UI_SERVO_HUB_SCREEN, UiScreenId::ServoHub, 3, "HITEC D SERIES", &UI_HITEC, uiHitecEnterD,
		  (int)HitecCtl::Leave, (int)HitecCtl::Restart },
		{ &UI_SERVO_HUB_SCREEN, UiScreenId::ServoHub, 4, "HITEC CLASSIC", &UI_HITEC, uiHitecEnterClassic,
		  (int)HitecCtl::Leave, (int)HitecCtl::Restart },
		{ &UI_RX_SIGNALS, UiScreenId::RxSignals, 0, "MONITOR", &UI_SBUS, nullptr, -1, (int)SbusCtl::Restart },
		{ &UI_RX_SIGNALS, UiScreenId::RxSignals, 1, "OUTPUT", &UI_SBUS_OUT, nullptr,
		  (int)SbusOutCtl::Leave, (int)SbusOutCtl::Restart },
	};
	checkTrue("the tables hold ten entries: 3, 5 and 2",
	          UI_ESC_CFG_ENTRIES_COUNT == 3 && UI_SERVO_ENTRIES_COUNT == 5 && UI_RX_ENTRIES_COUNT == 2);
	checkTrue("no screen is PROGRAMMER or TOOLS: 16 screens, SERVO, the hub, among them",
	          (int)UiScreenId::Count == 16);
	for (const auto &w : WANT) {
		char what[128];
		freshTester();
		// HITEC CLASSIC acts with the switched supply only.
		setSupply(1);
		tapId(UI_FIX(TesterCtl::Menu));
		hubFrames(1);
		const UiHub *hub = uiHubOf(w.hub);
		const UiHubEntry &e = hub->entries[w.entry];
		snprintf(what, sizeof(what), "%s: entry %d of %s, its screen and entry point as the table has them",
		         w.label, w.entry, w.hub->title);
		checkTrue(what, w.entry < hub->count && strcmp(e.label, w.label) == 0 && e.screen == w.tool &&
		                e.enter == w.enter);
		snprintf(what, sizeof(what), "%s: no MENU row opens it", w.label);
		checkTrue(what, uiMenuRowOf(w.tool) == -1);
		tapId(menuRow(w.hub));
		hubFrames(1);
		tapId(hubEntry(w.entry));
		hubFrames(2);
		snprintf(what, sizeof(what), "%s: two taps from MENU open it over %s", w.label, w.hub->title);
		checkTrue(what, navIsTool(w.hubId, w.tool->id));
		// Out again: BACK, and on a leave page LEAVE, then RESTART DSHOT.
		for (int k = 0; k < 600 && uiCurrentScreen() == w.tool->id; k++) {
			if (w.restart >= 0 && ctlEnabled(UI_FIX(w.restart))) tapId(UI_FIX(w.restart));
			else if (w.leave >= 0 && ctlEnabled(UI_FIX(w.leave))) tapId(UI_FIX(w.leave));
			else if (ctlEnabled(UI_FIX(UI_BACK))) tapId(UI_FIX(UI_BACK));
			else hubFrames(1);
		}
		hubFrames(1);
		snprintf(what, sizeof(what), "%s: it returns to %s", w.label, w.hub->title);
		checkTrue(what, navIsHub(w.hubId));
		for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) hubFrames(1);
		snprintf(what, sizeof(what), "%s: the claim Free, the entry acts again", w.label);
		checkTrue(what, pinClaimState() == ClaimState::Free && ctlEnabled(hubEntry(w.entry)));
		checkTrue("BACK: MENU, then the tester", backToTester());
	}
	setSupply(0);
	freshTester();
}

void runHubTests() {
	testHubTables();
	testHubDisabledEntry();
	testHubLines();
	testHubFit();
	testHubClaimGate();
	testServoScroll();
	testHubOpens();
	testNavDepth();
}
