#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_settings.h"
#include "deck.h"
#include "deck_index.h"
#include "deck_summary.h"
#include "media_image.h"
#include "review_state.h"
#include "scheduler.h"

#define TEST_STATE_PATH "/private/tmp/anki3ds-review-state-test.tsv"
#define TEST_STATE_TEMP_PATH TEST_STATE_PATH ".tmp"
#define TEST_STATE_BACKUP_PATH TEST_STATE_PATH ".bak"
#define TEST_SETTINGS_PATH "/private/tmp/anki3ds-settings-test.tsv"
#define TEST_SETTINGS_TEMP_PATH TEST_SETTINGS_PATH ".tmp"
#define TEST_SETTINGS_BACKUP_PATH TEST_SETTINGS_PATH ".bak"
#define TEST_CARDS_PATH "/private/tmp/anki3ds-cards-test.tsv"
#define TEST_MEDIA_PATH "/private/tmp/anki3ds-media-test.a3i"
#define TEST_DECK_ROOT "/private/tmp/anki3ds-deck-index-test"
#define TEST_TODAY 20000

static int failures;

static void write_file(const char *path, const char *content);
static void write_binary_file(const char *path, const unsigned char *content, size_t size);

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
	check(strcmp(card.front_media, "") == 0, "missing front media defaults empty");
	check(strcmp(card.back_media, "") == 0, "missing back media defaults empty");
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
	check(
		deck_parse_card_line(&card, "card\tnote\tfront\tback\ttag\tbad/name\t") ==
			DECK_PARSE_BAD_MEDIA_NAME,
		"media path separators rejected"
	);
}

static void test_parse_card_line_with_media(void)
{
	struct card card;
	enum deck_parse_result result = deck_parse_card_line(
		&card,
		"card-1\tnote-1\tfront\tback\ttag1\tfront.a3i\tback.a3i\n"
	);

	check(result == DECK_PARSE_OK, "media card line parses");
	check(strcmp(card.front_media, "front.a3i") == 0, "front media parses");
	check(strcmp(card.back_media, "back.a3i") == 0, "back media parses");
}

static void test_deck_load_rejects_duplicate_card_ids(void)
{
	struct deck deck;

	write_file(
		TEST_CARDS_PATH,
		"card-1\tnote-1\tfront 1\tback 1\ttag\n"
		"card-1\tnote-2\tfront 2\tback 2\ttag\n"
	);
	deck_init(&deck, "duplicate-test");

	check(
		deck_load_cards(&deck, TEST_CARDS_PATH) == DECK_LOAD_BAD_FORMAT,
		"duplicate card ids are rejected"
	);

	remove(TEST_CARDS_PATH);
}

static void test_tracked_sample_decks_load(void)
{
	struct deck deck;
	struct app_settings settings;

	deck_init(&deck, "sample");
	check(
		deck_load_cards(&deck, "sample-decks/sample/cards.tsv") == DECK_LOAD_OK,
		"tracked sample deck loads"
	);
	check(deck.card_count == 10, "tracked sample deck card count");
	check(
		app_settings_load(&settings, "sample-decks/sample/settings.tsv") ==
			APP_SETTINGS_LOAD_OK,
		"tracked sample settings load"
	);
	check(settings.new_limit == 20, "tracked sample new limit");
	check(settings.review_limit == 200, "tracked sample review limit");

	deck_init(&deck, "limits-demo");
	check(
		deck_load_cards(&deck, "sample-decks/limits-demo/cards.tsv") == DECK_LOAD_OK,
		"tracked limits demo deck loads"
	);
	check(deck.card_count == 6, "tracked limits demo deck card count");
	check(
		app_settings_load(&settings, "sample-decks/limits-demo/settings.tsv") ==
			APP_SETTINGS_LOAD_OK,
		"tracked limits demo settings load"
	);
	check(settings.new_limit == 2, "tracked limits demo new limit");
	check(settings.review_limit == 5, "tracked limits demo review limit");

	deck_init(&deck, "media-demo");
	check(
		deck_load_cards(&deck, "sample-decks/media-demo/cards.tsv") == DECK_LOAD_OK,
		"tracked media demo deck loads"
	);
	check(deck.card_count == 2, "tracked media demo deck card count");
	check(
		strcmp(deck.cards[0].front_media, "colors.a3i") == 0,
		"tracked media demo front media loads"
	);
	check(
		strcmp(deck.cards[0].back_media, "colors.a3i") == 0,
		"tracked media demo back media loads"
	);
	check(
		app_settings_load(&settings, "sample-decks/media-demo/settings.tsv") ==
			APP_SETTINGS_LOAD_OK,
		"tracked media demo settings load"
	);
}

