#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app_settings.h"
#include "app_controls.h"
#include "app_layout.h"
#include "app_power.h"
#include "app_review.h"
#include "app_status.h"
#include "app_time.h"
#include "app_text.h"
#include "deck.h"
#include "deck_index.h"
#include "deck_summary.h"
#include "review_log.h"
#include "review_state.h"
#include "scheduler.h"
#include "storage.h"

#define TEST_STATE_PATH "/tmp/anki3ds-review-state-test.tsv"
#define TEST_STATE_TEMP_PATH TEST_STATE_PATH ".tmp"
#define TEST_STATE_BACKUP_PATH TEST_STATE_PATH ".bak"
#define TEST_SETTINGS_PATH "/tmp/anki3ds-settings-test.tsv"
#define TEST_SETTINGS_TEMP_PATH TEST_SETTINGS_PATH ".tmp"
#define TEST_SETTINGS_BACKUP_PATH TEST_SETTINGS_PATH ".bak"
#define TEST_STORAGE_PATH "/tmp/anki3ds-storage-test.tsv"
#define TEST_STORAGE_TEMP_PATH TEST_STORAGE_PATH ".tmp"
#define TEST_STORAGE_BACKUP_PATH TEST_STORAGE_PATH ".bak"
#define TEST_CARDS_PATH "/tmp/anki3ds-cards-test.tsv"
#define TEST_REVIEW_LOG_PATH "/tmp/anki3ds-review-log-test.tsv"
#define TEST_REVIEW_LOG_TEMP_PATH TEST_REVIEW_LOG_PATH ".tmp"
#define TEST_REVIEW_LOG_BACKUP_PATH TEST_REVIEW_LOG_PATH ".bak"
#define TEST_DECK_ROOT "/tmp/anki3ds-deck-index-test"
#define TEST_TODAY 20000
#define TEST_SECONDS_PER_DAY 86400

static int failures;

static void write_file(const char *path, const char *content);
static void write_numbered_cards_file(const char *path, size_t card_count);
static void write_newline_terminated_filler_file(const char *path, size_t size);
static bool file_equals(const char *path, const char *content);
static long file_size(const char *path);
static long file_line_count(const char *path);
static void build_test_deck(struct deck *deck);
static void build_review_log_entry(struct review_log_entry *entry);
static void append_review_log_transition(
	const char *path,
	time_t timestamp,
	unsigned int day,
	enum review_log_event event,
	const char *card_id,
	enum scheduler_rating rating,
	const struct scheduler_card *before,
	const struct scheduler_card *after,
	const char *message
);

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
	check(
		deck_parse_card_line(&card, "card\\t1\tnote\tfront\tback\ttag") ==
			DECK_PARSE_BAD_CARD_ID,
		"card id escaped tab rejected"
	);
	check(
		deck_parse_card_line(&card, "card\\n1\tnote\tfront\tback\ttag") ==
			DECK_PARSE_BAD_CARD_ID,
		"card id escaped newline rejected"
	);
	check(
		deck_parse_card_line(&card, "#card\tnote\tfront\tback\ttag") ==
			DECK_PARSE_BAD_CARD_ID,
		"card id state metadata prefix rejected"
	);
	check(
		deck_parse_card_line(&card, "card\tnote\tfront\tback\ttag\textra") ==
			DECK_PARSE_BAD_FIELD_COUNT,
		"extra text card fields rejected"
	);
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

static void test_deck_load_accepts_final_line_without_newline(void)
{
	struct deck deck;

	write_file(TEST_CARDS_PATH, "card-1\tnote-1\tfront\tback\ttag");
	deck_init(&deck, "no-final-newline");

	check(
		deck_load_cards(&deck, TEST_CARDS_PATH) == DECK_LOAD_OK,
		"deck load accepts final row without newline"
	);
	check(deck.card_count == 1, "deck load stores no-newline card");
	check(strcmp(deck.cards[0].front, "front") == 0, "no-newline card front loads");

	remove(TEST_CARDS_PATH);
}

static void test_deck_load_card_limit(void)
{
	struct deck deck;

	write_numbered_cards_file(TEST_CARDS_PATH, DECK_MAX_CARDS);
	deck_init(&deck, "limit-test");
	check(
		deck_load_cards(&deck, TEST_CARDS_PATH) == DECK_LOAD_OK,
		"deck load accepts card limit"
	);
	check(deck.card_count == DECK_MAX_CARDS, "deck load stores card limit");

	write_numbered_cards_file(TEST_CARDS_PATH, DECK_MAX_CARDS + 1);
	deck_init(&deck, "too-large-test");
	check(
		deck_load_cards(&deck, TEST_CARDS_PATH) == DECK_LOAD_TOO_LARGE,
		"deck load rejects beyond card limit"
	);
	check(
		strcmp(deck_load_result_name(DECK_LOAD_OUT_OF_MEMORY), "out of memory") == 0,
		"deck load out-of-memory result names"
	);

	remove(TEST_CARDS_PATH);
}

static void test_tracked_text_sample_decks_load(void)
{
	struct deck deck;
	struct app_settings settings;

	deck_init(&deck, "sample");
	check(
		deck_load_cards(&deck, "sample-decks/sample/cards.tsv") == DECK_LOAD_OK,
		"tracked sample deck loads"
	);
	check(deck.card_count == 11, "tracked sample deck card count");
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
}

static void test_app_power_battery_poll_schedule(void)
{
	time_t next_poll_time = 0;

	app_power_schedule_next_battery_poll(&next_poll_time, 1000);
	check(
		next_poll_time == 1000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"battery poll schedules ten minutes out"
	);
	check(
		!app_power_battery_poll_is_due(&next_poll_time, 1599),
		"battery poll is not due before interval"
	);
	check(
		app_power_battery_poll_is_due(&next_poll_time, 1600),
		"battery poll is due at interval"
	);
}

static void test_app_power_battery_poll_arms_after_missing_clock(void)
{
	time_t next_poll_time = 0;

	app_power_schedule_next_battery_poll(&next_poll_time, (time_t)-1);
	check(next_poll_time == 0, "battery poll does not arm without clock");
	check(
		!app_power_battery_poll_is_due(&next_poll_time, (time_t)-1),
		"battery poll is not due without clock"
	);
	check(next_poll_time == 0, "battery poll stays unarmed without clock");
	check(
		!app_power_battery_poll_is_due(&next_poll_time, 2000),
		"battery poll arms when clock returns"
	);
	check(
		next_poll_time == 2000 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"battery poll stores recovered clock schedule"
	);
}

static void test_app_power_battery_sample_policy(void)
{
	time_t next_poll_time = 1600;

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_UNCHANGED
	);
	check(
		next_poll_time == 1600 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"sample completion schedules next battery poll"
	);
	check(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED
		),
		"closed shell does not change battery display"
	);
	check(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_READ_FAILED
		),
		"battery read failure keeps previous display"
	);
	check(
		!app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_UNCHANGED
		),
		"unchanged battery sample stays quiet"
	);
	check(
		app_power_battery_sample_changes_display(
			APP_POWER_BATTERY_SAMPLE_CHANGED
		),
		"changed battery sample redraws display"
	);

	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_READ_FAILED
	);
	check(
		next_poll_time == 1600 + APP_POWER_BATTERY_RETRY_INTERVAL_SECONDS,
		"battery read failure schedules short retry interval"
	);

	next_poll_time = 1600;
	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		(time_t)-1,
		APP_POWER_BATTERY_SAMPLE_READ_FAILED
	);
	check(
		!app_power_battery_poll_is_due(&next_poll_time, (time_t)-1),
		"battery read failure waits while clock is unavailable"
	);
	check(
		app_power_battery_poll_is_due(&next_poll_time, 1700),
		"battery read failure retries as soon as clock returns"
	);

	next_poll_time = 1600;
	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_SKIPPED_CLOSED
	);
	check(
		next_poll_time == 1600 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"closed shell sample schedules normal poll interval"
	);

	next_poll_time = 1600;
	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_UNAVAILABLE
	);
	check(
		next_poll_time == 1600 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"unavailable battery service schedules normal poll interval"
	);

	next_poll_time = 1600;
	app_power_schedule_next_battery_poll_after_sample(
		&next_poll_time,
		1600,
		APP_POWER_BATTERY_SAMPLE_CHANGED
	);
	check(
		next_poll_time == 1600 + APP_POWER_BATTERY_POLL_INTERVAL_SECONDS,
		"battery sample schedules next open-shell poll"
	);
}

static void test_app_power_battery_display_state(void)
{
	check(
		app_power_battery_display_state(false, false, 0) ==
			APP_POWER_BATTERY_DISPLAY_UNAVAILABLE,
		"missing battery sample displays unavailable"
	);
	check(
		app_power_battery_display_state(true, true, 1) ==
			APP_POWER_BATTERY_DISPLAY_CHARGING,
		"charging battery display wins over low level"
	);
	check(
		app_power_battery_display_state(true, false, APP_POWER_BATTERY_LOW_LEVEL) ==
			APP_POWER_BATTERY_DISPLAY_LOW,
		"low battery level displays warning"
	);
	check(
		app_power_battery_display_state(
			true,
			false,
			APP_POWER_BATTERY_LOW_LEVEL + 1
		) == APP_POWER_BATTERY_DISPLAY_NORMAL,
		"healthy battery level displays normal"
	);
}

static void test_app_power_idle_input_backoff(void)
{
	check(
		app_power_idle_input_wait_ns(0) ==
			APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS,
		"idle input starts with responsive wait"
	);
	check(
		app_power_idle_input_wait_ns(
			APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_WAIT_INITIAL_NS,
		"idle input keeps initial wait through fast tier"
	);
	check(
		app_power_idle_input_wait_ns(APP_POWER_IDLE_INPUT_FAST_WAIT_COUNT) ==
			APP_POWER_IDLE_INPUT_WAIT_MID_NS,
		"idle input backs off to mid wait"
	);
	check(
		app_power_idle_input_wait_ns(
			APP_POWER_IDLE_INPUT_MID_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_WAIT_MID_NS,
		"idle input keeps mid wait through mid tier"
	);
	check(
		app_power_idle_input_wait_ns(APP_POWER_IDLE_INPUT_MID_WAIT_COUNT) ==
			APP_POWER_IDLE_INPUT_WAIT_MAX_NS,
		"idle input backs off to max wait"
	);
	check(
		APP_POWER_IDLE_INPUT_WAIT_MAX_NS >= 1000000000LL,
		"idle input max wait avoids high-frequency idle polling"
	);
	check(
		app_power_next_idle_input_wait_count(0) == 1,
		"idle input wait count increments"
	);
	check(
		app_power_next_idle_input_wait_count(
			APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT - 1
		) == APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT,
		"idle input wait count reaches cap"
	);
	check(
		app_power_next_idle_input_wait_count(
			APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT
		) == APP_POWER_IDLE_INPUT_MAX_WAIT_COUNT,
		"idle input wait count remains capped"
	);
}

static void test_app_text_counts_utf8_columns(void)
{
	const char *text = "a" "\xc3" "\xa9" "\xf0" "\x9f" "\x99" "\x82" "b";
	const char invalid[] = { (char)0x80, 'a', '\0' };

	check(app_text_utf8_char_length("a") == 1, "ASCII char length is one byte");
	check(
		app_text_utf8_char_length("\xc3" "\xa9") == 2,
		"two-byte UTF-8 char length"
	);
	check(
		app_text_utf8_char_length("\xf0" "\x9f" "\x99" "\x82") == 4,
		"four-byte UTF-8 char length"
	);
	check(app_text_utf8_char_length(invalid) == 1, "invalid UTF-8 advances one byte");
	check(app_text_column_count(text) == 4, "UTF-8 text counts by characters");
	check(
		app_text_byte_count_for_columns(text, 2) == 3,
		"UTF-8 byte count keeps two-byte char whole"
	);
	check(
		app_text_byte_count_for_columns(text, 3) == 7,
		"UTF-8 byte count keeps four-byte char whole"
	);
	check(
		app_text_byte_count_for_columns("\xc3" "\xa9" "b", 1) == 2,
		"UTF-8 byte count keeps narrow truncation whole"
	);
	check(
		app_text_byte_count_for_columns(text, 20) == strlen(text),
		"UTF-8 byte count can include full text"
	);
}

static void test_app_text_counts_wrapped_rows(void)
{
	check(app_text_wrapped_row_count("abc", 3) == 1, "exact text fits one row");
	check(app_text_wrapped_row_count("abcd", 3) == 2, "wrapped text counts rows");
	check(
		app_text_wrapped_row_count("ab\ncd", 3) == 2,
		"explicit newline counts rows"
	);
	check(
		app_text_wrapped_row_count("ab\r\ncd", 3) == 2,
		"carriage return is ignored in row count"
	);
	check(
		app_text_wrapped_row_count("\xc3" "\xa9" "bc", 3) == 1,
		"UTF-8 row count keeps codepoint columns"
	);
	check(
		app_text_wrapped_row_count("\xc3" "\xa9" "bcd", 3) == 2,
		"UTF-8 wrapped text counts rows"
	);
	check(app_text_wrapped_row_count("", 3) == 1, "empty text still has one row");
	check(app_text_wrapped_row_count("abc", 0) == 0, "zero columns has zero rows");
	check(
		app_text_max_scroll_offset("abcd", 3, 1) == 1,
		"scroll offset exposes wrapped row"
	);
	check(
		app_text_max_scroll_offset("abcd", 3, 2) == 0,
		"no scroll when visible rows fit"
	);
	check(
		app_text_max_scroll_offset("abc", 3, 0) == 0,
		"zero visible rows has no scroll"
	);
}

static void set_test_timezone(const char *timezone)
{
	check(setenv("TZ", timezone, 1) == 0, "test timezone sets");
	tzset();
}

static time_t make_utc_timestamp(
	int year,
	int month,
	int month_day,
	int hour,
	int minute,
	int second
)
{
	struct tm timestamp;

	set_test_timezone("UTC0");
	memset(&timestamp, 0, sizeof(timestamp));
	timestamp.tm_year = year - 1900;
	timestamp.tm_mon = month - 1;
	timestamp.tm_mday = month_day;
	timestamp.tm_hour = hour;
	timestamp.tm_min = minute;
	timestamp.tm_sec = second;
	timestamp.tm_isdst = -1;

	return mktime(&timestamp);
}

static void test_app_time_local_calendar_day(void)
{
	time_t timestamp = make_utc_timestamp(2026, 6, 3, 0, 30, 0);
	unsigned int local_day;
	unsigned int utc_day;

	check(app_time_day_from_local_date(1970, 1, 1) == 0, "epoch local day");
	check(app_time_day_from_local_date(1970, 3, 1) == 59, "non-leap March day");
	check(app_time_day_from_local_date(1972, 3, 1) == 790, "leap March day");
	check(
		app_time_day_from_local_date(5000, 1, 1) == SCHEDULER_MAX_DAY,
		"future local day clamps"
	);
	check(timestamp != (time_t)-1, "UTC timestamp builds");

	set_test_timezone("EST5EDT,M3.2.0,M11.1.0");
	local_day = app_time_local_day_from_time(timestamp);
	utc_day = (unsigned int)(timestamp / TEST_SECONDS_PER_DAY);

	check(
		local_day == app_time_day_from_local_date(2026, 6, 2),
		"local evening stays on local calendar day"
	);
	check(local_day != utc_day, "local day differs from UTC rollover day");

	set_test_timezone("UTC0");
}

static void test_scheduler_schedules_due_days(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
	check(scheduler_current_index(&session) == 0, "scheduler starts at first card");
	check(session.due_count == 3, "new cards start due");
	check(scheduler_reviewed_today_count(&session) == 0, "no cards reviewed today");

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(session.due_count == 3, "again keeps card due today");
	check(session.cards[0].due_day == TEST_TODAY, "again stays due today");
	check(session.cards[0].interval_days == 0, "again keeps zero-day interval");
	check(session.cards[0].ease_permille == 2300, "again lowers ease");
	check(scheduler_current_index(&session) == 1, "again advances to next card");
	check(scheduler_reviewed_today_count(&session) == 1, "again touches one card today");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 2, "good schedules one card out");
	check(session.cards[1].due_day == TEST_TODAY + 1, "good schedules tomorrow");
	check(session.cards[1].interval_days == 1, "good starts one-day interval");
	check(scheduler_current_index(&session) == 0, "learning card is revisited before new card");
	check(scheduler_reviewed_today_count(&session) == 2, "good touches second card today");

	scheduler_rate_current(&session, SCHEDULER_RATING_HARD);
	check(session.due_count == 1, "hard schedules learning card out");
	check(session.cards[0].due_day == TEST_TODAY + 1, "hard schedules tomorrow");
	check(scheduler_current_index(&session) == 2, "new card follows learning card");
	check(
		scheduler_reviewed_today_count(&session) == 2,
		"rerating same card keeps today card count unique"
	);

	scheduler_rate_current(&session, SCHEDULER_RATING_EASY);
	check(session.due_count == 0, "easy schedules final due card out");
	check(session.cards[2].due_day == TEST_TODAY + 4, "easy starts four-day interval");
	check(scheduler_is_complete(&session), "scheduler completes when no due cards remain");
	check(session.rating_counts[SCHEDULER_RATING_AGAIN] == 1, "again count tracked");
	check(session.rating_counts[SCHEDULER_RATING_HARD] == 1, "hard count tracked");
	check(session.rating_counts[SCHEDULER_RATING_GOOD] == 1, "good count tracked");
	check(session.rating_counts[SCHEDULER_RATING_EASY] == 1, "easy count tracked");
	check(session.reviewed_count == 4, "reviewed count tracks ratings");
	check(scheduler_reviewed_today_count(&session) == 3, "today count tracks cards");
}

static void test_app_controls_review_front_actions(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;

	check(
		app_controls_can_open(APP_CONTROL_MODE_REVIEW, false),
		"controls can open before reveal"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_REVIEW, true),
		"controls cannot steal revealed Y rating"
	);
	check(
		app_controls_should_show_answer(APP_CONTROL_BUTTON_A, false),
		"A shows answer before reveal"
	);
	check(
		!app_controls_should_show_answer(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_START,
			false
		),
		"A with START does not show answer"
	);
	check(
		!app_controls_should_show_answer(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			false
		),
		"A with D-pad does not show answer"
	);
	check(
		!app_controls_should_show_answer_triggered(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			false
		),
		"A tap with held D-pad does not show answer"
	);
	check(
		!app_controls_should_show_answer_triggered(
			0,
			APP_CONTROL_BUTTON_A,
			false
		),
		"held A without new press does not show answer"
	);
	check(
		!app_controls_should_show_answer(APP_CONTROL_BUTTON_A, true),
		"A does not show answer after reveal"
	);
	check(
		!app_controls_rating_for_buttons(APP_CONTROL_BUTTON_A, false, &rating),
		"A is not Easy before reveal"
	);
	check(
		!app_controls_rating_for_buttons(APP_CONTROL_BUTTON_X, false, &rating),
		"X is not Hard before reveal"
	);
	check(
		!app_controls_rating_for_buttons(APP_CONTROL_BUTTON_Y, false, &rating),
		"Y is not Again before reveal"
	);
}

