#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "deck.h"
#include "scheduler.h"

static int failures;

static void check(bool condition, const char *message)
{
	if (condition)
		return;

	failures++;
	printf("FAIL: %s\n", message);
}

static void test_parse_card_line(void)
{
	struct card card;
	enum deck_parse_result result = deck_parse_card_line(
		&card,
		"card-1\tnote-1\tfront\\nline\tback\\\\slash\ttag1 tag2\n"
	);

	check(result == DECK_PARSE_OK, "card line parses");
	check(strcmp(card.card_id, "card-1") == 0, "card id parses");
	check(strcmp(card.note_id, "note-1") == 0, "note id parses");
	check(strcmp(card.front, "front\nline") == 0, "front escaped newline parses");
	check(strcmp(card.back, "back\\slash") == 0, "back escaped slash parses");
	check(strcmp(card.tags, "tag1 tag2") == 0, "tags parse");
}

static void test_reject_bad_card_line(void)
{
	struct card card;

	check(deck_parse_card_line(&card, "") == DECK_PARSE_EMPTY, "empty line rejected");
	check(
		deck_parse_card_line(&card, "card\tnote\tfront\tback") ==
			DECK_PARSE_BAD_FIELD_COUNT,
		"missing field rejected"
	);
	check(
		deck_parse_card_line(&card, "card\tnote\t\tback\ttag") ==
			DECK_PARSE_MISSING_REQUIRED_FIELD,
		"missing front rejected"
	);
	check(
		deck_parse_card_line(&card, "card\tnote\tfront\\x\tback\ttag") ==
			DECK_PARSE_BAD_ESCAPE,
		"bad escape rejected"
	);
}

static void test_scheduler_repeats_again(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3);
	check(scheduler_current_index(&session) == 0, "scheduler starts at first card");

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.done_count == 0, "again keeps card due");
	check(scheduler_current_index(&session) == 1, "again advances to next card");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.done_count == 1, "good marks card done");
	check(scheduler_current_index(&session) == 2, "good advances to next card");

	scheduler_rate_current(&session, SCHEDULER_RATING_EASY);
	check(session.done_count == 2, "easy marks card done");
	check(scheduler_current_index(&session) == 0, "again card is revisited");

	scheduler_rate_current(&session, SCHEDULER_RATING_HARD);
	check(session.done_count == 3, "hard marks final card done");
	check(scheduler_is_complete(&session), "scheduler completes");
	check(session.rating_counts[SCHEDULER_RATING_AGAIN] == 1, "again count tracked");
	check(session.rating_counts[SCHEDULER_RATING_HARD] == 1, "hard count tracked");
	check(session.rating_counts[SCHEDULER_RATING_GOOD] == 1, "good count tracked");
	check(session.rating_counts[SCHEDULER_RATING_EASY] == 1, "easy count tracked");
}

static void test_scheduler_rejects_invalid_rating(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1);
	scheduler_rate_current(&session, (enum scheduler_rating)99);
	check(session.done_count == 0, "invalid rating does not mark done");
	check(session.rating_counts[SCHEDULER_RATING_AGAIN] == 0, "invalid rating does not count");
}

int main(void)
{
	test_parse_card_line();
	test_reject_bad_card_line();
	test_scheduler_repeats_again();
	test_scheduler_rejects_invalid_rating();

	if (failures != 0)
	{
		printf("%d deck/scheduler test failures\n", failures);
		return 1;
	}

	puts("deck/scheduler tests passed");
	return 0;
}