static void test_scheduler_schedules_due_days(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
	check(scheduler_current_index(&session) == 0, "scheduler starts at first card");
	check(session.due_count == 3, "new cards start due");

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.due_count == 3, "again keeps card due today");
	check(session.cards[0].due_day == TEST_TODAY, "again stays due today");
	check(session.cards[0].interval_days == 0, "again keeps zero-day interval");
	check(session.cards[0].ease_permille == 2300, "again lowers ease");
	check(scheduler_current_index(&session) == 1, "again advances to next card");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 2, "good schedules one card out");
	check(session.cards[1].due_day == TEST_TODAY + 1, "good schedules tomorrow");
	check(session.cards[1].interval_days == 1, "good starts one-day interval");
	check(scheduler_current_index(&session) == 2, "good advances to next card");

	scheduler_rate_current(&session, SCHEDULER_RATING_EASY);
	check(session.due_count == 1, "easy schedules one card out");
	check(session.cards[2].due_day == TEST_TODAY + 4, "easy starts four-day interval");
	check(scheduler_current_index(&session) == 0, "again card is revisited");

	scheduler_rate_current(&session, SCHEDULER_RATING_HARD);
	check(session.due_count == 0, "hard schedules final due card out");
	check(session.cards[0].due_day == TEST_TODAY + 1, "hard schedules tomorrow");
	check(scheduler_is_complete(&session), "scheduler completes when no due cards remain");
	check(session.rating_counts[SCHEDULER_RATING_AGAIN] == 1, "again count tracked");
	check(session.rating_counts[SCHEDULER_RATING_HARD] == 1, "hard count tracked");
	check(session.rating_counts[SCHEDULER_RATING_GOOD] == 1, "good count tracked");
	check(session.rating_counts[SCHEDULER_RATING_EASY] == 1, "easy count tracked");
	check(session.reviewed_count == 4, "reviewed count tracks ratings");
}

static void test_scheduler_rejects_invalid_rating(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);
	scheduler_rate_current(&session, (enum scheduler_rating)99);
	check(session.due_count == 1, "invalid rating leaves due count unchanged");
	check(session.rating_counts[SCHEDULER_RATING_AGAIN] == 0, "invalid rating does not count");
}

static void test_scheduler_new_again_stays_in_initial_learning(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.cards[0].review_count == 2, "new again reviews count");
	check(session.cards[0].lapses == 0, "new again does not count as lapse");
	check(session.cards[0].interval_days == 0, "new again stays in zero-day loop");
	check(scheduler_card_is_due(&session, 0), "new again remains due");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.cards[0].interval_days == 1, "new good after again starts one-day interval");
	check(session.cards[0].due_day == TEST_TODAY + 1, "new good after again schedules tomorrow");
	check(session.cards[0].lapses == 0, "new good after again still has no lapses");
	check(session.due_count == 0, "new good after again clears due queue");
}

static void test_scheduler_scales_review_intervals(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			0,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			0,
			false,
			0,
			0
		),
		"review card restores"
	);

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.cards[0].interval_days == 25, "good scales interval by ease");
	check(session.cards[0].due_day == TEST_TODAY + 25, "good schedules scaled interval");
	check(session.due_count == 0, "scaled review leaves session complete");

	scheduler_init(&session, 1, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			0,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			0,
			false,
			0,
			0
		),
		"review card restores again"
	);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.cards[0].interval_days == 0, "again resets review interval");
	check(session.cards[0].due_day == TEST_TODAY, "again keeps review due");
	check(session.cards[0].lapses == 1, "again increments review lapses");
	check(session.cards[0].ease_permille == 2300, "again lowers review ease");
	check(session.due_count == 1, "again remains due");
}

static void test_scheduler_undo_last_rating(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(session.cards[0].due_day == TEST_TODAY + 1, "rated card schedules before undo");
	check(session.due_count == 1, "due count drops before undo");
	check(session.reviewed_count == 1, "review count increments before undo");
	check(scheduler_current_index(&session) == 1, "scheduler advances before undo");

	check(scheduler_undo_last(&session), "undo succeeds");
	check(session.cards[0].review_count == 0, "undo restores card review count");
	check(session.cards[0].due_day == TEST_TODAY, "undo restores due day");
	check(session.cards[0].interval_days == 0, "undo restores interval");
	check(session.due_count == 2, "undo restores due count");
	check(session.reviewed_count == 0, "undo restores reviewed count");
	check(session.rating_counts[SCHEDULER_RATING_GOOD] == 0, "undo restores rating count");
	check(scheduler_current_index(&session) == 0, "undo restores current index");
	check(!scheduler_undo_last(&session), "undo is one-shot");
}

static void test_scheduler_suspend_current(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);

	check(scheduler_suspend_current(&session), "suspend succeeds");
	check(session.cards[0].suspended, "suspend marks current card");
	check(scheduler_suspended_count(&session) == 1, "suspend count includes suspended card");
	check(!scheduler_card_is_due(&session, 0), "suspended card is not due");
	check(session.due_count == 1, "suspend drops due count");
	check(scheduler_current_index(&session) == 1, "suspend advances to next due card");
	check(session.reviewed_count == 0, "suspend does not count as review");

	check(scheduler_undo_last(&session), "undo suspend succeeds");
	check(!session.cards[0].suspended, "undo suspend restores card");
	check(scheduler_suspended_count(&session) == 0, "undo suspend clears suspended count");
	check(session.due_count == 2, "undo suspend restores due count");
	check(scheduler_current_index(&session) == 0, "undo suspend restores current index");
}