static void test_app_controls_review_rating_keys(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;

	check(
		app_controls_rating_for_buttons(APP_CONTROL_BUTTON_Y, true, &rating),
		"Y rates after reveal"
	);
	check(rating == SCHEDULER_RATING_AGAIN, "Y maps to Again");
	check(
		app_controls_rating_for_buttons(APP_CONTROL_BUTTON_X, true, &rating),
		"X rates after reveal"
	);
	check(rating == SCHEDULER_RATING_HARD, "X maps to Hard");
	check(
		app_controls_rating_for_buttons(APP_CONTROL_BUTTON_B, true, &rating),
		"B rates after reveal"
	);
	check(rating == SCHEDULER_RATING_GOOD, "B maps to Good");
	check(
		app_controls_rating_for_buttons(APP_CONTROL_BUTTON_A, true, &rating),
		"A rates after reveal"
	);
	check(rating == SCHEDULER_RATING_EASY, "A maps to Easy");
	check(
		app_controls_rating_for_trigger(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A,
			true,
			&rating
		),
		"A trigger rates after reveal"
	);
	check(rating == SCHEDULER_RATING_EASY, "A trigger maps to Easy");
}

static void test_app_controls_rejects_ambiguous_ratings(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_GOOD;

	check(
		!app_controls_rating_for_buttons(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_Y,
			true,
			&rating
		),
		"two rating buttons are ignored"
	);
	check(rating == SCHEDULER_RATING_GOOD, "ambiguous rating leaves output unchanged");
	check(
		!app_controls_rating_for_buttons(APP_CONTROL_BUTTON_A, true, NULL),
		"rating output is required"
	);
	check(
		!app_controls_rating_for_buttons(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_START,
			true,
			&rating
		),
		"rating with START is ignored"
	);
	check(
		!app_controls_rating_for_buttons(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			true,
			&rating
		),
		"rating with D-pad is ignored"
	);
	check(
		!app_controls_rating_for_trigger(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			true,
			&rating
		),
		"rating tap with held D-pad is ignored"
	);
	check(
		!app_controls_rating_for_trigger(
			0,
			APP_CONTROL_BUTTON_A,
			true,
			&rating
		),
		"held rating button without new press is ignored"
	);
}

static void test_app_controls_rejects_ambiguous_dpad_axes(void)
{
	bool down = true;
	bool right = true;

	check(
		app_controls_up_down_direction(APP_CONTROL_BUTTON_UP, &down),
		"up/down axis accepts up"
	);
	check(!down, "up/down axis maps up to false");
	check(
		app_controls_up_down_direction(APP_CONTROL_BUTTON_DOWN, &down),
		"up/down axis accepts down"
	);
	check(down, "up/down axis maps down to true");
	check(
		app_controls_up_down_triggered(
			APP_CONTROL_BUTTON_DOWN,
			APP_CONTROL_BUTTON_DOWN,
			&down
		),
		"up/down trigger accepts down"
	);
	check(down, "up/down trigger maps down to true");
	check(
		!app_controls_up_down_direction(
			APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_DOWN,
			&down
		),
		"up/down axis rejects opposite directions"
	);
	check(down, "ambiguous up/down leaves output unchanged");
	check(
		!app_controls_up_down_direction(
			APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_RIGHT,
			&down
		),
		"up/down axis rejects diagonal directions"
	);
	check(down, "diagonal up/down leaves output unchanged");
	check(
		!app_controls_up_down_direction(
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_A,
			&down
		),
		"up/down axis rejects command chords"
	);
	check(down, "command chord up/down leaves output unchanged");
	check(
		!app_controls_up_down_triggered(
			APP_CONTROL_BUTTON_DOWN,
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_A,
			&down
		),
		"up/down trigger rejects held command chord"
	);
	check(
		!app_controls_up_down_triggered(
			APP_CONTROL_BUTTON_DOWN,
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT,
			&down
		),
		"up/down trigger rejects held diagonal"
	);
	check(
		!app_controls_up_down_triggered(0, APP_CONTROL_BUTTON_DOWN, &down),
		"up/down trigger requires new or repeated direction"
	);
	check(
		!app_controls_up_down_direction(APP_CONTROL_BUTTON_UP, NULL),
		"up/down axis output is required"
	);

	check(
		app_controls_left_right_direction(APP_CONTROL_BUTTON_LEFT, &right),
		"left/right axis accepts left"
	);
	check(!right, "left/right axis maps left to false");
	check(
		app_controls_left_right_direction(APP_CONTROL_BUTTON_RIGHT, &right),
		"left/right axis accepts right"
	);
	check(right, "left/right axis maps right to true");
	check(
		app_controls_left_right_triggered(
			APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right trigger accepts right"
	);
	check(right, "left/right trigger maps right to true");
	check(
		!app_controls_left_right_direction(
			APP_CONTROL_BUTTON_LEFT | APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right axis rejects opposite directions"
	);
	check(right, "ambiguous left/right leaves output unchanged");
	check(
		!app_controls_left_right_direction(
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right axis rejects diagonal directions"
	);
	check(right, "diagonal left/right leaves output unchanged");
	check(
		!app_controls_left_right_direction(
			APP_CONTROL_BUTTON_B | APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right axis rejects command chords"
	);
	check(right, "command chord left/right leaves output unchanged");
	check(
		!app_controls_left_right_triggered(
			APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_B | APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right trigger rejects held command chord"
	);
	check(
		!app_controls_left_right_triggered(
			APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT,
			&right
		),
		"left/right trigger rejects held diagonal"
	);
	check(
		!app_controls_left_right_triggered(0, APP_CONTROL_BUTTON_RIGHT, &right),
		"left/right trigger requires new or repeated direction"
	);
	check(
		!app_controls_left_right_direction(APP_CONTROL_BUTTON_LEFT, NULL),
		"left/right axis output is required"
	);
}

static void test_app_controls_requires_single_command(void)
{
	unsigned int confirm_cancel_mask =
		APP_CONTROL_COMMAND_BUTTON_MASK;

	check(
		app_controls_command_pressed(APP_CONTROL_BUTTON_A, APP_CONTROL_BUTTON_A),
		"command helper accepts exact command"
	);
	check(
		app_controls_command_triggered(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A
		),
		"command trigger accepts exact command"
	);
	check(
		!app_controls_command_pressed(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B,
			APP_CONTROL_BUTTON_A
		),
		"command helper rejects command chord"
	);
	check(
		!app_controls_command_pressed(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_A
		),
		"command helper rejects navigation chord"
	);
	check(
		!app_controls_command_triggered(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_A
		),
		"command trigger rejects held navigation chord"
	);
	check(
		!app_controls_command_triggered(
			0,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A
		),
		"command trigger requires newly pressed command"
	);
	check(
		app_controls_single_command(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A,
			confirm_cancel_mask
		),
		"single command accepts exact confirm"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B,
			APP_CONTROL_BUTTON_A,
			confirm_cancel_mask
		),
		"single command rejects confirm with cancel"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_Y,
			APP_CONTROL_BUTTON_A,
			confirm_cancel_mask | APP_CONTROL_BUTTON_Y
		),
		"single command rejects extra face button"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_L,
			APP_CONTROL_BUTTON_A,
			confirm_cancel_mask
		),
		"single command rejects shoulder command"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_INPUT_MASK
		),
		"single command rejects navigation chord"
	);
	check(
		app_controls_single_command(
			APP_CONTROL_BUTTON_START,
			APP_CONTROL_BUTTON_START,
			confirm_cancel_mask
		),
		"single command accepts exact start"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_START | APP_CONTROL_BUTTON_B,
			APP_CONTROL_BUTTON_START,
			confirm_cancel_mask
		),
		"single command rejects start with cancel"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_B,
			APP_CONTROL_BUTTON_A,
			confirm_cancel_mask
		),
		"single command rejects different command"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B,
			confirm_cancel_mask
		),
		"single command rejects multi-bit target"
	);
	check(
		!app_controls_single_command(
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_A,
			APP_CONTROL_BUTTON_B | APP_CONTROL_BUTTON_SELECT
		),
		"single command rejects target outside mask"
	);
}

static void test_app_controls_modal_controls(void)
{
	check(
		app_controls_can_open(APP_CONTROL_MODE_DECK_SELECT, false),
		"controls can open from deck select"
	);
	check(
		app_controls_can_open(APP_CONTROL_MODE_SETTINGS, false),
		"controls can open from settings"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_CONTROLS, false),
		"controls screen does not reopen itself"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_CONFIRM_EXIT, false),
		"controls cannot open from exit confirmation"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_CONFIRM_RESTORE, false),
		"controls cannot open from restore confirmation"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_CONFIRM_SUSPEND, false),
		"controls cannot open from suspend confirmation"
	);
	check(
		!app_controls_can_open(APP_CONTROL_MODE_CONFIRM_RESET, false),
		"controls cannot open from reset confirmation"
	);
}

static void test_app_controls_classifies_app_actions(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	enum app_control_action action;

	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_EXIT, "exit confirmation accepts A");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_EXIT, "exit confirmation cancels with B");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_EXIT, "exit confirmation cancels with select");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "exit confirmation ignores confirm chord");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		0,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "held B does not cancel exit again");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_EXIT,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "exit confirmation does not reopen exit");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "start opens exit before settings");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "start opens exit from controls");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CLOSE_CONTROLS, "B closes controls screen");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CLOSE_CONTROLS, "Y closes controls screen");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CLOSE_CONTROLS, "SELECT closes controls screen");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		0,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "held B does not close controls again");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONTROLS,
		false,
		true,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "controls ignores command chords");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_DECK_SELECT,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_CONTROLS, "Y opens deck controls");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "Y does not open controls in modal");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_LOAD_ERROR,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT, "SELECT exits load error");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_LOAD_ERROR,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT, "B exits load error");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT, "B leaves front review");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SUMMARY,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RETURN_TO_DECK_SELECT, "B leaves summary");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_SHOW_ANSWER, "A reveals front review");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		0,
		APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "held A does not reveal again");

	rating = SCHEDULER_RATING_COUNT;
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RATE, "revealed Y rates instead of controls");
	check(rating == SCHEDULER_RATING_AGAIN, "revealed Y maps to Again");
	rating = SCHEDULER_RATING_COUNT;
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_RATE, "revealed B rates instead of leaving");
	check(rating == SCHEDULER_RATING_GOOD, "revealed B maps to Good");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A,
		NULL
	);
	check(action == APP_CONTROL_ACTION_NONE, "rating action requires output");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		true,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_ACTIONS, "select opens review actions");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SUMMARY,
		false,
		true,
		APP_CONTROL_BUTTON_L,
		APP_CONTROL_BUTTON_L,
		&rating
	);
	check(action == APP_CONTROL_ACTION_UNDO, "L undoes from safe summary");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SUMMARY,
		false,
		false,
		APP_CONTROL_BUTTON_L,
		APP_CONTROL_BUTTON_L,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "unsafe summary blocks undo");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN,
		&rating
	);
	check(action == APP_CONTROL_ACTION_SCROLL_DOWN, "D-pad down scrolls review text");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_UP,
		APP_CONTROL_BUTTON_UP,
		&rating
	);
	check(action == APP_CONTROL_ACTION_SCROLL_UP, "D-pad up scrolls review text");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "scroll ignores command chords");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SUMMARY,
		false,
		true,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "summary does not scroll review text");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		true,
		APP_CONTROL_BUTTON_R,
		APP_CONTROL_BUTTON_R,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_SUSPEND, "R opens suspend confirmation");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_REVIEW,
		false,
		false,
		APP_CONTROL_BUTTON_R,
		APP_CONTROL_BUTTON_R,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "unsafe review blocks suspend");
}

static void test_app_controls_settings_classifies_local_commands(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	enum app_control_action action;

	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_CONTROLS, "Y opens settings controls");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "START opens settings exit");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_SAVE_SETTINGS, "settings A saves");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_SETTINGS, "settings B cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_SETTINGS, "settings SELECT cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_RIGHT,
		APP_CONTROL_BUTTON_RIGHT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "settings value change remains local");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_RIGHT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "settings save ignores value chord");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "settings controls ignores chords");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_SETTINGS,
		false,
		true,
		APP_CONTROL_BUTTON_START | APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_START | APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "settings exit ignores chords");
}

