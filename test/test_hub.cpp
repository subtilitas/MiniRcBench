/**
 * @file test_hub.cpp
 * @brief The hub, `ui_hub`, the one widget under PROGRAMMER, ESC CONFIG, RX
 *        SIGNALS and TOOLS: slice 1 of menu structure C.
 *
 * Through the real UI: each of the four screens reports BACK and then one
 * control per entry of its table, at the entry's rectangle and enabled as
 * the hub draws it, is drawn again when it opens a second time, and only
 * PROGRAMMER's frame follows SERVO SUPPLY; HITEC CLASSIC, the one entry with a gate, reports
 * disabled without the switched supply and takes no tap, and with it reports
 * enabled and takes a press.
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
#include "ui_menu.h"
#include "ui_registry.h"
#include "ui_rx_signals.h"
#include "ui_screen.h"
#include "ui_servo_prog.h"
#include "ui_tools.h"
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
	// A claim outlives uiInit(); let the last test's claim end first.
	for (int k = 0; k < 200 && pinClaimState() != ClaimState::Free; k++) hubFrames(1);
	fakeFlashClear(); settingsLoad(); uiInit();
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
// the four screens, through the real UI
// ---------------------------------------------------------------------------

/** @brief One of the four screens and its hub. */
struct HubCase {
	const char     *name;
	const UiScreen *screen;
	const UiHub    *hub;
	UiScreenId      id;
	int             count;
};

/**
 * @brief The control table of each hub: BACK, then each entry at its
 *        rectangle, 228 x 46 px at (6, 62 + 80 x i), enabled as
 *        uiHubEntryActs() says and drawn so.
 */