static void test_scheduler_suspend_last_due_card(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);

	check(scheduler_suspend_current(&session), "last suspend succeeds");
	check(session.due_count == 0, "last suspend clears due queue");
	check(scheduler_is_complete(&session), "last suspend completes session");
	check(!scheduler_has_current(&session), "last suspend leaves no current card");

	check(scheduler_undo_last(&session), "undo last suspend succeeds");
	check(!session.cards[0].suspended, "undo last suspend restores flag");
	check(session.due_count == 1, "undo last suspend restores due queue");
	check(scheduler_has_current(&session), "undo last suspend restores current card");
	check(scheduler_current_index(&session) == 0, "undo last suspend restores current index");
}

static void test_scheduler_unsuspend_all(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	check(scheduler_suspend_current(&session), "suspend before unsuspend succeeds");
	check(session.due_count == 1, "suspend before unsuspend drops due count");
	check(scheduler_current_index(&session) == 1, "suspend before unsuspend advances");

	check(scheduler_unsuspend_all(&session) == 1, "unsuspend all returns count");
	check(!session.cards[0].suspended, "unsuspend all clears flag");
	check(session.due_count == 2, "unsuspend all restores due count");
	check(scheduler_current_index(&session) == 0, "unsuspend all repositions current");
	check(!scheduler_undo_last(&session), "unsuspend all clears one-step undo");
	check(scheduler_unsuspend_all(&session) == 0, "unsuspend all no-ops without suspended cards");
}

static void test_scheduler_limits_new_cards(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
	scheduler_set_daily_limits(&session, 1, 0);

	check(session.due_count == 1, "new limit exposes one new card");
	check(scheduler_current_index(&session) == 0, "new limit starts first card");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.new_count_today == 1, "new limit counts introduced card");
	check(session.review_count_today == 0, "new limit does not count review");
	check(session.due_count == 0, "new limit hides remaining new cards");
	check(scheduler_is_complete(&session), "new limit completes visible queue");
}

static void test_scheduler_allows_started_new_card_after_limit(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_set_daily_limits(&session, 1, 0);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.new_count_today == 1, "again counts first new card");
	check(session.due_count == 1, "started new card stays due after limit");
	check(scheduler_current_index(&session) == 0, "started new card stays current");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 0, "finished started new card clears visible queue");
}

static void test_scheduler_limits_review_cards(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			0,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			0,
			false,
			100,
			TEST_TODAY - 10
		),
		"first review card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			1,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			0,
			false,
			100,
			TEST_TODAY - 10
		),
		"second review card restores"
	);
	scheduler_set_daily_limits(&session, 0, 1);

	check(session.due_count == 1, "review limit exposes one review card");
	check(scheduler_current_index(&session) == 0, "review limit starts first review card");

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.review_count_today == 1, "review limit counts reviewed card");
	check(session.due_count == 1, "started review card stays due after limit");
	check(scheduler_current_index(&session) == 0, "started review card stays current");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 0, "review limit hides remaining review cards");
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
	scheduler_init(&session, deck.card_count, TEST_TODAY);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_NOT_FOUND,
		"missing state is reported"
	);
	check(session.due_count == deck.card_count, "missing state leaves scheduler unchanged");
}

static void test_review_state_round_trip(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"state saves"
	);

	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"state loads"
	);
	check(scheduler_card_is_due(&loaded, 0), "again card stays due after load");
	check(loaded.cards[0].due_day == TEST_TODAY, "again due day loads");
	check(loaded.cards[0].interval_days == 0, "again interval loads");
	check(loaded.cards[0].review_count == 1, "again review count loads");
	check(loaded.cards[0].last_rating == SCHEDULER_RATING_AGAIN, "again rating loads");
	check(loaded.cards[0].first_review_day == TEST_TODAY, "again first review day loads");
	check(loaded.cards[0].last_review_day == TEST_TODAY, "again last review day loads");
	check(!scheduler_card_is_due(&loaded, 1), "good card is scheduled after load");
	check(loaded.cards[1].due_day == TEST_TODAY + 1, "good due day loads");
	check(loaded.cards[1].interval_days == 1, "good interval loads");
	check(loaded.cards[1].review_count == 1, "good review count loads");
	check(loaded.cards[1].last_rating == SCHEDULER_RATING_GOOD, "good rating loads");
	check(!loaded.cards[1].suspended, "saved active card loads unsuspended");
	check(loaded.cards[1].first_review_day == TEST_TODAY, "good first review day loads");
	check(loaded.cards[1].last_review_day == TEST_TODAY, "good last review day loads");
	check(loaded.new_count_today == 2, "loaded state recounts new cards today");
	check(loaded.due_count == 1, "due count recalculates on load");
	check(scheduler_current_index(&loaded) == 0, "first due card selected after load");
	check(access(TEST_STATE_BACKUP_PATH, F_OK) != 0, "state save removes backup");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_round_trip_suspended_card(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);

	check(scheduler_suspend_current(&session), "suspended card saves from scheduler");
	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"suspended state saves"
	);

	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"suspended state loads"
	);
	check(loaded.cards[0].suspended, "suspended flag loads");
	check(!scheduler_card_is_due(&loaded, 0), "loaded suspended card is not due");
	check(loaded.due_count == 1, "loaded suspended state recounts due cards");
	check(scheduler_current_index(&loaded) == 1, "loaded suspended state picks next card");

	remove(TEST_STATE_PATH);
}