static void test_app_controls_action_confirm_classifies_local_commands(void)
{
	enum scheduler_rating rating = SCHEDULER_RATING_COUNT;
	enum app_control_action action;

	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_CONTROLS, "Y opens actions controls");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "START opens actions exit");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CHOOSE_ACTION, "actions A chooses item");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_ACTIONS, "actions B cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_ACTIONS, "actions SELECT cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "actions movement remains local");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_DOWN,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "actions choose ignores movement chord");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_ACTIONS,
		false,
		true,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_Y | APP_CONTROL_BUTTON_A,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "actions controls ignores chords");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "START opens restore exit");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_X,
		APP_CONTROL_BUTTON_X,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_RESTORE, "restore X confirms");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_RESTORE, "restore B cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_RESTORE, "restore SELECT cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "restore confirm ignores cancel chord");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESTORE,
		false,
		true,
		APP_CONTROL_BUTTON_Y,
		APP_CONTROL_BUTTON_Y,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "restore Y cannot open controls");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "START opens suspend exit");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		APP_CONTROL_BUTTON_X,
		APP_CONTROL_BUTTON_X,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_SUSPEND, "suspend X confirms");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		APP_CONTROL_BUTTON_SELECT,
		APP_CONTROL_BUTTON_SELECT,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_SUSPEND, "suspend SELECT cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_SUSPEND,
		false,
		true,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "suspend confirm ignores cancel chord");

	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		APP_CONTROL_BUTTON_START,
		APP_CONTROL_BUTTON_START,
		&rating
	);
	check(action == APP_CONTROL_ACTION_OPEN_EXIT, "START opens reset exit");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		APP_CONTROL_BUTTON_X,
		APP_CONTROL_BUTTON_X,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CONFIRM_RESET, "reset X confirms");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_CANCEL_RESET, "reset B cancels");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		APP_CONTROL_BUTTON_X | APP_CONTROL_BUTTON_B,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "reset confirm ignores cancel chord");
	action = app_controls_classify_action(
		APP_CONTROL_MODE_CONFIRM_RESET,
		false,
		true,
		APP_CONTROL_BUTTON_START | APP_CONTROL_BUTTON_X,
		APP_CONTROL_BUTTON_START | APP_CONTROL_BUTTON_X,
		&rating
	);
	check(action == APP_CONTROL_ACTION_NONE, "reset exit ignores confirm chord");
}

static void test_app_controls_navigation_repeat(void)
{
	struct app_control_repeat repeat;
	unsigned int repeated;

	check(
		APP_CONTROL_REPEAT_INITIAL_TICKS >= 15,
		"navigation repeat does not treat quick taps as holds"
	);
	check(
		APP_CONTROL_REPEAT_INTERVAL_TICKS >= 3,
		"navigation repeat interval stays human-readable"
	);
	check(
		app_controls_repeatable_navigation_held(APP_CONTROL_BUTTON_DOWN),
		"held navigation wait accepts single direction"
	);
	check(
		!app_controls_repeatable_navigation_held(
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT
		),
		"held navigation wait rejects diagonal"
	);
	check(
		!app_controls_repeatable_navigation_held(
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_A
		),
		"held navigation wait rejects command chord"
	);
	check(
		!app_controls_repeatable_navigation_held(APP_CONTROL_BUTTON_A),
		"held navigation wait rejects command-only hold"
	);
	check(
		app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_DECK_SELECT,
			APP_CONTROL_BUTTON_RIGHT
		),
		"deck select held wait accepts page direction"
	);
	check(
		app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_REVIEW,
			APP_CONTROL_BUTTON_UP
		),
		"review held wait accepts scroll direction"
	);
	check(
		!app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_REVIEW,
			APP_CONTROL_BUTTON_RIGHT
		),
		"review held wait rejects non-repeat axis"
	);
	check(
		app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_SETTINGS,
			APP_CONTROL_BUTTON_RIGHT
		),
		"settings held wait accepts value direction"
	);
	check(
		!app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_SETTINGS,
			APP_CONTROL_BUTTON_DOWN
		),
		"settings held wait rejects field direction"
	);
	check(
		!app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_SETTINGS,
			APP_CONTROL_BUTTON_RIGHT | APP_CONTROL_BUTTON_A
		),
		"settings held wait rejects command chord"
	);
	check(
		!app_controls_repeatable_navigation_held_for_mode(
			APP_CONTROL_MODE_LOAD_ERROR,
			APP_CONTROL_BUTTON_DOWN
		),
		"load error held wait rejects navigation"
	);

	app_controls_repeat_init(&repeat);
	repeated = app_controls_repeat_buttons(
		&repeat,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN
	);
	check(repeated == 0, "navigation repeat ignores initial edge");

	for (unsigned int tick = 1; tick < APP_CONTROL_REPEAT_INITIAL_TICKS; tick++)
	{
		repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
		check(repeated == 0, "navigation repeat waits before first repeat");
	}

	repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
	check(
		repeated == APP_CONTROL_BUTTON_DOWN,
		"navigation repeat emits first held direction"
	);

	for (unsigned int tick = 1; tick < APP_CONTROL_REPEAT_INTERVAL_TICKS; tick++)
	{
		repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
		check(repeated == 0, "navigation repeat waits between repeats");
	}

	repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
	check(
		repeated == APP_CONTROL_BUTTON_DOWN,
		"navigation repeat emits interval repeat"
	);

	repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_RIGHT);
	check(repeated == 0, "navigation repeat resets on direction change");

	app_controls_repeat_reset(&repeat);
	repeated = app_controls_repeat_buttons(
		&repeat,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN
	);
	check(repeated == 0, "navigation repeat re-arms from initial edge");
	for (unsigned int tick = 1; tick < APP_CONTROL_REPEAT_INITIAL_TICKS; tick++)
		app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
	repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
	check(repeated == APP_CONTROL_BUTTON_DOWN, "navigation repeat is active");
	repeated = app_controls_repeat_buttons(
		&repeat,
		APP_CONTROL_BUTTON_A,
		APP_CONTROL_BUTTON_A | APP_CONTROL_BUTTON_DOWN
	);
	check(repeated == 0, "navigation repeat rejects held command chords");
	repeated = app_controls_repeat_buttons(&repeat, 0, APP_CONTROL_BUTTON_DOWN);
	check(repeated == 0, "navigation repeat resets after command chord");

	app_controls_repeat_reset(&repeat);
	repeated = app_controls_repeat_buttons(
		&repeat,
		APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT,
		APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT
	);
	check(repeated == 0, "navigation repeat rejects initial diagonal hold");
	for (
		unsigned int tick = 0;
		tick < APP_CONTROL_REPEAT_INITIAL_TICKS + APP_CONTROL_REPEAT_INTERVAL_TICKS;
		tick++
	)
	{
		repeated = app_controls_repeat_buttons(
			&repeat,
			0,
			APP_CONTROL_BUTTON_DOWN | APP_CONTROL_BUTTON_RIGHT
		);
		check(repeated == 0, "navigation repeat rejects held diagonal hold");
	}

	app_controls_repeat_reset(&repeat);
	repeated = app_controls_repeat_buttons(&repeat, 0, 0);
	check(repeated == 0, "navigation repeat release stays quiet");

	app_controls_repeat_reset(&repeat);
	for (unsigned int tap = 0; tap < 3; tap++)
	{
		repeated = app_controls_repeat_buttons(
			&repeat,
			APP_CONTROL_BUTTON_DOWN,
			APP_CONTROL_BUTTON_DOWN
		);
		check(repeated == 0, "quick navigation tap does not repeat on press");
		repeated = app_controls_repeat_buttons(&repeat, 0, 0);
		check(repeated == 0, "quick navigation tap release clears repeat");
	}
	for (unsigned int tick = 0; tick < APP_CONTROL_REPEAT_INITIAL_TICKS; tick++)
	{
		repeated = app_controls_repeat_buttons(&repeat, 0, 0);
		check(repeated == 0, "released navigation cannot accumulate repeat ticks");
	}

	app_controls_repeat_init(&repeat);
	repeated = app_controls_repeat_buttons_for_mode(
		&repeat,
		APP_CONTROL_MODE_SETTINGS,
		APP_CONTROL_BUTTON_DOWN,
		APP_CONTROL_BUTTON_DOWN
	);
	check(repeated == 0, "settings field tap does not repeat on press");
	for (
		unsigned int tick = 0;
		tick < APP_CONTROL_REPEAT_INITIAL_TICKS + APP_CONTROL_REPEAT_INTERVAL_TICKS;
		tick++
	)
	{
		repeated = app_controls_repeat_buttons_for_mode(
			&repeat,
			APP_CONTROL_MODE_SETTINGS,
			0,
			APP_CONTROL_BUTTON_DOWN
		);
		check(repeated == 0, "settings field hold does not repeat");
	}

	app_controls_repeat_init(&repeat);
	repeated = app_controls_repeat_buttons_for_mode(
		&repeat,
		APP_CONTROL_MODE_SETTINGS,
		APP_CONTROL_BUTTON_RIGHT,
		APP_CONTROL_BUTTON_RIGHT
	);
	check(repeated == 0, "settings value tap does not repeat on press");
	for (unsigned int tick = 1; tick < APP_CONTROL_REPEAT_INITIAL_TICKS; tick++)
	{
		repeated = app_controls_repeat_buttons_for_mode(
			&repeat,
			APP_CONTROL_MODE_SETTINGS,
			0,
			APP_CONTROL_BUTTON_RIGHT
		);
		check(repeated == 0, "settings value hold waits before repeat");
	}
	repeated = app_controls_repeat_buttons_for_mode(
		&repeat,
		APP_CONTROL_MODE_SETTINGS,
		0,
		APP_CONTROL_BUTTON_RIGHT
	);
	check(
		repeated == APP_CONTROL_BUTTON_RIGHT,
		"settings value hold repeats after delay"
	);
}

static void test_app_controls_navigation_repeat_modes(void)
{
	unsigned int up_down_mask = APP_CONTROL_BUTTON_UP | APP_CONTROL_BUTTON_DOWN;
	unsigned int left_right_mask = APP_CONTROL_BUTTON_LEFT | APP_CONTROL_BUTTON_RIGHT;

	check(
		app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_DECK_SELECT),
		"deck select supports navigation repeat"
	);
	check(
		app_controls_navigation_repeat_mask(APP_CONTROL_MODE_DECK_SELECT) ==
			APP_CONTROL_BUTTON_NAVIGATION_MASK,
		"deck select repeats all navigation directions"
	);
	check(
		app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_REVIEW),
		"review supports navigation repeat"
	);
	check(
		app_controls_navigation_repeat_mask(APP_CONTROL_MODE_REVIEW) ==
			up_down_mask,
		"review repeats only text scroll directions"
	);
	check(
		app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_ACTIONS),
		"actions supports navigation repeat"
	);
	check(
		app_controls_navigation_repeat_mask(APP_CONTROL_MODE_ACTIONS) ==
			up_down_mask,
		"actions repeats only selection directions"
	);
	check(
		app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_SETTINGS),
		"settings supports navigation repeat"
	);
	check(
		app_controls_navigation_repeat_mask(APP_CONTROL_MODE_SETTINGS) ==
			left_right_mask,
		"settings repeats only value directions"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_LOAD_ERROR),
		"load error does not repeat navigation"
	);
	check(
		app_controls_navigation_repeat_mask(APP_CONTROL_MODE_LOAD_ERROR) == 0,
		"load error has no repeat mask"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_SUMMARY),
		"summary does not repeat navigation"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_CONTROLS),
		"controls screen does not repeat navigation"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_CONFIRM_RESTORE),
		"restore confirmation does not repeat navigation"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_CONFIRM_SUSPEND),
		"suspend confirmation does not repeat navigation"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_CONFIRM_RESET),
		"reset confirmation does not repeat navigation"
	);
	check(
		!app_controls_mode_uses_navigation_repeat(APP_CONTROL_MODE_CONFIRM_EXIT),
		"exit confirmation does not repeat navigation"
	);
}

static void test_app_controls_input_activity(void)
{
	check(
		app_controls_input_is_active(APP_CONTROL_BUTTON_DOWN, 0, 0),
		"input activity sees pressed button"
	);
	check(
		app_controls_input_is_active(0, APP_CONTROL_BUTTON_DOWN, 0),
		"input activity sees held button"
	);
	check(
		app_controls_input_is_active(0, 0, APP_CONTROL_BUTTON_DOWN),
		"input activity sees repeated button"
	);
	check(
		!app_controls_input_is_active(0, 0, 0),
		"input activity ignores idle loop"
	);
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

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.cards[0].interval_days == 1, "lapsed good starts one-day relearning interval");
	check(session.cards[0].due_day == TEST_TODAY + 1, "lapsed good schedules tomorrow");
	check(session.cards[0].lapses == 1, "lapsed good preserves lapse count");
	check(session.due_count == 0, "lapsed good clears due queue");
}

static void test_scheduler_caps_loaded_state_counters(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			0,
			SCHEDULER_MAX_REVIEW_COUNT,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			SCHEDULER_MAX_LAPSES,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"max counter card restores"
	);
	scheduler_reposition(&session);

	scheduler_rate_current(&session, SCHEDULER_RATING_AGAIN);
	check(
		session.cards[0].review_count == SCHEDULER_MAX_REVIEW_COUNT,
		"review count caps at maximum"
	);
	check(
		session.cards[0].lapses == SCHEDULER_MAX_LAPSES,
		"lapses cap at maximum"
	);
	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"max counter state saves"
	);

	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"max counter state reloads"
	);
	check(
		loaded.cards[0].review_count == SCHEDULER_MAX_REVIEW_COUNT,
		"max counter review count reloads"
	);
	check(
		loaded.cards[0].lapses == SCHEDULER_MAX_LAPSES,
		"max counter lapses reload"
	);

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
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

static void test_scheduler_limit_change_clears_undo(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 2, "limit undo setup rates card");

	scheduler_set_daily_limits(&session, 1, 0);
	check(session.due_count == 0, "limit change recalculates visible queue");
	check(!scheduler_undo_last(&session), "limit change clears stale undo");
	check(session.cards[0].review_count == 1, "cleared undo leaves rating intact");
	check(session.due_count == 0, "cleared undo leaves limited due count");
}

static void test_scheduler_day_change_resets_new_limit(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_set_daily_limits(&session, 1, 0);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(session.new_count_today == 1, "day change setup starts one new card");
	check(session.due_count == 0, "day change setup exhausts new limit");

	scheduler_set_today(&session, TEST_TODAY + 1);

	check(session.today == TEST_TODAY + 1, "day change updates scheduler date");
	check(session.new_count_today == 0, "day change resets new daily count");
	check(session.review_count_today == 0, "day change resets review daily count");
	check(session.due_count == 2, "day change exposes tomorrow review and one new card");
	check(scheduler_current_index(&session) == 0, "day change prioritizes due review");
	check(!scheduler_undo_last(&session), "day change clears stale undo");
	check(session.cards[0].review_count == 1, "cleared day-change undo leaves rating intact");
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

static void test_scheduler_review_limit_uses_due_priority(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
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
			TEST_TODAY - 40,
			TEST_TODAY - 10
		),
		"less-overdue review restores"
	);
	check(
		scheduler_restore_card(
			&session,
			1,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY - 7,
			10,
			2500,
			0,
			false,
			TEST_TODAY - 40,
			TEST_TODAY - 10
		),
		"most-overdue review restores"
	);
	check(
		scheduler_restore_card(
			&session,
			2,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY - 3,
			10,
			2500,
			0,
			false,
			TEST_TODAY - 40,
			TEST_TODAY - 10
		),
		"second-overdue review restores"
	);
	scheduler_set_daily_limits(&session, 0, 1);

	check(!scheduler_card_is_due(&session, 0), "review limit hides less-overdue card");
	check(scheduler_card_is_due(&session, 1), "review limit exposes oldest review");
	check(!scheduler_card_is_due(&session, 2), "review limit hides second-overdue card");
	check(session.due_count == 1, "review limit exposes one priority review");
	check(scheduler_current_index(&session) == 1, "oldest review wins under limit");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.review_count_today == 1, "priority review counts against limit");
	check(session.due_count == 0, "review limit is exhausted after priority review");
}

static void test_scheduler_restore_repositions_to_due_card(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			0,
			1,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY + 10,
			10,
			2500,
			0,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"future restored card"
	);

	check(session.due_count == 1, "restore leaves one due card");
	check(scheduler_current_index(&session) == 1, "restore repositions to due card");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.cards[0].review_count == 1, "future restored card is not rated");
	check(session.cards[1].review_count == 1, "current due card is rated");
}

static void test_scheduler_day_change_resets_review_limit(void)
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
			TEST_TODAY - 30,
			TEST_TODAY - 10
		),
		"day change first review card restores"
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
			TEST_TODAY - 30,
			TEST_TODAY - 10
		),
		"day change second review card restores"
	);
	scheduler_set_daily_limits(&session, 0, 1);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(session.review_count_today == 1, "day change setup starts one review");
	check(session.due_count == 0, "day change setup exhausts review limit");

	scheduler_set_today(&session, TEST_TODAY + 1);

	check(session.review_count_today == 0, "day change resets review limit count");
	check(session.due_count == 1, "day change exposes next limited review");
	check(scheduler_current_index(&session) == 1, "day change selects next review");
}

