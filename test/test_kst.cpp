/**
 * @file test_kst.cpp
 * @brief The KST programming suite: the search (test_kst_seek.cpp), the
 *        core1 job (test_kst_job.cpp), the files on the card
 *        (test_kst_files.cpp), and the screen, a stub.
 *
 * Slice 3 of docs/design/pin-claim.md replaces ui_kst.cpp and the stub's
 * checks here, with the tests in docs/design/kst-programming.md.
 */

#include "check.h"
#include "pin_claim.h"
#include "ui_kst.h"

void runKstSeekTests();
void runKstJobTests();
void runKstFilesTests();

void runKstTests() {
	runKstSeekTests();
	runKstJobTests();
	runKstFilesTests();

	section("KST programming: the stub screen");

	checkTrue("the screen is not built", !uiKstAvailable());
	uiKstEnter();
	checkTrue("opening it claims nothing", pinClaimHolder() == PinUser::None);
	checkTrue("it leaves on its first frame", !uiKstTick(nullptr));
}