static void test_review_state_loads_backup_when_primary_missing(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_BACKUP_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"backup state loads when primary is missing"
	);
	check(session.cards[0].review_count == 1, "backup state card loads");
	check(!scheduler_card_is_due(&session, 0), "backup state due day loads");

	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_loads_previous_current_format(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "card-1\t1\t2\t20001\t1\t2500\t0\n");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"previous current state format loads"
	);
	check(!session.cards[0].suspended, "previous state format defaults unsuspended");
	check(session.cards[0].first_review_day == 0, "previous state format defaults first day");
	check(session.cards[0].last_review_day == 0, "previous state format defaults last day");
	check(!scheduler_card_is_due(&session, 0), "previous state due day loads");

	remove(TEST_STATE_PATH);
}

static void test_review_state_loads_suspended_format(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "card-1\t1\t2\t20001\t1\t2500\t0\t1\n");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"suspended state format loads"
	);
	check(session.cards[0].suspended, "suspended state format loads flag");
	check(session.cards[0].first_review_day == 0, "suspended state format defaults first day");
	check(session.cards[0].last_review_day == 0, "suspended state format defaults last day");
	check(!scheduler_card_is_due(&session, 0), "suspended state format hides due card");

	remove(TEST_STATE_PATH);
}

static void test_review_state_loads_legacy_done_format(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "card-1\t0\t2\t0\ncard-2\t1\t3\t2\n");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"legacy state loads"
	);
	check(scheduler_card_is_due(&session, 0), "legacy not-done card is due");
	check(!scheduler_card_is_due(&session, 1), "legacy done card is scheduled out");
	check(session.cards[1].due_day == TEST_TODAY + 1, "legacy done card migrates to tomorrow");
	check(session.cards[1].interval_days == 1, "legacy done card migrates interval");
	check(session.due_count == 1, "legacy due count recalculates");

	remove(TEST_STATE_PATH);
}

static void test_review_state_delete_removes_save_artifacts(void)
{
	write_file(TEST_STATE_PATH, "state\n");
	write_file(TEST_STATE_TEMP_PATH, "temp\n");
	write_file(TEST_STATE_BACKUP_PATH, "backup\n");

	check(review_state_delete(TEST_STATE_PATH), "review state delete succeeds");
	check(access(TEST_STATE_PATH, F_OK) != 0, "review state delete removes primary");
	check(access(TEST_STATE_TEMP_PATH, F_OK) != 0, "review state delete removes temp");
	check(access(TEST_STATE_BACKUP_PATH, F_OK) != 0, "review state delete removes backup");
}

static void test_review_state_bad_load_does_not_mutate_session(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "card-1\t1\t2\t20001\t1\t2500\t0\nbad\n");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_BAD_FORMAT,
		"bad state load fails"
	);
	check(session.due_count == deck.card_count, "bad state load leaves due count unchanged");
	check(session.cards[0].due_day == TEST_TODAY, "bad state load leaves card unchanged");

	remove(TEST_STATE_PATH);
}

static void test_app_settings_missing_file_uses_defaults(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_PATH);

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_NOT_FOUND,
		"missing settings reports defaults"
	);
	check(settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT, "missing settings new default");
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"missing settings review default"
	);
}