static void test_scheduler_prioritizes_learning_cards(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 3, TEST_TODAY);
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
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"priority review card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			2,
			5,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			1,
			false,
			TEST_TODAY - 20,
			TEST_TODAY
		),
		"priority learning card restores"
	);

	scheduler_reposition(&session);
	check(scheduler_current_index(&session) == 2, "learning card is selected first");
}

static void test_scheduler_learning_cards_bypass_review_limit(void)
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
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"limited normal review restores"
	);
	check(
		scheduler_restore_card(
			&session,
			1,
			5,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			1,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 1
		),
		"limited relearning card restores"
	);
	scheduler_set_daily_limits(&session, 0, 1);

	check(scheduler_card_is_due(&session, 0), "review limit exposes normal review");
	check(scheduler_card_is_due(&session, 1), "review limit keeps relearning card due");
	check(session.due_count == 2, "review limit includes relearning card");
	check(scheduler_current_index(&session) == 1, "relearning card wins priority");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.review_count_today == 1, "relearning card counts after review");
	check(!scheduler_card_is_due(&session, 0), "review limit hides normal review after relearning");
	check(session.due_count == 0, "review limit is exhausted after relearning review");
}

static void test_scheduler_counts_due_card_types(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 5, TEST_TODAY);
	check(
		scheduler_restore_card(
			&session,
			1,
			5,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			1,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 1
		),
		"due count learning card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			2,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			10,
			2500,
			0,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"due count review card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			3,
			5,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY + 5,
			10,
			2500,
			0,
			false,
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"due count future review card restores"
	);
	scheduler_set_daily_limits(&session, 1, 0);

	check(session.due_count == 3, "due category total honors limits");
	check(scheduler_new_due_count(&session) == 1, "due category counts limited new card");
	check(scheduler_learning_due_count(&session) == 1, "due category counts learning card");
	check(scheduler_review_due_count(&session) == 1, "due category counts review card");
	check(!scheduler_card_is_due(&session, 4), "second unstarted new card is hidden");
}

static void test_scheduler_prioritizes_overdue_reviews_before_new_cards(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 4, TEST_TODAY);
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
			TEST_TODAY - 30,
			TEST_TODAY - 10
		),
		"priority due review restores"
	);
	check(
		scheduler_restore_card(
			&session,
			2,
			8,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY - 20,
			20,
			2500,
			0,
			false,
			TEST_TODAY - 60,
			TEST_TODAY - 20
		),
		"priority overdue review restores"
	);
	scheduler_reposition(&session);

	check(scheduler_current_index(&session) == 2, "oldest overdue review is selected first");
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(scheduler_current_index(&session) == 1, "due review follows overdue review");
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(scheduler_current_index(&session) == 3, "new cards follow due reviews by rotation");
}

static void test_scheduler_restored_started_new_card_stays_due_after_limit(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_set_daily_limits(&session, 1, 0);
	check(
		scheduler_restore_card(
			&session,
			0,
			1,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			0,
			false,
			TEST_TODAY,
			TEST_TODAY
		),
		"restored started new card"
	);
	scheduler_reposition(&session);

	check(session.new_count_today == 1, "restored new card counts against new limit");
	check(session.review_count_today == 0, "restored new card does not count as review");
	check(session.due_count == 1, "restored started new card remains visible");
	check(scheduler_current_index(&session) == 0, "restored started new card selected");
	check(scheduler_card_is_due(&session, 0), "started new card is due");
	check(!scheduler_card_is_due(&session, 1), "unstarted new card remains hidden");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 0, "finished restored new card leaves limit full");
	check(!scheduler_card_is_due(&session, 1), "new limit still hides next new card");
}

static void test_scheduler_restored_started_review_card_stays_due_after_limit(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_set_daily_limits(&session, 0, 1);
	check(
		scheduler_restore_card(
			&session,
			0,
			5,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			1,
			false,
			TEST_TODAY - 20,
			TEST_TODAY
		),
		"restored started review card"
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
			TEST_TODAY - 20,
			TEST_TODAY - 10
		),
		"restored unstarted review card"
	);
	scheduler_reposition(&session);

	check(session.new_count_today == 0, "restored review card does not count as new");
	check(session.review_count_today == 1, "restored review card counts against review limit");
	check(session.due_count == 1, "restored started review card remains visible");
	check(scheduler_current_index(&session) == 0, "restored started review card selected");
	check(scheduler_card_is_due(&session, 0), "started review card is due");
	check(!scheduler_card_is_due(&session, 1), "unstarted review card remains hidden");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.due_count == 0, "finished restored review card leaves limit full");
	check(!scheduler_card_is_due(&session, 1), "review limit still hides next review card");
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

static void build_review_log_entry(struct review_log_entry *entry)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);

	memset(entry, 0, sizeof(*entry));
	entry->timestamp = 12345;
	entry->day = TEST_TODAY;
	entry->event = REVIEW_LOG_EVENT_RATING;
	entry->card_id = "card-1";
	entry->rating = SCHEDULER_RATING_GOOD;
	entry->before = session.cards[0];
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	entry->after = session.cards[0];
}

static void append_review_log_transition(
	const char *path,
	time_t timestamp,
	unsigned int day,
	enum review_log_event event,
	const char *card_id,
	enum scheduler_rating rating,
	const struct scheduler_card *before,
	const struct scheduler_card *after,
	const char *message
)
{
	struct review_log_entry entry;

	memset(&entry, 0, sizeof(entry));
	entry.timestamp = timestamp;
	entry.day = day;
	entry.event = event;
	entry.card_id = card_id;
	entry.rating = rating;
	entry.before = *before;
	entry.after = *after;

	check(review_log_append(path, &entry), message);
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

static void test_review_state_load_rejects_null_arguments(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);

	check(
		review_state_load(NULL, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"state load rejects null deck"
	);
	check(
		session.due_count == deck.card_count,
		"null deck state load leaves scheduler unchanged"
	);
	check(
		review_state_load(&deck, NULL, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"state load rejects null session"
	);
	check(
		review_state_load(&deck, &session, NULL) == REVIEW_STATE_LOAD_BAD_FORMAT,
		"state load rejects null path"
	);
	check(
		session.due_count == deck.card_count,
		"null path state load leaves scheduler unchanged"
	);
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

static void test_review_state_round_trip_review_limit_count(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
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
			TEST_TODAY - 30,
			TEST_TODAY - 10
		),
		"review limit first mature card restores"
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
			TEST_TODAY - 30,
			TEST_TODAY - 10
		),
		"review limit second mature card restores"
	);
	scheduler_set_daily_limits(&session, 0, 1);
	check(session.due_count == 1, "review limit exposes one mature card");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(session.review_count_today == 1, "review limit counts rated mature card");
	check(session.due_count == 0, "review limit hides second mature card");
	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"review limit state saves"
	);

	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	scheduler_set_daily_limits(&loaded, 0, 1);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"review limit state reloads"
	);
	check(
		loaded.review_count_today == 1,
		"review limit count reloads from last review day"
	);
	check(loaded.due_count == 0, "review limit remains exhausted after reload");
	check(
		!scheduler_card_is_due(&loaded, 1),
		"unstarted mature card stays hidden after reload"
	);
	check(
		scheduler_reviewed_today_count(&loaded) == 1,
		"review limit reviewed-today summary reloads"
	);

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_save_rejects_count_mismatch(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, 1, TEST_TODAY);

	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_SAVE_FAILED,
		"state save rejects deck/session count mismatch"
	);
	check(access(TEST_STATE_PATH, F_OK) != 0, "mismatch save leaves no primary");
	check(access(TEST_STATE_TEMP_PATH, F_OK) != 0, "mismatch save leaves no temp");
	check(access(TEST_STATE_BACKUP_PATH, F_OK) != 0, "mismatch save leaves no backup");
}

static void test_review_state_save_retains_backup_for_primary_recovery(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;
	const char *old_state =
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n";

	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	write_file(TEST_STATE_PATH, old_state);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);

	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"state save with previous primary succeeds"
	);
	check(file_equals(TEST_STATE_BACKUP_PATH, old_state), "state save retains backup");

	write_file(TEST_STATE_PATH, "bad\n");
	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"retained backup recovers bad primary state"
	);
	check(loaded.cards[0].review_count == 1, "retained backup review count loads");
	check(loaded.cards[0].interval_days == 1, "retained backup interval loads");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
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
	check(access(TEST_STATE_BACKUP_PATH, F_OK) != 0, "backup state promotes backup");
	check(file_equals(
		TEST_STATE_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	), "backup state promotes primary");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_loads_backup_when_primary_is_bad(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "bad\n");
	write_file(
		TEST_STATE_BACKUP_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"backup state loads when primary is bad"
	);
	check(session.cards[0].review_count == 1, "backup after bad primary restores card");
	check(!scheduler_card_is_due(&session, 0), "backup after bad primary restores due day");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_loads_temp_when_primary_and_backup_missing(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_TEMP_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"temp state loads when primary and backup are missing"
	);
	check(session.cards[0].review_count == 1, "temp state card loads");
	check(!scheduler_card_is_due(&session, 0), "temp state due day loads");
	check(access(TEST_STATE_TEMP_PATH, F_OK) != 0, "temp state promotes temp");
	check(file_equals(
		TEST_STATE_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	), "temp state promotes primary");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
}

static void test_review_state_bad_temp_falls_back_to_backup(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_TEMP_PATH, "bad\n");
	write_file(
		TEST_STATE_BACKUP_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"backup state loads when temp is bad"
	);
	check(session.cards[0].review_count == 1, "backup after bad temp restores card");
	check(!scheduler_card_is_due(&session, 0), "backup after bad temp restores due day");

	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_bad_primary_prefers_backup_before_temp(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "bad\n");
	write_file(
		TEST_STATE_BACKUP_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);
	write_file(
		TEST_STATE_TEMP_PATH,
		"card-1\t2\t3\t20004\t4\t2650\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"backup state loads before temp when primary is bad"
	);
	check(session.cards[0].review_count == 1, "bad primary backup review count wins");
	check(session.cards[0].interval_days == 1, "bad primary backup interval wins");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
}

static void test_review_state_empty_file_is_bad_format(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"empty state file is bad format"
	);
	check(session.due_count == deck.card_count, "empty state file leaves session unchanged");

	remove(TEST_STATE_PATH);
}

static void test_review_state_unknown_only_file_is_unmatched(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_PATH,
		"missing-card\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_UNMATCHED,
		"unknown-only state file is unmatched"
	);
	check(session.due_count == deck.card_count, "unknown-only state leaves session unchanged");

	remove(TEST_STATE_PATH);
}

static void test_review_state_duplicate_card_row_is_bad_format(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct review_state_load_report report;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_PATH,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
		"card-1\t2\t3\t20002\t4\t2600\t0\t0\t100\t20000\n"
	);

	check(
		review_state_load_with_report(
			&deck,
			&session,
			TEST_STATE_PATH,
			&report
		) == REVIEW_STATE_LOAD_BAD_FORMAT,
		"duplicate state card row is bad format"
	);
	check(report.line_number == 2, "duplicate state card row reports line");
	check(
		report.parse_result == REVIEW_STATE_PARSE_DUPLICATE_CARD,
		"duplicate state card row reports reason"
	);
	check(session.cards[0].review_count == 0, "duplicate state leaves session unchanged");
	check(session.due_count == deck.card_count, "duplicate state leaves due count unchanged");

	remove(TEST_STATE_PATH);
}

static void test_review_state_marked_file_requires_complete_footer(void)
{
	struct deck deck;
	struct scheduler_session session;

	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_PATH,
		"#anki3ds-state-v1\t2\n"
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"marked state without footer is bad format"
	);
	check(session.cards[0].review_count == 0, "missing footer leaves card unchanged");
	check(session.due_count == deck.card_count, "missing footer leaves queue unchanged");

	write_file(
		TEST_STATE_PATH,
		"#anki3ds-state-v1\t2\n"
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t100\t19999\n"
		"#anki3ds-state-complete\t2\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"marked state with wrong row count is bad format"
	);
	check(session.cards[0].review_count == 0, "wrong row count leaves card unchanged");
	check(session.due_count == deck.card_count, "wrong row count leaves queue unchanged");

	remove(TEST_STATE_PATH);
}

static void test_review_state_rejects_inconsistent_current_rows(void)
{
	struct deck deck;
	struct scheduler_session session;

	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(
		TEST_STATE_PATH,
		"#anki3ds-state-v1\t1\n"
		"card-1\t0\t2\t20000\t0\t2500\t0\t0\t20000\t20000\n"
		"#anki3ds-state-complete\t1\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"unreviewed current row with review days is bad format"
	);
	check(session.cards[0].review_count == 0, "inconsistent row leaves card unchanged");
	check(session.due_count == deck.card_count, "inconsistent row leaves queue unchanged");

	write_file(
		TEST_STATE_PATH,
		"#anki3ds-state-v1\t1\n"
		"card-1\t1\t2\t20000\t1\t2500\t2\t0\t19999\t20000\n"
		"#anki3ds-state-complete\t1\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"current row with too many lapses is bad format"
	);

	write_file(
		TEST_STATE_PATH,
		"#anki3ds-state-v1\t1\n"
		"card-1\t1\t2\t20000\t1\t2500\t0\t0\t20000\t19999\n"
		"#anki3ds-state-complete\t1\n"
	);

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) ==
			REVIEW_STATE_LOAD_BAD_FORMAT,
		"current row with reversed review days is bad format"
	);

	remove(TEST_STATE_PATH);
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

