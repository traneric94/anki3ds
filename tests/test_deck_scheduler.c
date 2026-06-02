#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "deck.h"
#include "review_state.h"
#include "scheduler.h"

#define TEST_STATE_PATH "/private/tmp/anki3ds-review-state-test.tsv"

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

static void build_test_deck(struct deck *deck)
{
	deck_init(deck, "state-test");
	deck->card_count = 2;
	snprintf(deck->cards[0].card_id, sizeof(deck->cards[0].card_id), "card-1");
	snprintf(deck->cards[0].front, sizeof(deck->cards[0].front), "front 1");
	snprintf(deck->cards[0].back, sizeof(deck->cards[0].back), "back 1");
	snprintf(deck->cards[1].card_id, sizeof(deck->cards[1].card_id), "card-2");
	snprintf(deck->cards[1].front, sizeof(deck->cards[1].front), "front 2");
	snprintf(deck->cards[1].back, sizeof(deck->cards[1].back), "back 2");
}

static void test_review_state_missing_file(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_NOT_FOUND,
		"missing state is reported"
	);
	check(session.done_count == 0, "missing state leaves scheduler unchanged");
}

static void test_review_state_round_trip(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"state saves"
	);

	scheduler_init(&loaded, deck.card_count);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"state loads"
	);
	check(!loaded.cards[0].done, "again card stays due after load");
	check(loaded.cards[0].review_count == 1, "again review count loads");
	check(loaded.cards[0].last_rating == SCHEDULER_RATING_AGAIN, "again rating loads");
	check(loaded.cards[1].done, "good card done loads");
	check(loaded.cards[1].review_count == 1, "good review count loads");
	check(loaded.cards[1].last_rating == SCHEDULER_RATING_GOOD, "good rating loads");
	check(loaded.done_count == 1, "done count recalculates on load");
	check(scheduler_current_index(&loaded) == 0, "first due card selected after load");

	remove(TEST_STATE_PATH);
}

int main(void)
{
	test_parse_card_line();
	test_reject_bad_card_line();
	test_scheduler_repeats_again();
	test_scheduler_rejects_invalid_rating();
	test_review_state_missing_file();
	test_review_state_round_trip();

	if (failures != 0)
	{
		printf("%d deck/scheduler test failures\n", failures);
		return 1;
	}

	puts("deck/scheduler tests passed");
	return 0;
}