static void test_app_settings_loads_limits(void)
{
	struct app_settings settings;

	write_file(TEST_SETTINGS_PATH, "# limits\nnew_limit\t1\nreview_limit\t2\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"settings load"
	);
	check(settings.new_limit == 1, "settings new limit loads");
	check(settings.review_limit == 2, "settings review limit loads");

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_loads_backup_when_primary_missing(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_PATH);
	write_file(TEST_SETTINGS_BACKUP_PATH, "new_limit\t9\nreview_limit\t10\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"backup settings load when primary is missing"
	);
	check(settings.new_limit == 9, "backup settings new limit loads");
	check(settings.review_limit == 10, "backup settings review limit loads");

	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_bad_file_uses_defaults(void)
{
	struct app_settings settings;

	write_file(TEST_SETTINGS_PATH, "new_limit\tbad\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_BAD_FORMAT,
		"bad settings reports ignored"
	);
	check(settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT, "bad settings new default");
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"bad settings review default"
	);

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_save_round_trip(void)
{
	struct app_settings settings;
	struct app_settings loaded;

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);

	settings.new_limit = 3;
	settings.review_limit = 7;

	check(
		app_settings_save(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_SAVE_OK,
		"settings save succeeds"
	);
	check(
		app_settings_load(&loaded, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"saved settings load"
	);
	check(loaded.new_limit == 3, "saved settings new limit loads");
	check(loaded.review_limit == 7, "saved settings review limit loads");
	check(access(TEST_SETTINGS_TEMP_PATH, F_OK) != 0, "settings save removes temp");
	check(access(TEST_SETTINGS_BACKUP_PATH, F_OK) != 0, "settings save removes backup");

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_save_replaces_existing_file(void)
{
	struct app_settings settings;
	struct app_settings loaded;

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_PATH, "new_limit\t1\nreview_limit\t2\n");

	settings.new_limit = 0;
	settings.review_limit = 50;

	check(
		app_settings_save(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_SAVE_OK,
		"settings save replaces existing file"
	);
	check(
		app_settings_load(&loaded, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"replaced settings load"
	);
	check(loaded.new_limit == 0, "replaced settings new limit loads");
	check(loaded.review_limit == 50, "replaced settings review limit loads");
	check(access(TEST_SETTINGS_BACKUP_PATH, F_OK) != 0, "settings save removes old backup");

	remove(TEST_SETTINGS_PATH);
}

static void write_file(const char *path, const char *content)
{
	FILE *file = fopen(path, "w");

	check(file != NULL, "test file opens");
	if (file == NULL)
		return;

	fputs(content, file);
	fclose(file);
}

static void write_binary_file(const char *path, const unsigned char *content, size_t size)
{
	FILE *file = fopen(path, "wb");

	check(file != NULL, "test binary file opens");
	if (file == NULL)
		return;

	fwrite(content, 1, size, file);
	fclose(file);
}

static void load_entry_deck(
	struct deck *deck,
	const struct deck_entry *entry,
	const char *message
)
{
	deck_init(deck, entry->display_name);
	check(deck_load_cards(deck, entry->cards_path) == DECK_LOAD_OK, message);
}

static void save_entry_state(
	const struct deck *deck,
	const struct scheduler_session *session,
	const struct deck_entry *entry,
	const char *message
)
{
	check(
		review_state_save(deck, session, entry->state_path) == REVIEW_STATE_SAVE_OK,
		message
	);
}

static void test_deck_index_builds_paths(void)
{
	struct deck_entry entry;

	check(
		deck_index_build_entry(&entry, "/root", "sample"),
		"deck index builds entry"
	);
	check(strcmp(entry.id, "sample") == 0, "deck index stores id");
	check(strcmp(entry.display_name, "sample") == 0, "deck index defaults display name");
	check(strcmp(entry.deck_json_path, "/root/sample/deck.json") == 0, "deck json path builds");
	check(strcmp(entry.cards_path, "/root/sample/cards.tsv") == 0, "cards path builds");
	check(strcmp(entry.state_path, "/root/sample/state.tsv") == 0, "state path builds");
	check(
		strcmp(entry.settings_path, "/root/sample/settings.tsv") == 0,
		"settings path builds"
	);
	check(strcmp(entry.media_path, "/root/sample/media") == 0, "media path builds");
	check(!deck_index_build_entry(&entry, "/root", ".hidden"), "hidden id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad/id"), "slash id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad\\id"), "backslash id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad id"), "space id rejected");
}

static void test_media_image_loads_rgb565(void)
{
	static const unsigned char content[] = {
		'A', '3', 'I', '1',
		2, 0,
		1, 0,
		0x00, 0xf8,
		0xe0, 0x07,
	};
	struct media_image image;

	write_binary_file(TEST_MEDIA_PATH, content, sizeof(content));

	check(
		media_image_load(&image, TEST_MEDIA_PATH) == MEDIA_IMAGE_LOAD_OK,
		"media image loads"
	);
	check(image.loaded, "media image loaded flag");
	check(image.width == 2, "media image width loads");
	check(image.height == 1, "media image height loads");
	check(image.pixels[0] == 0xf800, "media image first pixel loads");
	check(image.pixels[1] == 0x07e0, "media image second pixel loads");

	remove(TEST_MEDIA_PATH);
}

static void test_media_image_rejects_bad_files(void)
{
	static const unsigned char bad_magic[] = {
		'B', 'A', 'D', '!',
		1, 0,
		1, 0,
		0, 0,
	};
	static const unsigned char too_large[] = {
		'A', '3', 'I', '1',
		(MEDIA_IMAGE_MAX_WIDTH + 1) & 0xff,
		((MEDIA_IMAGE_MAX_WIDTH + 1) >> 8) & 0xff,
		1, 0,
		0, 0,
	};
	struct media_image image;

	remove(TEST_MEDIA_PATH);
	check(
		media_image_load(&image, TEST_MEDIA_PATH) == MEDIA_IMAGE_LOAD_NOT_FOUND,
		"missing media image reports not found"
	);

	write_binary_file(TEST_MEDIA_PATH, bad_magic, sizeof(bad_magic));
	check(
		media_image_load(&image, TEST_MEDIA_PATH) == MEDIA_IMAGE_LOAD_BAD_FORMAT,
		"bad media image magic rejected"
	);

	write_binary_file(TEST_MEDIA_PATH, too_large, sizeof(too_large));
	check(
		media_image_load(&image, TEST_MEDIA_PATH) == MEDIA_IMAGE_LOAD_TOO_LARGE,
		"oversized media image rejected"
	);

	remove(TEST_MEDIA_PATH);
}

static void remove_test_deck_dir(const char *deck_id)
{
	char path[256];

	snprintf(path, sizeof(path), "%s/%s/cards.tsv", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/deck.json", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/settings.tsv", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/settings.tsv.tmp", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/settings.tsv.bak", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/state.tsv", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/state.tsv.tmp", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/state.tsv.bak", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s", TEST_DECK_ROOT, deck_id);
	rmdir(path);
}

static void cleanup_deck_index_test_root(void)
{
	remove_test_deck_dir("alpha");
	remove_test_deck_dir("beta");
	remove_test_deck_dir("broken");
	remove_test_deck_dir("empty");
	remove_test_deck_dir("summary");
	remove_test_deck_dir("zeta");

	for (unsigned int deck_number = 0; deck_number < 18; deck_number++)
	{
		char deck_id[16];

		snprintf(deck_id, sizeof(deck_id), "deck%02u", deck_number);
		remove_test_deck_dir(deck_id);
	}

	rmdir(TEST_DECK_ROOT);
}

static void create_test_deck_dir_with_name(
	const char *deck_id,
	bool has_cards,
	const char *deck_name
)
{
	char path[256];

	snprintf(path, sizeof(path), "%s/%s", TEST_DECK_ROOT, deck_id);
	mkdir(path, 0700);

	if (has_cards)
	{
		snprintf(path, sizeof(path), "%s/%s/cards.tsv", TEST_DECK_ROOT, deck_id);
		write_file(path, "card\tnote\tfront\tback\ttags\n");
	}

	if (deck_name != NULL)
	{
		snprintf(path, sizeof(path), "%s/%s/deck.json", TEST_DECK_ROOT, deck_id);
		write_file(path, deck_name);
	}
}

static void create_test_deck_dir(const char *deck_id, bool has_cards)
{
	create_test_deck_dir_with_name(deck_id, has_cards, NULL);
}

static void test_deck_index_scans_sorted_decks_with_cards(void)
{
	struct deck_index index;
	size_t entry_index;

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	create_test_deck_dir("zeta", true);
	create_test_deck_dir("empty", false);
	create_test_deck_dir("alpha", true);

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == 2, "deck index scans only folders with cards");
	check(strcmp(index.entries[0].id, "alpha") == 0, "deck index sorts first deck");
	check(strcmp(index.entries[1].id, "zeta") == 0, "deck index sorts second deck");
	check(deck_index_find(&index, "alpha", &entry_index), "deck index finds first deck");
	check(entry_index == 0, "deck index first deck index");
	check(deck_index_find(&index, "zeta", &entry_index), "deck index finds second deck");
	check(entry_index == 1, "deck index second deck index");
	check(!deck_index_find(&index, "missing", &entry_index), "deck index rejects missing deck");
	check(!deck_index_find(NULL, "alpha", &entry_index), "deck index rejects null index");
	check(!index.overflowed, "deck index does not report overflow under limit");

	cleanup_deck_index_test_root();
}

