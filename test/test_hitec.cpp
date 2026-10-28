/**
 * @file test_hitec.cpp
 * @brief The Hitec programming suite: the core1 job (test_hitec_job.cpp) and
 *        the files on the card (test_hitec_files.cpp).
 *
 * Hitec is a name or trademark of its owner; it is used here only to say
 * which servos this module works with.
 *
 * The tests are those of docs/design/hitec-programming.md, "Testing".
 */

void runHitecJobTests();
void runHitecFilesTests();

void runHitecTests() {
	runHitecJobTests();
	runHitecFilesTests();
}