static void test_review_state_migrated_review_round_trips_after_rating(void)
{
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session loaded;

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
	build_test_deck(&deck);
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	write_file(TEST_STATE_PATH, "card-1\t1\t2\t20000\t1\t2500\t0\n");

	check(
		review_state_load(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"previous format due review loads"
	);
	scheduler_set_daily_limits(&session, 1, 1);
	check(session.cards[0].first_review_day == 0, "migrated review starts unknown first day");
	check(session.cards[0].last_review_day == 0, "migrated review starts unknown last day");
	check(scheduler_current_index(&session) == 0, "migrated due review selected");

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(
		session.cards[0].first_review_day == 0,
		"migrated review keeps unknown first day"
	);
	check(
		session.cards[0].last_review_day == TEST_TODAY,
		"migrated review rating fills last day"
	);
	check(session.new_count_today == 0, "migrated review does not count as new");
	check(session.review_count_today == 1, "migrated review counts as review");
	check(
		review_state_save(&deck, &session, TEST_STATE_PATH) == REVIEW_STATE_SAVE_OK,
		"migrated review saves after rating"
	);

	scheduler_init(&loaded, deck.card_count, TEST_TODAY);
	scheduler_set_daily_limits(&loaded, 1, 1);
	check(
		review_state_load(&deck, &loaded, TEST_STATE_PATH) == REVIEW_STATE_LOAD_OK,
		"migrated review reloads after save"
	);
	check(loaded.cards[0].first_review_day == 0, "migrated first day remains unknown");
	check(loaded.cards[0].last_review_day == TEST_TODAY, "migrated last day reloads");
	check(loaded.new_count_today == 0, "migrated reload keeps new count clear");
	check(loaded.review_count_today == 1, "migrated reload restores review count");

	remove(TEST_STATE_PATH);
	remove(TEST_STATE_TEMP_PATH);
	remove(TEST_STATE_BACKUP_PATH);
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

static void test_storage_replace_file_commits_temp_file(void)
{
	write_file(TEST_STORAGE_PATH, "old\n");
	write_file(TEST_STORAGE_TEMP_PATH, "new\n");
	write_file(TEST_STORAGE_BACKUP_PATH, "stale backup\n");

	check(storage_replace_file(TEST_STORAGE_PATH), "storage replace succeeds");
	check(file_equals(TEST_STORAGE_PATH, "new\n"), "storage replace commits temp");
	check(access(TEST_STORAGE_TEMP_PATH, F_OK) != 0, "storage replace removes temp");
	check(file_equals(TEST_STORAGE_BACKUP_PATH, "old\n"), "storage replace retains backup");

	remove(TEST_STORAGE_PATH);
	remove(TEST_STORAGE_BACKUP_PATH);
}

static void test_storage_replace_file_commits_first_save(void)
{
	remove(TEST_STORAGE_PATH);
	remove(TEST_STORAGE_BACKUP_PATH);
	write_file(TEST_STORAGE_TEMP_PATH, "new\n");

	check(storage_replace_file(TEST_STORAGE_PATH), "storage first save succeeds");
	check(file_equals(TEST_STORAGE_PATH, "new\n"), "storage first save commits temp");
	check(access(TEST_STORAGE_TEMP_PATH, F_OK) != 0, "storage first save removes temp");
	check(access(TEST_STORAGE_BACKUP_PATH, F_OK) != 0, "storage first save has no backup");

	remove(TEST_STORAGE_PATH);
}

static void test_storage_replace_file_preserves_backup_without_primary(void)
{
	remove(TEST_STORAGE_PATH);
	write_file(TEST_STORAGE_TEMP_PATH, "new\n");
	write_file(TEST_STORAGE_BACKUP_PATH, "backup\n");

	check(
		storage_replace_file(TEST_STORAGE_PATH),
		"storage missing primary replace succeeds"
	);
	check(file_equals(TEST_STORAGE_PATH, "new\n"), "storage missing primary commits temp");
	check(
		file_equals(TEST_STORAGE_BACKUP_PATH, "backup\n"),
		"storage missing primary preserves backup"
	);
	check(access(TEST_STORAGE_TEMP_PATH, F_OK) != 0, "storage missing primary removes temp");

	remove(TEST_STORAGE_PATH);
	remove(TEST_STORAGE_BACKUP_PATH);
}

static void test_storage_delete_save_files_removes_related_files(void)
{
	write_file(TEST_STORAGE_PATH, "primary\n");
	write_file(TEST_STORAGE_TEMP_PATH, "temp\n");
	write_file(TEST_STORAGE_BACKUP_PATH, "backup\n");

	check(storage_delete_save_files(TEST_STORAGE_PATH), "storage delete succeeds");
	check(access(TEST_STORAGE_PATH, F_OK) != 0, "storage delete removes primary");
	check(access(TEST_STORAGE_TEMP_PATH, F_OK) != 0, "storage delete removes temp");
	check(access(TEST_STORAGE_BACKUP_PATH, F_OK) != 0, "storage delete removes backup");
}

static void test_storage_delete_save_files_removes_orphaned_artifacts(void)
{
	remove(TEST_STORAGE_PATH);
	write_file(TEST_STORAGE_TEMP_PATH, "temp\n");
	write_file(TEST_STORAGE_BACKUP_PATH, "backup\n");

	check(
		storage_delete_save_files(TEST_STORAGE_PATH),
		"storage delete orphaned artifacts succeeds"
	);
	check(access(TEST_STORAGE_PATH, F_OK) != 0, "storage orphan delete leaves no primary");
	check(access(TEST_STORAGE_TEMP_PATH, F_OK) != 0, "storage orphan delete removes temp");
	check(access(TEST_STORAGE_BACKUP_PATH, F_OK) != 0, "storage orphan delete removes backup");
}

static void test_review_log_appends_study_events(void)
{
	struct scheduler_session session;
	struct review_log_entry entry;
	struct scheduler_card initial_card;
	struct scheduler_card rated_card;

	remove(TEST_REVIEW_LOG_PATH);
	scheduler_init(&session, 1, TEST_TODAY);
	initial_card = session.cards[0];
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	rated_card = session.cards[0];

	memset(&entry, 0, sizeof(entry));
	entry.timestamp = 12345;
	entry.day = TEST_TODAY;
	entry.event = REVIEW_LOG_EVENT_RATING;
	entry.card_id = "card-1";
	entry.rating = SCHEDULER_RATING_GOOD;
	entry.before = initial_card;
	entry.after = rated_card;

	check(review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log appends rating");
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
		),
		"review log writes rating transition"
	);

	entry.timestamp = 12346;
	entry.event = REVIEW_LOG_EVENT_UNDO;
	entry.rating = SCHEDULER_RATING_COUNT;
	entry.before = rated_card;
	entry.after = initial_card;

	check(review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log appends undo");
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
			"12346\t20000\tundo\tcard-1\t-\t1\t20001\t1\t2500\t0\t0\t"
			"0\t20000\t0\t2500\t0\t0\n"
		),
		"review log preserves appended transitions"
	);

	entry.timestamp = 12347;
	entry.event = REVIEW_LOG_EVENT_RESTORE;
	entry.card_id = "card-1";
	entry.before = initial_card;
	entry.before.suspended = true;
	entry.after = initial_card;

	check(review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log appends restore");
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
			"12346\t20000\tundo\tcard-1\t-\t1\t20001\t1\t2500\t0\t0\t"
			"0\t20000\t0\t2500\t0\t0\n"
			"12347\t20000\trestore\tcard-1\t-\t0\t20000\t0\t2500\t0\t1\t"
			"0\t20000\t0\t2500\t0\t0\n"
		),
		"review log writes restore transition"
	);
	check(!review_log_append(NULL, &entry), "review log rejects null path");
	check(!review_log_append(TEST_REVIEW_LOG_PATH, NULL), "review log rejects null entry");
	entry.card_id = "bad\tid";
	check(!review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log rejects tab id");

	remove(TEST_REVIEW_LOG_PATH);
}

static void test_review_log_delete_removes_log_file(void)
{
	write_file(TEST_REVIEW_LOG_PATH, "study history\n");
	write_file(TEST_REVIEW_LOG_TEMP_PATH, "temp\n");
	write_file(TEST_REVIEW_LOG_BACKUP_PATH, "backup\n");

	check(review_log_delete(TEST_REVIEW_LOG_PATH), "review log delete succeeds");
	check(access(TEST_REVIEW_LOG_PATH, F_OK) != 0, "review log delete removes file");
	check(access(TEST_REVIEW_LOG_TEMP_PATH, F_OK) != 0, "review log delete removes temp");
	check(access(TEST_REVIEW_LOG_BACKUP_PATH, F_OK) != 0, "review log delete removes backup");
	check(review_log_delete(TEST_REVIEW_LOG_PATH), "review log delete accepts missing file");
	check(!review_log_delete(NULL), "review log delete rejects null path");
}

static void test_review_log_rejects_full_log(void)
{
	struct review_log_entry entry;

	remove(TEST_REVIEW_LOG_PATH);
	build_review_log_entry(&entry);

	write_newline_terminated_filler_file(
		TEST_REVIEW_LOG_PATH,
		(size_t)REVIEW_LOG_MAX_BYTES
	);

	check(!review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log rejects full file");
	check(
		file_size(TEST_REVIEW_LOG_PATH) == REVIEW_LOG_MAX_BYTES,
		"review log leaves full file unchanged"
	);

	remove(TEST_REVIEW_LOG_PATH);
}

static void test_review_log_repairs_partial_final_row(void)
{
	struct review_log_entry entry;

	remove(TEST_REVIEW_LOG_PATH);
	remove(TEST_REVIEW_LOG_TEMP_PATH);
	remove(TEST_REVIEW_LOG_BACKUP_PATH);
	build_review_log_entry(&entry);
	write_file(TEST_REVIEW_LOG_PATH, "complete row\npartial row without newline");

	check(
		review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log repairs partial final row"
	);
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"complete row\n"
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
		),
		"review log preserves complete rows before repaired append"
	);
	check(access(TEST_REVIEW_LOG_TEMP_PATH, F_OK) != 0, "review log repair removes temp");
	check(access(TEST_REVIEW_LOG_BACKUP_PATH, F_OK) != 0, "review log repair removes backup");

	remove(TEST_REVIEW_LOG_PATH);
	write_file(TEST_REVIEW_LOG_PATH, "partial row without newline");
	check(
		review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log repairs fully partial file"
	);
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
		),
		"review log replaces fully partial file with appended row"
	);

	remove(TEST_REVIEW_LOG_PATH);
}

static void test_review_log_recovers_pending_repair_before_append(void)
{
	struct review_log_entry entry;

	remove(TEST_REVIEW_LOG_PATH);
	remove(TEST_REVIEW_LOG_TEMP_PATH);
	remove(TEST_REVIEW_LOG_BACKUP_PATH);
	build_review_log_entry(&entry);
	write_file(TEST_REVIEW_LOG_TEMP_PATH, "complete row\n");
	write_file(TEST_REVIEW_LOG_BACKUP_PATH, "complete row\npartial row");

	check(
		review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log recovers pending repair before append"
	);
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"complete row\n"
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
		),
		"review log keeps recovered complete prefix"
	);
	check(access(TEST_REVIEW_LOG_TEMP_PATH, F_OK) != 0, "pending log repair removes temp");
	check(
		access(TEST_REVIEW_LOG_BACKUP_PATH, F_OK) != 0,
		"pending log repair removes backup"
	);

	remove(TEST_REVIEW_LOG_PATH);
	remove(TEST_REVIEW_LOG_TEMP_PATH);
	remove(TEST_REVIEW_LOG_BACKUP_PATH);
	write_file(TEST_REVIEW_LOG_PATH, "existing row\n");
	write_file(TEST_REVIEW_LOG_TEMP_PATH, "stale temp\n");
	write_file(TEST_REVIEW_LOG_BACKUP_PATH, "stale backup\n");
	check(
		review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log cleans stale repair files before append"
	);
	check(
		file_equals(
			TEST_REVIEW_LOG_PATH,
			"existing row\n"
			"12345\t20000\trating\tcard-1\tgood\t0\t20000\t0\t2500\t0\t0\t"
			"1\t20001\t1\t2500\t0\t0\n"
		),
		"review log appends after stale repair cleanup"
	);
	check(access(TEST_REVIEW_LOG_TEMP_PATH, F_OK) != 0, "stale log temp removed");
	check(access(TEST_REVIEW_LOG_BACKUP_PATH, F_OK) != 0, "stale log backup removed");

	remove(TEST_REVIEW_LOG_PATH);
}