static void test_deck_index_loads_display_names(void)
{
	struct deck_index index;

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	create_test_deck_dir_with_name(
		"alpha",
		true,
		"{\n  \"format_version\": 1,\n  \"name\": \"Alpha Deck\"\n}\n"
	);
	create_test_deck_dir_with_name("beta", true, "{ \"name\": \"bad\\u0020name\" }\n");
	create_test_deck_dir_with_name("zeta", true, "{ \"name\": null, \"created_by\": \"x\" }\n");

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == 3, "deck index scans named decks");
	check(strcmp(index.entries[0].id, "alpha") == 0, "named deck id remains folder id");
	check(strcmp(index.entries[0].display_name, "Alpha Deck") == 0, "deck name loads");
	check(strcmp(index.entries[1].display_name, "beta") == 0, "bad deck name falls back");
	check(strcmp(index.entries[2].display_name, "zeta") == 0, "non-string deck name falls back");

	cleanup_deck_index_test_root();
}

static void test_deck_index_reports_overflow(void)
{
	struct deck_index index;

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);

	for (unsigned int deck_number = 0; deck_number < 18; deck_number++)
	{
		char deck_id[16];

		snprintf(deck_id, sizeof(deck_id), "deck%02u", deck_number);
		create_test_deck_dir(deck_id, true);
	}

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == DECK_INDEX_MAX_DECKS, "deck index stops at display limit");
	check(index.overflowed, "deck index reports overflow");
	check(strcmp(index.entries[0].id, "deck00") == 0, "deck index keeps sorted first deck");
	check(strcmp(index.entries[DECK_INDEX_MAX_DECKS - 1].id, "deck15") == 0, "deck index keeps first visible deck set");

	cleanup_deck_index_test_root();
}

static void test_deck_summary_counts_due_cards(void)
{
	struct deck_entry entry;
	struct deck_summary summary;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/summary", TEST_DECK_ROOT);
	mkdir(path, 0700);

	check(
		deck_index_build_entry(&entry, TEST_DECK_ROOT, "summary"),
		"summary deck entry builds"
	);
	write_file(
		entry.cards_path,
		"card-1\tnote-1\tfront 1\tback 1\ttag\n"
		"card-2\tnote-2\tfront 2\tback 2\ttag\n"
		"card-3\tnote-3\tfront 3\tback 3\ttag\n"
		"card-4\tnote-4\tfront 4\tback 4\ttag\n"
	);
	write_file(entry.settings_path, "new_limit\t1\nreview_limit\t0\n");
	write_file(
		entry.state_path,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t19999\t19999\n"
		"card-2\t1\t2\t20000\t1\t2500\t0\t0\t19999\t19999\n"
		"card-3\t0\t2\t20000\t0\t2500\t0\t1\t0\t0\n"
	);

	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_OK, "summary deck loads");
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "summary settings load");
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "summary state loads");
	check(summary.card_count == 4, "summary card count");
	check(summary.due_count == 2, "summary due count includes review and limited new");
	check(summary.new_due_count == 1, "summary new due count honors new limit");
	check(summary.suspended_count == 1, "summary suspended count includes saved state");

	cleanup_deck_index_test_root();
}