static void testHubTables() {
	section("hub: the control tables of the four screens");
	static const HubCase CASES[] = {
		{ "ESC CONFIG", &UI_ESC_CONFIG, &UI_ESC_CFG_HUB, UiScreenId::EscConfig, 2 },
		{ "PROGRAMMER", &UI_SERVO_PROG, &UI_PROG_HUB, UiScreenId::ServoProg, 3 },
		{ "RX SIGNALS", &UI_RX_SIGNALS, &UI_RX_HUB, UiScreenId::RxSignals, 2 },
		{ "TOOLS", &UI_TOOLS, &UI_TOOLS_HUB, UiScreenId::Tools, 1 },
	};
	for (const HubCase &c : CASES) {
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
		snprintf(what, sizeof(what), "%s: entries in its table", c.name);
		checkInt(what, c.hub->count, c.count);
		UiControl t[UI_CONTROLS_MAX];
		const uint8_t n = uiControls(t, UI_CONTROLS_MAX);
		snprintf(what, sizeof(what), "%s: BACK and one control per entry", c.name);
		checkInt(what, n, c.count + 1);
		const UiRect back = uiBackRect();
		snprintf(what, sizeof(what), "%s: BACK first, at its rectangle, acting", c.name);
		checkTrue(what, n > 0 && t[0].id == UI_FIX(UI_BACK) && t[0].enabled && t[0].rect.x == back.x &&
		                t[0].rect.y == back.y && t[0].rect.w == back.w && t[0].rect.h == back.h);
		for (int i = 0; i < c.hub->count && i + 1 < n; i++) {
			const UiControl &e = t[i + 1];
			const UiRect r = uiHubEntryRect(i);
			const char *label = c.hub->entries[i].label;
			snprintf(what, sizeof(what), "%s: %s is control %d, HUB_ENTRY(%d)", c.name, label, i + 1, i);
			checkTrue(what, e.id == HUB_ENTRY(i) && e.id == UI_FIX(1 + i));
			snprintf(what, sizeof(what), "%s: %s at (6, %d), 228 x 46 px", c.name, label, 62 + 80 * i);
			checkTrue(what, e.rect.x == 6 && e.rect.y == 62 + 80 * i && e.rect.w == 228 && e.rect.h == 46 &&
			                r.x == e.rect.x && r.y == e.rect.y && r.w == e.rect.w && r.h == e.rect.h);
			const bool acts = uiHubEntryActs(*c.hub, i);
			snprintf(what, sizeof(what), "%s: %s reports %s and is drawn so", c.name, label,
			         acts ? "enabled" : "disabled");
			checkTrue(what, e.enabled == acts && buttonIs(e.rect, label, acts));
			// The one entry with a gate is HITEC CLASSIC, disabled at OWN.
			snprintf(what, sizeof(what), "%s: %s %s", c.name, label,
			         c.hub->entries[i].enabled ? "has a gate, which fails" : "has no gate and acts");
			checkTrue(what, acts == (c.hub->entries[i].enabled == nullptr));
		}
		checkControlTable(c.name);

		// Only PROGRAMMER's entries carry a power line: the other three
		// screens draw the same frame whatever SERVO SUPPLY is.
		bool power = false;
		for (int i = 0; i < c.hub->count; i++)
			power = power || c.hub->entries[i].powerOwn || c.hub->entries[i].powerSwitchedFmt;
		snprintf(what, sizeof(what), "%s: %s", c.name,
		         c.id == UiScreenId::ServoProg ? "its entries carry a power line" : "no entry carries a power line");
		checkTrue(what, power == (c.id == UiScreenId::ServoProg));
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
		// The four screens share the widget's repaint key: the same hub
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
	checkTrue("PROGRAMMER is open, HITEC CLASSIC its last entry",
	          uiCurrentScreen() == UiScreenId::ServoProg && cl == UI_PROGRAMMERS_COUNT - 1);
	const UiRect r = uiHubEntryRect(cl);
	UiControl c;
	checkTrue("OWN: its gate fails", !uiHubEntryActs(UI_PROG_HUB, cl) && !servoSupplyFitted());
	checkTrue("OWN: the table lists it at its rectangle, disabled",
	          ctlFind(PROG_ENTRY(cl), &c) && !c.enabled && c.rect.x == r.x && c.rect.y == r.y &&
	          c.rect.w == r.w && c.rect.h == r.h);
	checkTrue("OWN: it is drawn disabled", buttonIs(r, "HITEC CLASSIC", false));
	fakePress(r.x + r.w / 2, r.y + r.h / 2);
	hubFrames(1);
	checkTrue("OWN: a press draws no pressed look", buttonIs(r, "HITEC CLASSIC", false));
	fakeRelease();
	hubFrames(2);
	checkTrue("OWN: the tap opens nothing and claims nothing",
	          uiCurrentScreen() == UiScreenId::ServoProg && pinClaimState() == ClaimState::Free &&
	          !escTaskSuspended());
	checkTrue("the entries above it act", ctlEnabled(PROG_ENTRY(0)) && ctlEnabled(PROG_ENTRY(1)));

	setSupply(1);
	hubFrames(1);
	checkTrue("GP28: its gate holds", uiHubEntryActs(UI_PROG_HUB, cl) && servoSupplyFitted());
	checkTrue("GP28: the table lists it enabled, on the next frame",
	          ctlFind(PROG_ENTRY(cl), &c) && c.enabled && c.rect.y == r.y);
	checkTrue("GP28: it is drawn live", buttonIs(r, "HITEC CLASSIC", true));
	{
		// Its tap opening HITEC SERVO is test_hitec_ui.cpp's; here the press
		// is taken and, slid off before the lift, opens nothing.
		const uint32_t rest = fakeRegionHash(r.x, r.y, r.w, r.h);
		fakePress(r.x + r.w / 2, r.y + r.h / 2);
		hubFrames(1);
		checkTrue("GP28: a press draws it pressed",
		          fakeRegionHash(r.x, r.y, r.w, r.h) != rest && uiCurrentScreen() == UiScreenId::ServoProg);
		fakeHold(r.x + r.w / 2, 30);
		hubFrames(1);
		fakeRelease();
		hubFrames(1);
		checkTrue("GP28: slid off before the lift, it opens nothing",
		          uiCurrentScreen() == UiScreenId::ServoProg && pinClaimState() == ClaimState::Free &&
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
	uiHubEnter();
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
	uiHubEnter();
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
	const UiHubEntry t[4] = { plain("A", "X", "Y"), plain("B", "X", "Y"), plain("C", "X", "Y"),
		                      plain("D", "X", "Y") };
	return hubEntriesFit(UiHub{ "T", nullptr, t, (uint8_t)n });
}

static bool yes() { return true; }

/** @brief Each rule of hubEntriesFit() at its limit and one step past it. */
static void testHubFit() {
	section("hub: hubEntriesFit(), each rule at its limit");
	checkTrue("the four tables fit", hubEntriesFit(UI_PROG_HUB) && hubEntriesFit(UI_ESC_CFG_HUB) &&
	          hubEntriesFit(UI_RX_HUB) && hubEntriesFit(UI_TOOLS_HUB));
	checkTrue("the limits: 18 and 37 characters", sizeof(L18) - 1 == UI_HUB_LABEL_CHARS &&
	          sizeof(L37) - 1 == UI_HUB_LINE_CHARS && gfxTextW(L18, 2) == 214 && gfxTextW(L37, 1) == 221);

	checkTrue("1, 2 and 3 entries fit; 0 and 4 do not",
	          fitsN(1) && fitsN(2) && fitsN(3) && !fitsN(0) && !fitsN(4));
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
	// under its top, the next entry starts at 80, the panel ends 98 px under
	// the third entry's top.
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
	checkTrue("four lines under the last entry fit the panel only for the first two entries",
	          fits1(twoWhy2) && !fits2(twoWhy2));
	{
		// 222 + 50 + 36 + 7 = 315 <= 320: the page's height, not a line
		// count, is the limit under the last entry.
		const UiHubEntry t[3] = { two, two, twoWhy2 };
		checkTrue("four lines under the third entry end at y 315 and fit",
		          hubEntriesFit(UiHub{ "T", nullptr, t, 3 }));
	}
}

void runHubTests() {
	testHubTables();
	testHubDisabledEntry();
	testHubLines();
	testHubFit();
}