static void test_review_log_appends_at_capacity_boundary(void)
{
	struct review_log_entry entry;
	long row_size;

	remove(TEST_REVIEW_LOG_PATH);
	build_review_log_entry(&entry);

	check(review_log_append(TEST_REVIEW_LOG_PATH, &entry), "review log sample row writes");
	row_size = file_size(TEST_REVIEW_LOG_PATH);
	check(
		row_size > 0 && row_size <= REVIEW_LOG_MAX_BYTES,
		"review log sample row has bounded size"
	);
	if (row_size <= 0 || row_size > REVIEW_LOG_MAX_BYTES)
	{
		remove(TEST_REVIEW_LOG_PATH);
		return;
	}

	remove(TEST_REVIEW_LOG_PATH);
	write_newline_terminated_filler_file(
		TEST_REVIEW_LOG_PATH,
		(size_t)(REVIEW_LOG_MAX_BYTES - row_size)
	);

	check(
		review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log appends row that exactly fills cap"
	);
	check(
		file_size(TEST_REVIEW_LOG_PATH) == REVIEW_LOG_MAX_BYTES,
		"review log reaches exact cap"
	);
	check(
		!review_log_append(TEST_REVIEW_LOG_PATH, &entry),
		"review log rejects row after exact cap"
	);
	check(
		file_size(TEST_REVIEW_LOG_PATH) == REVIEW_LOG_MAX_BYTES,
		"review log exact cap failure leaves file unchanged"
	);

	remove(TEST_REVIEW_LOG_PATH);
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

static void test_review_state_save_policy_rejects_bad_load(void)
{
	check(
		review_state_load_result_allows_save(REVIEW_STATE_LOAD_OK),
		"state save policy accepts loaded state"
	);
	check(
		review_state_load_result_allows_save(REVIEW_STATE_LOAD_NOT_FOUND),
		"state save policy accepts missing state"
	);
	check(
		review_state_load_result_allows_save(REVIEW_STATE_LOAD_UNMATCHED),
		"state save policy accepts unmatched state"
	);
	check(
		!review_state_load_result_allows_save(REVIEW_STATE_LOAD_BAD_FORMAT),
		"state save policy rejects bad state"
	);
}

static void test_app_review_queue_requires_safe_state(void)
{
	struct scheduler_session session;

	scheduler_init(&session, 1, TEST_TODAY);
	check(
		app_review_should_show_queue(REVIEW_STATE_LOAD_OK, &session),
		"review queue shows for safe due state"
	);
	check(
		app_review_should_show_queue(REVIEW_STATE_LOAD_NOT_FOUND, &session),
		"review queue shows for first-save state"
	);
	check(
		app_review_should_show_queue(REVIEW_STATE_LOAD_UNMATCHED, &session),
		"review queue shows for unmatched state"
	);
	check(
		!app_review_should_show_queue(REVIEW_STATE_LOAD_BAD_FORMAT, &session),
		"review queue hides for bad state"
	);
	check(
		!app_review_should_show_queue(REVIEW_STATE_LOAD_OK, NULL),
		"review queue rejects null session"
	);

	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	check(
		!app_review_should_show_queue(REVIEW_STATE_LOAD_OK, &session),
		"review queue hides when no cards are due"
	);
}

static void test_app_review_formats_rating_status(void)
{
	char message[64];
	char tiny[8];

	app_review_format_rating_status(
		message,
		sizeof(message),
		"Good",
		true,
		false,
		false,
		1,
		3
	);
	check(strcmp(message, "Good saved; card 2/3") == 0, "rating status shows next card");

	app_review_format_rating_status(
		message,
		sizeof(message),
		"Again",
		true,
		false,
		true,
		0,
		1
	);
	check(
		strcmp(message, "Again saved; same card due") == 0,
		"rating status shows same due card"
	);

	app_review_format_rating_status(
		message,
		sizeof(message),
		"Easy",
		true,
		true,
		false,
		2,
		3
	);
	check(
		strcmp(message, "Easy saved; no cards due") == 0,
		"rating status shows complete queue"
	);

	app_review_format_rating_status(
		message,
		sizeof(message),
		"Hard",
		false,
		false,
		false,
		2,
		4
	);
	check(
		strcmp(message, "Hard saved; card 3/4; log skipped") == 0,
		"rating status preserves next card when log skips"
	);

	app_review_format_rating_status(
		tiny,
		sizeof(tiny),
		"Good",
		true,
		false,
		false,
		0,
		1
	);
	check(tiny[sizeof(tiny) - 1] == '\0', "rating status truncates safely");
	app_review_format_rating_status(NULL, 0, "Good", true, false, false, 0, 1);
}

static void test_app_review_formats_day_change_status(void)
{
	char message[64];
	char tiny[8];
	struct scheduler_session session;

	scheduler_init(&session, 2, TEST_TODAY);
	app_review_format_day_change_status(
		message,
		sizeof(message),
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(
		strcmp(message, "New day; cards due") == 0,
		"day-change status shows active due queue"
	);

	scheduler_set_daily_limits(&session, 1, 200);
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	app_review_format_day_change_status(
		message,
		sizeof(message),
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(
		strcmp(message, "New day; daily limit reached") == 0,
		"day-change status distinguishes limit-blocked queue"
	);

	scheduler_set_daily_limits(&session, 0, 0);
	while (!scheduler_is_complete(&session))
		scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	app_review_format_day_change_status(
		message,
		sizeof(message),
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(
		strcmp(message, "New day; no cards due") == 0,
		"day-change status shows empty due queue"
	);

	app_review_format_day_change_status(
		message,
		sizeof(message),
		REVIEW_STATE_LOAD_BAD_FORMAT,
		&session
	);
	check(
		strcmp(message, "Reset bad state first") == 0,
		"day-change status preserves bad-state recovery"
	);

	app_review_format_day_change_status(
		tiny,
		sizeof(tiny),
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(tiny[sizeof(tiny) - 1] == '\0', "day-change status truncates safely");
	app_review_format_day_change_status(NULL, 0, REVIEW_STATE_LOAD_OK, &session);
}

static void test_app_status_classifies_daily_use_feedback(void)
{
	char visible[64];

	check(
		app_status_message_color("Loaded deck") == APP_STATUS_COLOR_SUCCESS,
		"status loaded deck is success"
	);
	check(
		app_status_message_color("Restored 2 suspended") == APP_STATUS_COLOR_SUCCESS,
		"status restore is success"
	);
	check(
		app_status_message_color("New day; no cards due") == APP_STATUS_COLOR_SUCCESS,
		"status no due is success"
	);
	check(
		app_status_message_color("New day; cards due") == APP_STATUS_COLOR_SUCCESS,
		"status cards due is success"
	);
	check(
		app_status_message_color("New day; cards due; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status new day settings ignored is warning"
	);
	check(
		app_status_message_color("New day; no cards due; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status new day state unmatched is warning"
	);
	check(
		app_status_message_color("New day; daily limit reached; settings/state") ==
			APP_STATUS_COLOR_WARNING,
		"status new day limit settings/state is warning"
	);
	check(
		app_status_message_color("Reset bad state first; reset state/settings") ==
			APP_STATUS_COLOR_DANGER,
		"status new day reset state/settings is danger"
	);
	check(
		app_status_message_color("Missing deck") == APP_STATUS_COLOR_DANGER,
		"status missing deck is danger"
	);
	check(
		app_status_message_color("Save failed; card not advanced") ==
			APP_STATUS_COLOR_DANGER,
		"status failed save is danger"
	);
	check(
		app_status_message_color("Unsaved limit edits") == APP_STATUS_COLOR_WARNING,
		"status unsaved limit is warning"
	);
	check(
		app_status_message_color("Nothing to undo; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status nothing undo limit reached is warning"
	);
	check(
		app_status_message_color("Nothing to suspend; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status nothing suspend reset state/settings is warning"
	);
	check(
		app_status_message_color("Nothing suspended; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status nothing suspended settings ignored is warning"
	);
	check(
		app_status_message_color("Undo saved; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status undo saved settings ignored is warning"
	);
	check(
		app_status_message_color("Undo saved; log skipped; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status undo saved log skipped limit reached is warning"
	);
	check(
		app_status_message_color("Good saved; card 2/3; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status rating saved settings ignored is warning"
	);
	check(
		app_status_message_color("Easy saved; no cards due; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status rating saved state unmatched is warning"
	);
	check(
		app_status_message_color("Suspend saved; settings/state") ==
			APP_STATUS_COLOR_WARNING,
		"status suspend saved settings/state is warning"
	);
	check(
		app_status_message_color("Suspend saved; no cards due; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status suspend saved no due settings ignored is warning"
	);
	check(
		app_status_message_color("Restored 1 suspended; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status restore saved state unmatched is warning"
	);
	check(
		app_status_message_color("Progress reset; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status reset settings ignored is warning"
	);
	check(
		app_status_message_color("Progress reset; log kept; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status reset log kept limit reached is warning"
	);
	check(
		app_status_message_color("Limits saved; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status limits saved state unmatched is warning"
	);
	check(
		app_status_message_color("Limits saved; no cards due; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status limits saved no due state unmatched is warning"
	);
	check(
		app_status_message_color("Limits saved; daily limit reached; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status limits saved daily limit state unmatched is warning"
	);
	check(
		app_status_message_color("Answer shown; choose rating; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status answer shown settings ignored is warning"
	);
	check(
		app_status_message_color("Text 2/3; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status text scroll state unmatched is warning"
	);
	check(
		app_status_message_color("Text bottom; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status text bottom limit reached is warning"
	);
	check(
		app_status_message_color("No decks found; 2 ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status no decks found is warning"
	);
	check(
		app_status_message_color("Scan: 2 decks; 1 ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status ignored decks is warning"
	);
	check(
		app_status_message_color("Exit loses unsaved limits") ==
			APP_STATUS_COLOR_WARNING,
		"status exit losing limits is warning"
	);
	check(
		app_status_message_color("Limits saved; daily limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status limit reached is warning"
	);
	check(
		app_status_message_color("Deck 2/3; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status deck limit reached is warning"
	);
	check(
		app_status_message_color("Deck 2/3; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status deck settings ignored is warning"
	);
	check(
		app_status_message_color("Deck 2/3; settings/state") ==
			APP_STATUS_COLOR_WARNING,
		"status deck settings/state is warning"
	);
	check(
		app_status_message_color("Deck 2/3; state unmatched") ==
			APP_STATUS_COLOR_WARNING,
		"status deck state unmatched is warning"
	);
	check(
		app_status_message_color("Deck 2/3; load error") ==
			APP_STATUS_COLOR_DANGER,
		"status deck load error is danger"
	);
	check(
		app_status_message_color("Controls; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status controls limit reached is warning"
	);
	check(
		app_status_message_color("Controls; load error") ==
			APP_STATUS_COLOR_DANGER,
		"status controls load error is danger"
	);
	check(
		app_status_message_color("Controls; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status controls reset state/settings is warning"
	);
	check(
		app_status_message_color("Controls; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status controls settings ignored is warning"
	);
	check(
		app_status_message_color("Controls closed; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status controls closed limit reached is warning"
	);
	check(
		app_status_message_color("Controls closed; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status controls closed reset state/settings is warning"
	);
	check(
		app_status_message_color("Actions; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status actions reset state/settings is warning"
	);
	check(
		app_status_message_color("Actions; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status actions limit reached is warning"
	);
	check(
		app_status_message_color("Action: Daily limits; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status action selection limit reached is warning"
	);
	check(
		app_status_message_color("Action: Reset deck progress") ==
			APP_STATUS_COLOR_WARNING,
		"status reset action selection is warning"
	);
	check(
		app_status_message_color("Action: Reset deck progress; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status reset action selection reset state/settings is warning"
	);
	check(
		app_status_message_color("Editing limits; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status editing limits limit reached is warning"
	);
	check(
		app_status_message_color("Editing limits; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status editing limits reset state/settings is warning"
	);
	check(
		app_status_message_color("Editing review_limit; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status editing field limit reached is warning"
	);
	check(
		app_status_message_color("new_limit: 20; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status setting value settings ignored is warning"
	);
	check(
		app_status_message_color("Limits canceled; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status limits cancel limit reached is warning"
	);
	check(
		app_status_message_color("Limits canceled; discarded; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status limits cancel discarded reset state/settings is warning"
	);
	check(
		app_status_message_color("Restore requires X; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status restore requires limit reached is warning"
	);
	check(
		app_status_message_color("Reset requires X; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status reset requires reset state/settings is warning"
	);
	check(
		app_status_message_color("Exit requires A; settings ignored") ==
			APP_STATUS_COLOR_WARNING,
		"status exit requires settings ignored is warning"
	);
	check(
		app_status_message_color("Actions canceled") == APP_STATUS_COLOR_WARNING,
		"status cancel is warning"
	);
	check(
		app_status_message_color("Actions canceled; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status actions cancel limit reached is warning"
	);
	check(
		app_status_message_color("Reset canceled; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status reset cancel reset state/settings is warning"
	);
	check(
		app_status_message_color("Exit canceled; limit reached") ==
			APP_STATUS_COLOR_WARNING,
		"status exit cancel limit reached is warning"
	);
	check(
		app_status_message_color("Exit canceled; reset state/settings") ==
			APP_STATUS_COLOR_WARNING,
		"status exit cancel reset state/settings is warning"
	);
	check(
		app_status_message_color("Controls") == APP_STATUS_COLOR_NEUTRAL,
		"status controls is neutral"
	);
	check(
		app_status_message_color(NULL) == APP_STATUS_COLOR_NEUTRAL,
		"status null is neutral"
	);

	app_status_format_for_width(
		visible,
		sizeof(visible),
		"Answer shown; choose rating; settings ignored",
		31
	);
	check(
		strcmp(visible, "Answer sho...; settings ignored") == 0,
		"status width keeps settings warning suffix"
	);

	app_status_format_for_width(
		visible,
		sizeof(visible),
		"Limits saved; daily limit reached; state unmatched",
		31
	);
	check(
		strcmp(visible, "Limits save...; state unmatched") == 0,
		"status width keeps state warning suffix"
	);

	app_status_format_for_width(
		visible,
		sizeof(visible),
		"Progress reset; log kept; limit reached",
		31
	);
	check(
		strcmp(visible, "Progress rese...; limit reached") == 0,
		"status width keeps limit suffix"
	);

	app_status_format_for_width(
		visible,
		sizeof(visible),
		"New day; cards due",
		31
	);
	check(
		strcmp(visible, "New day; cards due") == 0,
		"status width leaves fitting message unchanged"
	);

	app_status_format_for_width(
		visible,
		sizeof(visible),
		"abcdefghijklmnopqrstuvwxyz",
		8
	);
	check(strcmp(visible, "abcde...") == 0, "status width truncates without suffix");

	app_status_format_for_width(visible, sizeof(visible), NULL, 31);
	check(visible[0] == '\0', "status width handles null message");
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

	write_file(TEST_SETTINGS_PATH, "# limits\n\nnew_limit\t1\nreview_limit\t2\n");

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
	check(access(TEST_SETTINGS_BACKUP_PATH, F_OK) != 0, "backup settings promotes backup");
	check(
		file_equals(TEST_SETTINGS_PATH, "new_limit\t9\nreview_limit\t10\n"),
		"backup settings promotes primary"
	);

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_loads_backup_when_primary_is_bad(void)
{
	struct app_settings settings;

	write_file(TEST_SETTINGS_PATH, "new_limit\tbad\n");
	write_file(TEST_SETTINGS_BACKUP_PATH, "new_limit\t9\nreview_limit\t10\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"backup settings load when primary is bad"
	);
	check(settings.new_limit == 9, "backup after bad primary new limit loads");
	check(settings.review_limit == 10, "backup after bad primary review limit loads");

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_partial_primary_falls_back_to_backup(void)
{
	struct app_settings settings;

	write_file(TEST_SETTINGS_PATH, "new_limit\t7\n");
	write_file(TEST_SETTINGS_BACKUP_PATH, "new_limit\t9\nreview_limit\t10\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"backup settings load when primary is incomplete"
	);
	check(settings.new_limit == 9, "backup after partial primary new limit loads");
	check(settings.review_limit == 10, "backup after partial primary review limit loads");

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_loads_temp_when_primary_and_backup_missing(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_TEMP_PATH, "new_limit\t11\nreview_limit\t12\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"temp settings load when primary and backup are missing"
	);
	check(settings.new_limit == 11, "temp settings new limit loads");
	check(settings.review_limit == 12, "temp settings review limit loads");
	check(access(TEST_SETTINGS_TEMP_PATH, F_OK) != 0, "temp settings promotes temp");
	check(
		file_equals(TEST_SETTINGS_PATH, "new_limit\t11\nreview_limit\t12\n"),
		"temp settings promotes primary"
	);

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_TEMP_PATH);
}

static void test_app_settings_bad_temp_falls_back_to_backup(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_PATH);
	write_file(TEST_SETTINGS_TEMP_PATH, "new_limit\tbad\n");
	write_file(TEST_SETTINGS_BACKUP_PATH, "new_limit\t9\nreview_limit\t10\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"backup settings load when temp is bad"
	);
	check(settings.new_limit == 9, "backup after bad temp new limit loads");
	check(settings.review_limit == 10, "backup after bad temp review limit loads");

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_bad_primary_prefers_backup_before_temp(void)
{
	struct app_settings settings;

	write_file(TEST_SETTINGS_PATH, "new_limit\tbad\n");
	write_file(TEST_SETTINGS_BACKUP_PATH, "new_limit\t9\nreview_limit\t10\n");
	write_file(TEST_SETTINGS_TEMP_PATH, "new_limit\t11\nreview_limit\t12\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_OK,
		"backup settings load before temp when primary is bad"
	);
	check(settings.new_limit == 9, "bad primary backup new limit wins");
	check(settings.review_limit == 10, "bad primary backup review limit wins");

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
}

static void test_app_settings_bad_file_uses_defaults(void)
{
	struct app_settings settings;
	struct app_settings_load_report report;

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_PATH, "new_limit\tbad\n");

	check(
		app_settings_load_with_report(
			&settings,
			TEST_SETTINGS_PATH,
			&report
		) == APP_SETTINGS_LOAD_BAD_FORMAT,
		"bad settings reports ignored"
	);
	check(report.line_number == 1, "bad settings reports first bad line");
	check(
		report.parse_result == APP_SETTINGS_PARSE_BAD_VALUE,
		"bad settings reports bad value"
	);
	check(settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT, "bad settings new default");
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"bad settings review default"
	);

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_rejects_non_plain_unsigned_values(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_PATH, "new_limit\t+1\nreview_limit\t2\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) ==
			APP_SETTINGS_LOAD_BAD_FORMAT,
		"signed settings value reports ignored"
	);
	check(
		settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT,
		"signed settings value uses new default"
	);
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"signed settings value uses review default"
	);

	write_file(TEST_SETTINGS_PATH, "new_limit\t 1\nreview_limit\t2\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) ==
			APP_SETTINGS_LOAD_BAD_FORMAT,
		"spaced settings value reports ignored"
	);
	check(
		settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT,
		"spaced settings value uses new default"
	);
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"spaced settings value uses review default"
	);

	write_file(TEST_SETTINGS_PATH, "new_limit\t1000001\nreview_limit\t2\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) ==
			APP_SETTINGS_LOAD_BAD_FORMAT,
		"oversized settings value reports ignored"
	);
	check(
		settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT,
		"oversized settings value uses new default"
	);
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"oversized settings value uses review default"
	);

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_duplicate_rows_use_defaults(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_PATH, "new_limit\t1\nnew_limit\t2\nreview_limit\t3\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_BAD_FORMAT,
		"duplicate new limit reports ignored"
	);
	check(
		settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT,
		"duplicate new limit uses default"
	);
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"duplicate new limit review default"
	);

	write_file(TEST_SETTINGS_PATH, "new_limit\t1\nreview_limit\t2\nreview_limit\t3\n");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_BAD_FORMAT,
		"duplicate review limit reports ignored"
	);
	check(
		settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT,
		"duplicate review limit new default"
	);
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"duplicate review limit uses default"
	);

	remove(TEST_SETTINGS_PATH);
}

static void test_app_settings_empty_file_uses_defaults(void)
{
	struct app_settings settings;

	remove(TEST_SETTINGS_TEMP_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
	write_file(TEST_SETTINGS_PATH, "");

	check(
		app_settings_load(&settings, TEST_SETTINGS_PATH) == APP_SETTINGS_LOAD_BAD_FORMAT,
		"empty settings reports ignored"
	);
	check(settings.new_limit == APP_SETTINGS_DEFAULT_NEW_LIMIT, "empty settings new default");
	check(
		settings.review_limit == APP_SETTINGS_DEFAULT_REVIEW_LIMIT,
		"empty settings review default"
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
	check(
		file_equals(TEST_SETTINGS_BACKUP_PATH, "new_limit\t1\nreview_limit\t2\n"),
		"settings save retains previous primary backup"
	);

	remove(TEST_SETTINGS_PATH);
	remove(TEST_SETTINGS_BACKUP_PATH);
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

static void write_numbered_cards_file(const char *path, size_t card_count)
{
	FILE *file = fopen(path, "w");

	check(file != NULL, "test numbered cards file opens");
	if (file == NULL)
		return;

	for (size_t index = 0; index < card_count; index++)
	{
		fprintf(
			file,
			"card-%03lu\tnote-%03lu\tfront %lu\tback %lu\ttag\n",
			(unsigned long)index,
			(unsigned long)index,
			(unsigned long)index,
			(unsigned long)index
		);
	}

	fclose(file);
}

static void write_newline_terminated_filler_file(const char *path, size_t size)
{
	FILE *file = fopen(path, "wb");
	bool failed = false;

	check(file != NULL, "test newline filler file opens");
	if (file == NULL)
		return;

	if (size == 0)
	{
		fclose(file);
		return;
	}

	for (size_t index = 1; index < size; index++)
	{
		if (fputc('x', file) == EOF)
		{
			failed = true;
			break;
		}
	}
	if (!failed && fputc('\n', file) == EOF)
		failed = true;

	check(!failed, "test newline filler file writes");
	fclose(file);
}

static bool file_equals(const char *path, const char *content)
{
	char buffer[512];
	FILE *file = fopen(path, "r");
	size_t bytes_read;
	bool matches;

	if (file == NULL)
		return false;

	bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
	if (ferror(file))
	{
		fclose(file);
		return false;
	}

	buffer[bytes_read] = '\0';
	matches = strcmp(buffer, content) == 0 && fgetc(file) == EOF;
	fclose(file);
	return matches;
}

static long file_size(const char *path)
{
	FILE *file = fopen(path, "rb");
	long size;

	if (file == NULL)
		return -1;
	if (fseek(file, 0, SEEK_END) != 0)
	{
		fclose(file);
		return -1;
	}

	size = ftell(file);
	fclose(file);
	return size;
}

static long file_line_count(const char *path)
{
	FILE *file = fopen(path, "r");
	long count = 0;
	int value;

	if (file == NULL)
		return -1;

	while ((value = fgetc(file)) != EOF)
	{
		if (value == '\n')
			count++;
	}

	if (ferror(file))
	{
		fclose(file);
		return -1;
	}

	fclose(file);
	return count;
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
	char longest_valid_id[DECK_MAX_NAME_LENGTH];
	char too_long_id[DECK_MAX_NAME_LENGTH + 1];

	memset(longest_valid_id, 'a', sizeof(longest_valid_id) - 1);
	longest_valid_id[sizeof(longest_valid_id) - 1] = '\0';
	memset(too_long_id, 'b', sizeof(too_long_id) - 1);
	too_long_id[sizeof(too_long_id) - 1] = '\0';

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
		strcmp(entry.review_log_path, "/root/sample/review-log.tsv") == 0,
		"review log path builds"
	);
	check(
		strcmp(entry.settings_path, "/root/sample/settings.tsv") == 0,
		"settings path builds"
	);
	check(!deck_index_build_entry(&entry, "/root", ".hidden"), "hidden id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad/id"), "slash id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad\\id"), "backslash id rejected");
	check(!deck_index_build_entry(&entry, "/root", "bad id"), "space id rejected");
	check(
		deck_index_build_entry(&entry, "/root", longest_valid_id),
		"longest deck id accepted"
	);
	check(
		strcmp(entry.id, longest_valid_id) == 0,
		"longest deck id stores without truncation"
	);
	check(!deck_index_build_entry(&entry, "/root", too_long_id), "too-long deck id rejected");
}

static void remove_test_deck_dir(const char *deck_id)
{
	char path[256];

	snprintf(path, sizeof(path), "%s/%s/cards.tsv", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/deck.json", TEST_DECK_ROOT, deck_id);
	remove(path);
	snprintf(path, sizeof(path), "%s/%s/review-log.tsv", TEST_DECK_ROOT, deck_id);
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
	remove_test_deck_dir("bad id");
	remove_test_deck_dir("bad-settings");
	remove_test_deck_dir("bad-state");
	remove_test_deck_dir("beta");
	remove_test_deck_dir("broken");
	remove_test_deck_dir("empty");
	remove_test_deck_dir("gamma");
	remove_test_deck_dir("reset");
	remove_test_deck_dir("summary");
	remove_test_deck_dir("zeta");

	for (
		unsigned int deck_number = 0;
		deck_number < DECK_INDEX_MAX_DECKS + 2;
		deck_number++
	)
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
	create_test_deck_dir("bad id", true);

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == 2, "deck index scans only folders with cards");
	check(index.total_count == 2, "deck index counts valid decks");
	check(index.ignored_count == 2, "deck index counts ignored entries");
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
	create_test_deck_dir_with_name("gamma", true, "{ \"name\": \"bad\\nname\" }\n");
	create_test_deck_dir_with_name("zeta", true, "{ \"name\": null, \"created_by\": \"x\" }\n");

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == 4, "deck index scans named decks");
	check(index.total_count == 4, "deck index counts named decks");
	check(index.ignored_count == 0, "deck index has no ignored named decks");
	check(strcmp(index.entries[0].id, "alpha") == 0, "named deck id remains folder id");
	check(strcmp(index.entries[0].display_name, "Alpha Deck") == 0, "deck name loads");
	check(strcmp(index.entries[1].display_name, "beta") == 0, "bad deck name falls back");
	check(
		strcmp(index.entries[2].display_name, "gamma") == 0,
		"escaped control deck name falls back"
	);
	check(strcmp(index.entries[3].display_name, "zeta") == 0, "non-string deck name falls back");

	cleanup_deck_index_test_root();
}

static void test_deck_index_reports_overflow(void)
{
	struct deck_index index;

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);

	for (
		unsigned int deck_number = 0;
		deck_number < DECK_INDEX_MAX_DECKS + 2;
		deck_number++
	)
	{
		char deck_id[16];

		snprintf(deck_id, sizeof(deck_id), "deck%02u", deck_number);
		create_test_deck_dir(deck_id, true);
	}

	deck_index_scan(&index, TEST_DECK_ROOT);

	check(index.count == DECK_INDEX_MAX_DECKS, "deck index stops at display limit");
	check(
		index.total_count == DECK_INDEX_MAX_DECKS + 2,
		"deck index counts all valid decks"
	);
	check(index.overflowed, "deck index reports overflow");
	check(index.ignored_count == 0, "deck index overflow ignores no entries");
	check(strcmp(index.entries[0].id, "deck00") == 0, "deck index keeps sorted first deck");
	check(
		strcmp(index.entries[DECK_INDEX_MAX_DECKS - 1].id, "deck63") == 0,
		"deck index keeps first visible deck set"
	);

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
		"card-5\tnote-5\tfront 5\tback 5\ttag\n"
	);
	write_file(entry.settings_path, "new_limit\t1\nreview_limit\t0\n");
	write_file(
		entry.state_path,
		"card-1\t1\t2\t20001\t1\t2500\t0\t0\t19999\t19999\n"
		"card-2\t1\t2\t20000\t1\t2500\t0\t0\t19999\t19999\n"
		"card-3\t0\t2\t20000\t0\t2500\t0\t1\t0\t0\n"
		"card-5\t1\t0\t20000\t0\t2300\t0\t0\t19999\t19999\n"
	);

	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_OK, "summary deck loads");
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "summary settings load");
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "summary state loads");
	check(summary.card_count == 5, "summary card count");
	check(summary.due_count == 3, "summary due count includes review, learning, and limited new");
	check(summary.new_due_count == 1, "summary new due count honors new limit");
	check(summary.learning_due_count == 1, "summary learning due count");
	check(summary.review_due_count == 1, "summary review due count");
	check(summary.new_limit_blocked_count == 0, "summary new limit blocks none");
	check(summary.review_limit_blocked_count == 0, "summary review limit blocks none");
	check(summary.suspended_count == 1, "summary suspended count includes saved state");

	cleanup_deck_index_test_root();
}

static void test_deck_summary_reports_bad_settings_with_default_counts(void)
{
	struct deck_entry entry;
	struct deck_summary summary;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/bad-settings", TEST_DECK_ROOT);
	mkdir(path, 0700);

	check(
		deck_index_build_entry(&entry, TEST_DECK_ROOT, "bad-settings"),
		"bad settings summary deck entry builds"
	);
	write_file(
		entry.cards_path,
		"card-1\tnote-1\tfront 1\tback 1\ttag\n"
		"card-2\tnote-2\tfront 2\tback 2\ttag\n"
	);
	write_file(entry.settings_path, "new_limit\tbad\n");

	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_OK, "bad settings summary deck loads");
	check(
		summary.settings_load_result == APP_SETTINGS_LOAD_BAD_FORMAT,
		"bad settings summary reports ignored settings"
	);
	check(
		summary.settings_load_report.line_number == 1,
		"bad settings summary reports bad line"
	);
	check(
		summary.settings_load_report.parse_result == APP_SETTINGS_PARSE_BAD_VALUE,
		"bad settings summary reports bad value"
	);
	check(
		summary.state_load_result == REVIEW_STATE_LOAD_NOT_FOUND,
		"bad settings summary starts without state"
	);
	check(summary.card_count == 2, "bad settings summary keeps card count");
	check(summary.due_count == 2, "bad settings summary keeps default due count");
	check(summary.new_due_count == 2, "bad settings summary keeps default new count");
	check(
		summary.new_limit_blocked_count == 0,
		"bad settings summary has no new blocked count"
	);
	check(
		summary.review_limit_blocked_count == 0,
		"bad settings summary has no review blocked count"
	);

	cleanup_deck_index_test_root();
}

static void test_deck_summary_suppresses_bad_state_counts(void)
{
	struct deck_entry entry;
	struct deck_summary summary;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/bad-state", TEST_DECK_ROOT);
	mkdir(path, 0700);

	check(
		deck_index_build_entry(&entry, TEST_DECK_ROOT, "bad-state"),
		"bad state summary deck entry builds"
	);
	write_file(
		entry.cards_path,
		"card-1\tnote-1\tfront 1\tback 1\ttag\n"
		"card-2\tnote-2\tfront 2\tback 2\ttag\n"
	);
	write_file(entry.state_path, "bad\n");

	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_OK, "bad state summary deck loads");
	check(
		summary.state_load_result == REVIEW_STATE_LOAD_BAD_FORMAT,
		"bad state summary reports bad state"
	);
	check(summary.card_count == 2, "bad state summary keeps card count");
	check(summary.due_count == 0, "bad state summary hides due count");
	check(summary.new_due_count == 0, "bad state summary hides new count");
	check(summary.learning_due_count == 0, "bad state summary hides learning count");
	check(summary.review_due_count == 0, "bad state summary hides review count");
	check(summary.new_limit_blocked_count == 0, "bad state summary hides new blocked count");
	check(
		summary.review_limit_blocked_count == 0,
		"bad state summary hides review blocked count"
	);
	check(summary.suspended_count == 0, "bad state summary hides suspended count");

	cleanup_deck_index_test_root();
}

static void test_deck_summary_from_session_counts_due_cards(void)
{
	struct scheduler_session session;
	struct deck_summary summary;

	scheduler_init(&session, 4, TEST_TODAY);
	scheduler_set_daily_limits(&session, 1, 0);
	check(
		scheduler_restore_card(
			&session,
			1,
			1,
			SCHEDULER_RATING_AGAIN,
			TEST_TODAY,
			0,
			2300,
			0,
			false,
			TEST_TODAY,
			TEST_TODAY
		),
		"summary session learning card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			2,
			3,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			5,
			2500,
			0,
			false,
			TEST_TODAY - 10,
			TEST_TODAY - 5
		),
		"summary session review card restores"
	);
	check(
		scheduler_restore_card(
			&session,
			3,
			0,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			0,
			2500,
			0,
			true,
			0,
			0
		),
		"summary session suspended card restores"
	);

	deck_summary_from_session(
		&summary,
		DECK_LOAD_OK,
		APP_SETTINGS_LOAD_OK,
		REVIEW_STATE_LOAD_OK,
		&session
	);

	check(summary.deck_load_result == DECK_LOAD_OK, "summary session deck result");
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "summary session settings result");
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "summary session state result");
	check(summary.card_count == 4, "summary session card count");
	check(summary.due_count == 2, "summary session due count");
	check(summary.new_due_count == 0, "summary session new count after suspend");
	check(summary.learning_due_count == 1, "summary session learning count");
	check(summary.review_due_count == 1, "summary session review count");
	check(summary.new_limit_blocked_count == 1, "summary session new limit blocked count");
	check(
		summary.review_limit_blocked_count == 0,
		"summary session review limit blocked count"
	);
	check(summary.suspended_count == 1, "summary session suspended count");

	scheduler_init(&session, 2, TEST_TODAY);
	scheduler_set_daily_limits(&session, 0, 1);
	check(
		scheduler_restore_card(
			&session,
			0,
			2,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			2,
			2500,
			0,
			false,
			TEST_TODAY - 10,
			TEST_TODAY - 2
		),
		"summary session first limited review restores"
	);
	check(
		scheduler_restore_card(
			&session,
			1,
			2,
			SCHEDULER_RATING_GOOD,
			TEST_TODAY,
			2,
			2500,
			0,
			false,
			TEST_TODAY - 10,
			TEST_TODAY - 2
		),
		"summary session second limited review restores"
	);
	deck_summary_from_session(
		&summary,
		DECK_LOAD_OK,
		APP_SETTINGS_LOAD_OK,
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(summary.due_count == 1, "summary session review limit visible due count");
	check(summary.review_due_count == 1, "summary session review limit visible review count");
	check(
		summary.review_limit_blocked_count == 1,
		"summary session review limit blocked count"
	);

	scheduler_init(&session, 4, TEST_TODAY);
	deck_summary_from_session(
		&summary,
		DECK_LOAD_BAD_FORMAT,
		APP_SETTINGS_LOAD_OK,
		REVIEW_STATE_LOAD_OK,
		&session
	);
	check(summary.card_count == 0, "bad session summary clears card count");
	check(summary.due_count == 0, "bad session summary clears due count");
	check(summary.new_limit_blocked_count == 0, "bad session summary clears new blocked count");
	check(
		summary.review_limit_blocked_count == 0,
		"bad session summary clears review blocked count"
	);

	deck_summary_from_session(
		&summary,
		DECK_LOAD_OK,
		APP_SETTINGS_LOAD_OK,
		REVIEW_STATE_LOAD_BAD_FORMAT,
		&session
	);
	check(summary.card_count == 4, "bad state session summary keeps card count");
	check(summary.due_count == 0, "bad state session summary clears due count");
	check(summary.new_due_count == 0, "bad state session summary clears new count");
	check(
		summary.new_limit_blocked_count == 0,
		"bad state session summary clears new blocked count"
	);
	check(
		summary.review_limit_blocked_count == 0,
		"bad state session summary clears review blocked count"
	);
	check(summary.suspended_count == 0, "bad state session summary clears suspended count");
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

	write_file(entry.cards_path, "bad\trow\n");
	deck_summary_load(&summary, &entry, TEST_TODAY);

	check(summary.deck_load_result == DECK_LOAD_BAD_FORMAT, "summary reports bad cards");
	check(summary.deck_load_report.line_number == 1, "summary reports bad line");
	check(
		summary.deck_load_report.parse_result == DECK_PARSE_BAD_FIELD_COUNT,
		"summary reports bad parse reason"
	);

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
	size_t card_index;
	struct scheduler_card before;
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

	card_index = scheduler_current_index(&alpha_session);
	before = alpha_session.cards[card_index];
	scheduler_rate_current(&alpha_session, SCHEDULER_RATING_GOOD);
	check(alpha_session.due_count == 2, "daily workflow alpha rating applies");
	append_review_log_transition(
		index.entries[alpha_index].review_log_path,
		12345,
		TEST_TODAY,
		REVIEW_LOG_EVENT_RATING,
		alpha_deck.cards[card_index].card_id,
		SCHEDULER_RATING_GOOD,
		&before,
		&alpha_session.cards[card_index],
		"daily workflow alpha rating logs"
	);
	save_entry_state(
		&alpha_deck,
		&alpha_session,
		&index.entries[alpha_index],
		"daily workflow alpha rating saves"
	);
	before = alpha_session.cards[card_index];
	check(scheduler_undo_last(&alpha_session), "daily workflow alpha rating undo works");
	check(alpha_session.due_count == 3, "daily workflow alpha undo restores due count");
	append_review_log_transition(
		index.entries[alpha_index].review_log_path,
		12346,
		TEST_TODAY,
		REVIEW_LOG_EVENT_UNDO,
		alpha_deck.cards[card_index].card_id,
		SCHEDULER_RATING_COUNT,
		&before,
		&alpha_session.cards[card_index],
		"daily workflow alpha undo logs"
	);
	save_entry_state(
		&alpha_deck,
		&alpha_session,
		&index.entries[alpha_index],
		"daily workflow alpha undo saves"
	);
	card_index = scheduler_current_index(&alpha_session);
	before = alpha_session.cards[card_index];
	scheduler_rate_current(&alpha_session, SCHEDULER_RATING_EASY);
	check(alpha_session.cards[0].due_day == TEST_TODAY + 4, "daily workflow alpha rerates");
	append_review_log_transition(
		index.entries[alpha_index].review_log_path,
		12347,
		TEST_TODAY,
		REVIEW_LOG_EVENT_RATING,
		alpha_deck.cards[card_index].card_id,
		SCHEDULER_RATING_EASY,
		&before,
		&alpha_session.cards[card_index],
		"daily workflow alpha rerating logs"
	);
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
	scheduler_set_daily_limits(
		&alpha_session,
		alpha_settings.new_limit,
		alpha_settings.review_limit
	);
	deck_summary_from_session(
		&summary,
		DECK_LOAD_OK,
		APP_SETTINGS_LOAD_OK,
		REVIEW_STATE_LOAD_NOT_FOUND,
		&alpha_session
	);
	check(
		summary.due_count == 1,
		"daily workflow alpha live summary due limit applies"
	);
	check(
		summary.new_due_count == 1,
		"daily workflow alpha live summary new count applies"
	);
	check(
		summary.new_limit_blocked_count == 1,
		"daily workflow alpha live summary shows blocked new count"
	);

	load_entry_deck(
		&beta_deck,
		&index.entries[beta_index],
		"daily workflow beta deck loads"
	);
	scheduler_init(&beta_session, beta_deck.card_count, TEST_TODAY);
	card_index = scheduler_current_index(&beta_session);
	before = beta_session.cards[card_index];
	check(scheduler_suspend_current(&beta_session), "daily workflow beta suspends card");
	check(scheduler_suspended_count(&beta_session) == 1, "daily workflow beta count suspends");
	append_review_log_transition(
		index.entries[beta_index].review_log_path,
		12348,
		TEST_TODAY,
		REVIEW_LOG_EVENT_SUSPEND,
		beta_deck.cards[card_index].card_id,
		SCHEDULER_RATING_COUNT,
		&before,
		&beta_session.cards[card_index],
		"daily workflow beta suspend logs"
	);
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta suspension saves"
	);
	deck_summary_load(&summary, &index.entries[beta_index], TEST_TODAY);
	check(summary.suspended_count == 1, "daily workflow beta summary shows suspension");

	before = beta_session.cards[card_index];
	check(scheduler_unsuspend_all(&beta_session) == 1, "daily workflow beta restores card");
	check(scheduler_suspended_count(&beta_session) == 0, "daily workflow beta count restores");
	append_review_log_transition(
		index.entries[beta_index].review_log_path,
		12349,
		TEST_TODAY,
		REVIEW_LOG_EVENT_RESTORE,
		beta_deck.cards[card_index].card_id,
		SCHEDULER_RATING_COUNT,
		&before,
		&beta_session.cards[card_index],
		"daily workflow beta restore logs"
	);
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta restore saves"
	);
	card_index = scheduler_current_index(&beta_session);
	before = beta_session.cards[card_index];
	scheduler_rate_current(&beta_session, SCHEDULER_RATING_GOOD);
	append_review_log_transition(
		index.entries[beta_index].review_log_path,
		12350,
		TEST_TODAY,
		REVIEW_LOG_EVENT_RATING,
		beta_deck.cards[card_index].card_id,
		SCHEDULER_RATING_GOOD,
		&before,
		&beta_session.cards[card_index],
		"daily workflow beta review logs"
	);
	save_entry_state(
		&beta_deck,
		&beta_session,
		&index.entries[beta_index],
		"daily workflow beta review saves"
	);

	deck_summary_load(&summary, &index.entries[alpha_index], TEST_TODAY);
	check(summary.deck_load_result == DECK_LOAD_OK, "daily workflow alpha summary reloads");
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "daily workflow alpha summary settings");
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "daily workflow alpha summary state");
	check(summary.card_count == 3, "daily workflow alpha summary card count persists");
	check(summary.due_count == 1, "daily workflow alpha summary due limit applies");
	check(summary.new_due_count == 1, "daily workflow alpha summary new count applies");
	check(summary.learning_due_count == 0, "daily workflow alpha summary has no learning due");
	check(summary.review_due_count == 0, "daily workflow alpha summary has no review due");
	check(summary.suspended_count == 0, "daily workflow alpha summary stays unsuspended");

	deck_summary_load(&summary, &index.entries[beta_index], TEST_TODAY);
	check(summary.deck_load_result == DECK_LOAD_OK, "daily workflow beta summary reloads");
	check(
		summary.settings_load_result == APP_SETTINGS_LOAD_NOT_FOUND,
		"daily workflow beta summary uses default settings"
	);
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "daily workflow beta summary state");
	check(summary.card_count == 2, "daily workflow beta summary card count persists");
	check(summary.due_count == 1, "daily workflow beta summary due count persists");
	check(summary.new_due_count == 1, "daily workflow beta summary new count persists");
	check(summary.learning_due_count == 0, "daily workflow beta summary has no learning due");
	check(summary.review_due_count == 0, "daily workflow beta summary has no review due");
	check(summary.suspended_count == 0, "daily workflow beta summary restore persists");

	check(
		file_line_count(index.entries[alpha_index].review_log_path) == 3,
		"daily workflow alpha keeps three log rows"
	);
	check(
		file_line_count(index.entries[beta_index].review_log_path) == 3,
		"daily workflow beta keeps three log rows"
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
	check(
		scheduler_reviewed_today_count(&alpha_reloaded) == 1,
		"daily workflow alpha today card count reloads"
	);

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
	check(
		scheduler_reviewed_today_count(&beta_reloaded) == 1,
		"daily workflow beta today card count reloads"
	);
	check(
		file_line_count(index.entries[alpha_index].review_log_path) == 3,
		"daily workflow alpha log persists after reload"
	);
	check(
		file_line_count(index.entries[beta_index].review_log_path) == 3,
		"daily workflow beta log persists after reload"
	);

	cleanup_deck_index_test_root();
}

static void test_daily_use_reset_clears_progress_and_keeps_settings(void)
{
	struct deck_index index;
	struct deck deck;
	struct scheduler_session session;
	struct scheduler_session reloaded;
	struct app_settings settings;
	struct app_settings loaded_settings;
	struct deck_summary summary;
	size_t reset_index = 0;
	size_t card_index;
	struct scheduler_card before;
	char path[256];

	cleanup_deck_index_test_root();
	mkdir(TEST_DECK_ROOT, 0700);
	snprintf(path, sizeof(path), "%s/reset", TEST_DECK_ROOT);
	mkdir(path, 0700);
	snprintf(path, sizeof(path), "%s/reset/cards.tsv", TEST_DECK_ROOT);
	write_file(
		path,
		"reset-1\tnote-1\treset front 1\treset back 1\ttag\n"
		"reset-2\tnote-2\treset front 2\treset back 2\ttag\n"
		"reset-3\tnote-3\treset front 3\treset back 3\ttag\n"
	);

	deck_index_scan(&index, TEST_DECK_ROOT);
	check(index.count == 1, "daily reset scans one deck");
	check(deck_index_find(&index, "reset", &reset_index), "daily reset finds deck");

	settings.new_limit = 2;
	settings.review_limit = 4;
	check(
		app_settings_save(&settings, index.entries[reset_index].settings_path) ==
			APP_SETTINGS_SAVE_OK,
		"daily reset settings save"
	);

	load_entry_deck(&deck, &index.entries[reset_index], "daily reset deck loads");
	scheduler_init(&session, deck.card_count, TEST_TODAY);
	scheduler_set_daily_limits(&session, settings.new_limit, settings.review_limit);
	check(
		review_state_load(&deck, &session, index.entries[reset_index].state_path) ==
			REVIEW_STATE_LOAD_NOT_FOUND,
		"daily reset starts without state"
	);
	check(session.due_count == 2, "daily reset settings limit initial due cards");

	card_index = scheduler_current_index(&session);
	before = session.cards[card_index];
	scheduler_rate_current(&session, SCHEDULER_RATING_GOOD);
	append_review_log_transition(
		index.entries[reset_index].review_log_path,
		12351,
		TEST_TODAY,
		REVIEW_LOG_EVENT_RATING,
		deck.cards[card_index].card_id,
		SCHEDULER_RATING_GOOD,
		&before,
		&session.cards[card_index],
		"daily reset rating logs"
	);
	save_entry_state(
		&deck,
		&session,
		&index.entries[reset_index],
		"daily reset rating saves"
	);
	check(access(index.entries[reset_index].state_path, F_OK) == 0, "daily reset state exists");
	check(access(index.entries[reset_index].review_log_path, F_OK) == 0, "daily reset log exists");

	deck_summary_load(&summary, &index.entries[reset_index], TEST_TODAY);
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "daily reset summary settings");
	check(summary.state_load_result == REVIEW_STATE_LOAD_OK, "daily reset summary state");
	check(summary.due_count == 1, "daily reset summary reflects saved progress");

	check(
		review_state_delete(index.entries[reset_index].state_path),
		"daily reset deletes state artifacts"
	);
	check(
		review_log_delete(index.entries[reset_index].review_log_path),
		"daily reset deletes review log artifacts"
	);
	check(access(index.entries[reset_index].state_path, F_OK) != 0, "daily reset state removed");
	check(access(index.entries[reset_index].review_log_path, F_OK) != 0, "daily reset log removed");
	check(
		access(index.entries[reset_index].settings_path, F_OK) == 0,
		"daily reset settings remain"
	);

	check(
		app_settings_load(&loaded_settings, index.entries[reset_index].settings_path) ==
			APP_SETTINGS_LOAD_OK,
		"daily reset settings reload"
	);
	check(loaded_settings.new_limit == 2, "daily reset new limit persists");
	check(loaded_settings.review_limit == 4, "daily reset review limit persists");

	deck_summary_load(&summary, &index.entries[reset_index], TEST_TODAY);
	check(summary.settings_load_result == APP_SETTINGS_LOAD_OK, "daily reset summary settings persist");
	check(
		summary.state_load_result == REVIEW_STATE_LOAD_NOT_FOUND,
		"daily reset summary reports cleared state"
	);
	check(summary.due_count == 2, "daily reset due cards return under saved limits");
	check(summary.new_due_count == 2, "daily reset new count returns under saved limits");
	check(summary.suspended_count == 0, "daily reset has no suspended cards");

	scheduler_init(&reloaded, deck.card_count, TEST_TODAY);
	scheduler_set_daily_limits(
		&reloaded,
		loaded_settings.new_limit,
		loaded_settings.review_limit
	);
	check(
		review_state_load(&deck, &reloaded, index.entries[reset_index].state_path) ==
			REVIEW_STATE_LOAD_NOT_FOUND,
		"daily reset reload sees no state"
	);
	check(reloaded.due_count == 2, "daily reset reload applies preserved limits");
	check(reloaded.cards[0].review_count == 0, "daily reset clears reviewed card");

	cleanup_deck_index_test_root();
}

int main(void)
{
	test_parse_card_line();
	test_reject_bad_card_line();
	test_deck_load_rejects_duplicate_card_ids();
	test_deck_load_accepts_final_line_without_newline();
	test_deck_load_card_limit();
	test_tracked_text_sample_decks_load();
	test_app_power_battery_poll_schedule();
	test_app_power_battery_poll_arms_after_missing_clock();
	test_app_power_battery_sample_policy();
	test_app_power_battery_display_state();
	test_app_power_idle_input_backoff();
	test_app_text_counts_utf8_columns();
	test_app_text_counts_wrapped_rows();
	test_app_time_local_calendar_day();
	test_scheduler_schedules_due_days();
	test_app_controls_review_front_actions();
	test_app_controls_review_rating_keys();
	test_app_controls_rejects_ambiguous_ratings();
	test_app_controls_rejects_ambiguous_dpad_axes();
	test_app_controls_requires_single_command();
	test_app_controls_modal_controls();
	test_app_controls_classifies_app_actions();
	test_app_controls_settings_classifies_local_commands();
	test_app_controls_action_confirm_classifies_local_commands();
	test_app_controls_navigation_repeat();
	test_app_controls_navigation_repeat_modes();
	test_app_controls_input_activity();
	test_scheduler_rejects_invalid_rating();
	test_scheduler_new_again_stays_in_initial_learning();
	test_scheduler_scales_review_intervals();
	test_scheduler_caps_loaded_state_counters();
	test_scheduler_undo_last_rating();
	test_scheduler_suspend_current();
	test_scheduler_suspend_last_due_card();
	test_scheduler_unsuspend_all();
	test_scheduler_limits_new_cards();
	test_scheduler_limit_change_clears_undo();
	test_scheduler_day_change_resets_new_limit();
	test_scheduler_allows_started_new_card_after_limit();
	test_scheduler_limits_review_cards();
	test_scheduler_review_limit_uses_due_priority();
	test_scheduler_restore_repositions_to_due_card();
	test_scheduler_day_change_resets_review_limit();
	test_scheduler_prioritizes_learning_cards();
	test_scheduler_learning_cards_bypass_review_limit();
	test_scheduler_counts_due_card_types();
	test_scheduler_prioritizes_overdue_reviews_before_new_cards();
	test_scheduler_restored_started_new_card_stays_due_after_limit();
	test_scheduler_restored_started_review_card_stays_due_after_limit();
	test_review_state_missing_file();
	test_review_state_load_rejects_null_arguments();
	test_review_state_round_trip();
	test_review_state_round_trip_suspended_card();
	test_review_state_round_trip_review_limit_count();
	test_review_state_save_rejects_count_mismatch();
	test_review_state_save_retains_backup_for_primary_recovery();
	test_review_state_loads_backup_when_primary_missing();
	test_review_state_loads_backup_when_primary_is_bad();
	test_review_state_loads_temp_when_primary_and_backup_missing();
	test_review_state_bad_temp_falls_back_to_backup();
	test_review_state_bad_primary_prefers_backup_before_temp();
	test_review_state_empty_file_is_bad_format();
	test_review_state_unknown_only_file_is_unmatched();
	test_review_state_duplicate_card_row_is_bad_format();
	test_review_state_marked_file_requires_complete_footer();
	test_review_state_rejects_inconsistent_current_rows();
	test_review_state_loads_previous_current_format();
	test_review_state_migrated_review_round_trips_after_rating();
	test_review_state_loads_suspended_format();
	test_review_state_loads_legacy_done_format();
	test_review_state_bad_load_does_not_mutate_session();
	test_review_state_save_policy_rejects_bad_load();
	test_app_review_queue_requires_safe_state();
	test_app_review_formats_rating_status();
	test_app_review_formats_day_change_status();
	test_app_status_classifies_daily_use_feedback();
	test_review_state_delete_removes_save_artifacts();
	test_storage_replace_file_commits_temp_file();
	test_storage_replace_file_commits_first_save();
	test_storage_replace_file_preserves_backup_without_primary();
	test_storage_delete_save_files_removes_related_files();
	test_storage_delete_save_files_removes_orphaned_artifacts();
	test_review_log_appends_study_events();
	test_review_log_delete_removes_log_file();
	test_review_log_rejects_full_log();
	test_review_log_repairs_partial_final_row();
	test_review_log_recovers_pending_repair_before_append();
	test_review_log_appends_at_capacity_boundary();
	test_app_settings_missing_file_uses_defaults();
	test_app_settings_loads_limits();
	test_app_settings_loads_backup_when_primary_missing();
	test_app_settings_loads_backup_when_primary_is_bad();
	test_app_settings_partial_primary_falls_back_to_backup();
	test_app_settings_loads_temp_when_primary_and_backup_missing();
	test_app_settings_bad_temp_falls_back_to_backup();
	test_app_settings_bad_primary_prefers_backup_before_temp();
	test_app_settings_bad_file_uses_defaults();
	test_app_settings_rejects_non_plain_unsigned_values();
	test_app_settings_duplicate_rows_use_defaults();
	test_app_settings_empty_file_uses_defaults();
	test_app_settings_save_round_trip();
	test_app_settings_save_replaces_existing_file();
	test_deck_index_builds_paths();
	test_deck_index_scans_sorted_decks_with_cards();
	test_deck_index_loads_display_names();
	test_deck_index_reports_overflow();
	test_deck_summary_counts_due_cards();
	test_deck_summary_reports_bad_settings_with_default_counts();
	test_deck_summary_suppresses_bad_state_counts();
	test_deck_summary_from_session_counts_due_cards();
	test_deck_summary_reports_load_error();
	test_daily_use_workflow_persists_two_decks();
	test_daily_use_reset_clears_progress_and_keeps_settings();

	if (failures != 0)
	{
		printf("%d deck/scheduler test failures\n", failures);
		return 1;
	}

	puts("deck/scheduler tests passed");
	return 0;
}