static void test_deck_summary_reports_load_error(void)
{
	struct deck_entry entry;
	struct deck_summary summary;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/broken", TEST_DECK_ROOT);
	mkdir(path, 0700);

	check(
		deck_index_build_entry(&entry, TEST_DECK_ROOT, "broken"),
		"broken summary entry builds"
	);

	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_NOT_FOUND, "summary reports missing cards");
	check(summary.card_count == 0, "bad summary has zero cards");
	check(summary.due_count == 0, "bad summary has zero due cards");
	check(summary.new_due_count == 0, "bad summary has zero new cards");
	check(summary.suspended_count == 0, "bad summary has zero suspended cards");

	cleanup_deck_index_test_root();
}

static void test_daily_use_workflow_persists_two_decks(void)
{
	struct deck_index index;
	struct deck alpha_deck;
	struct deck beta_deck;
	struct deck alpha_reloaded_deck;
	struct deck beta_reloaded_deck;
	struct scheduler_session alpha_session;
	struct scheduler_session beta_session;
	struct scheduler_session alpha_reloaded;
	struct scheduler_session beta_reloaded;
	struct app_settings alpha_settings;
	struct app_settings loaded_settings;
	struct deck_summary summary;
	size_t alpha_index = 0;
	size_t beta_index = 0;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/alpha", TEST_DECK_ROOT);
	mkdir(path, 0700);
	snprintf(path, sizeof(path), "%s/beta", TEST_DECK_ROOT);
	mkdir(path, 0700);

	snprintf(path, sizeof(path), "%s/alpha/cards.tsv", TEST_DECK_ROOT);
	write_file(
		path,
		"alpha-1\tnote-1\talpha front 1\talpha back 1\ttag\n"
		"alpha-2\tnote-2\talpha front 2\talpha back 2\ttag\n"
		"alpha-3\tnote-3\talpha front 3\talpha back 3\ttag\n"
	);
	snprintf(path, sizeof(path), "%s/beta/cards.tsv", TEST_DECK_ROOT);
	write_file(
		path,
		"beta-1\tnote-1\tbeta front 1\tbeta back 1\ttag\n"
		"beta-2\tnote-2\tbeta front 2\tbeta back 2\ttag\n"
	);

	deck_index_scan(&index, TEST_DECK_ROOT);
	check(index.count == 2, "daily workflow scans two decks");
	check(deck_index_find(&index, "alpha", &alpha_index), "daily workflow finds alpha");
	check(deck_index_find(&index, "beta", &beta_index), "daily workflow finds beta");

	deck_summary_load(&summary, &index.entries[alpha_index], TEST_TODAY);
	check(summary.deck_load_result == DECK_LOAD_OK, "daily workflow alpha summary loads");
	check(summary.card_count == 3, "daily workflow alpha summary card count");
	check(summary.due_count == 3, "daily workflow alpha starts due");
	check(summary.suspended_count == 0, "daily workflow alpha starts unsuspended");

	load_entry_deck(
		&alpha_deck,
		&index.entries[alpha_index],
		"daily workflow alpha deck loads"
	);
	scheduler_init(&alpha_session, alpha_deck.card_count, TEST_TODAY);
	check(
		review_state_load(
			&alpha_deck,
			&alpha_session,
			index.entries[alpha_index].state_path
		) == REVIEW_STATE_LOAD_NOT_FOUND,
		"daily workflow alpha starts without state"
	);

	scheduler_rate_current(&alpha_session, SCHEDULER_RATING_GOOD);
	check(alpha_session.due_count == 2, "daily workflow alpha rating applies");
	save_entry_state(
		&alpha_deck,
		&alpha_session,
		&index.entries[alpha_index],
		"daily workflow alpha rating saves"
	);
	check(scheduler_undo_last(&alpha_session), "daily workflow alpha rating undo works");
	check(alpha_session.due_count == 3, "daily workflow alpha undo restores due count");
	save_entry_state(
		&alpha_deck,
		&alpha_session,
		&index.entries[alpha_index],
		"daily workflow alpha undo saves"
	);
	scheduler_rate_current(&alpha_session, SCHEDULER_RATING_EASY);
	check(alpha_session.cards[0].due_day == TEST_TODAY + 4, "daily workflow alpha rerates");
	save_entry_state(
		&alpha_deck,
		&alpha_session,
		&index.entries[alpha_index],
		"daily workflow alpha rerating saves"
	);

	alpha_settings.new_limit = 2;
	alpha_settings.review_limit = 4;
	check(
		app_settings_save(
			&alpha_settings,
			index.entries[alpha_index].settings_path
		) == APP_SETTINGS_SAVE_OK,
		"daily workflow alpha settings save"
	);

	load_entry_deck(
		&beta_deck,
		&index.entries[beta_index],
		"daily workflow beta deck loads"
	);
	scheduler_init(&beta_session, beta_deck.card_count, TEST_TODAY);
	check(scheduler_suspend_current(&beta_session), "daily workflow beta suspends card");
	check(scheduler_suspended_count(&beta_session) == 1, "daily workflow beta count suspends");
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta suspension saves"
	);
	deck_summary_load(&summary, &index.entries[beta_index], TEST_TODAY);
	check(summary.suspended_count == 1, "daily workflow beta summary shows suspension");

	check(scheduler_unsuspend_all(&beta_session) == 1, "daily workflow beta restores card");
	check(scheduler_suspended_count(&beta_session) == 0, "daily workflow beta count restores");
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta restore saves"
	);
	scheduler_rate_current(&beta_session, SCHEDULER_RATING_GOOD);
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta review saves"
	);

	check(
		app_settings_load(
			&loaded_settings,
			index.entries[alpha_index].settings_path
		) == APP_SETTINGS_LOAD_OK,
		"daily workflow alpha settings reload"
	);
	check(loaded_settings.new_limit == 2, "daily workflow alpha new limit persists");
	check(loaded_settings.review_limit == 4, "daily workflow alpha review limit persists");

	load_entry_deck(
		&alpha_reloaded_deck,
		&index.entries[alpha_index],
		"daily workflow alpha reload deck"
	);
	scheduler_init(&alpha_reloaded, alpha_reloaded_deck.card_count, TEST_TODAY);
	scheduler_set_daily_limits(
		&alpha_reloaded,
		loaded_settings.new_limit,
		loaded_settings.review_limit
	);
	check(
		review_state_load(
			&alpha_reloaded_deck,
			&alpha_reloaded,
			index.entries[alpha_index].state_path
		) == REVIEW_STATE_LOAD_OK,
		"daily workflow alpha state reload"
	);
	check(
		alpha_reloaded.cards[0].review_count == 1,
		"daily workflow alpha review persists"
	);
	check(
		alpha_reloaded.cards[0].last_rating == SCHEDULER_RATING_EASY,
		"daily workflow alpha rating persists"
	);
	check(!scheduler_card_is_due(&alpha_reloaded, 0), "daily workflow alpha reviewed card hidden");
	check(alpha_reloaded.due_count == 1, "daily workflow alpha new limit applies after reload");

	load_entry_deck(
		&beta_reloaded_deck,
		&index.entries[beta_index],
		"daily workflow beta reload deck"
	);
	scheduler_init(&beta_reloaded, beta_reloaded_deck.card_count, TEST_TODAY);
	check(
		review_state_load(
			&beta_reloaded_deck,
			&beta_reloaded,
			index.entries[beta_index].state_path
		) == REVIEW_STATE_LOAD_OK,
		"daily workflow beta state reload"
	);
	check(scheduler_suspended_count(&beta_reloaded) == 0, "daily workflow beta restore persists");
	check(beta_reloaded.cards[0].review_count == 1, "daily workflow beta review persists");

	cleanup_deck_index_test_root();
}

