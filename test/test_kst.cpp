/**
 * @file test_kst.cpp
 * @brief The KST programming suite: the search (test_kst_seek.cpp), the
 *        core1 job (test_kst_job.cpp), the files on the card
 *        (test_kst_files.cpp), what the screen shows (test_kst_view.cpp),
 *        and the screen through MENU (test_kst_ui.cpp).
 *
 * The tests are those of docs/design/kst-programming.md, "Tests".
 */

#include "check.h"
#include "pin_claim.h"
#include "ui_kst.h"

void runKstSeekTests();
void runKstJobTests();
void runKstFilesTests();
void runKstViewTests();
void runKstUiTests();

void runKstTests() {
	runKstSeekTests();
	runKstJobTests();
	runKstFilesTests();

	runKstViewTests();
	runKstUiTests();
}