int main(void)
{
	test_parse_card_line();
	test_reject_bad_card_line();
	test_parse_card_line_with_media();
	test_deck_load_rejects_duplicate_card_ids();
	test_tracked_sample_decks_load();
	test_scheduler_schedules_due_days();
	test_scheduler_rejects_invalid_rating();
	test_scheduler_new_again_stays_in_initial_learning();
	test_scheduler_scales_review_intervals();
	test_scheduler_undo_last_rating();
	test_scheduler_suspend_current();
	test_scheduler_suspend_last_due_card();
	test_scheduler_unsuspend_all();
	test_scheduler_limits_new_cards();
	test_scheduler_allows_started_new_card_after_limit();
	test_scheduler_limits_review_cards();
	test_review_state_missing_file();
	test_review_state_round_trip();
	test_review_state_round_trip_suspended_card();
	test_review_state_loads_backup_when_primary_missing();
	test_review_state_loads_previous_current_format();
	test_review_state_loads_suspended_format();
	test_review_state_loads_legacy_done_format();
	test_review_state_bad_load_does_not_mutate_session();
	test_review_state_delete_removes_save_artifacts();
	test_app_settings_missing_file_uses_defaults();
	test_app_settings_loads_limits();
	test_app_settings_loads_backup_when_primary_missing();
	test_app_settings_bad_file_uses_defaults();
	test_app_settings_save_round_trip();
	test_app_settings_save_replaces_existing_file();
	test_deck_index_builds_paths();
	test_media_image_loads_rgb565();
	test_media_image_rejects_bad_files();
	test_deck_index_scans_sorted_decks_with_cards();
	test_deck_index_loads_display_names();
	test_deck_index_reports_overflow();
	test_deck_summary_counts_due_cards();
	test_deck_summary_reports_load_error();
	test_daily_use_workflow_persists_two_decks();

	if (failures != 0)
	{
		printf("%d deck/scheduler test failures\n", failures);
		return 1;
	}

	puts("deck/scheduler tests passed");
	return 0;
}
