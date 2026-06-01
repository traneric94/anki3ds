# C Style And Architecture

This project should stay boring, explicit, and easy to debug on hardware.
The 3DS app is a small utility, not a framework.

## Priorities

Prefer, in order:

1. Correctness on real hardware.
2. Clear control flow.
3. Small functions with obvious ownership.
4. Simple data structures.
5. Performance only where measured or clearly necessary.

Avoid cleverness. A little repetition is better than a premature abstraction.

## Formatting

- Use C for the 3DS app.
- Follow the devkitPro/libctru example style where reasonable.
- Use tabs for C indentation in `app-3ds/`.
- Put opening braces on their own line.
- Keep lines near 100 columns when practical.
- Use ASCII in source files unless there is a specific test fixture that needs
  UTF-8.

Example:

```c
static void draw_screen(void)
{
	printf("anki3ds\n");
}
```

## Naming

Use names that describe the domain object instead of implementation details.

- files: `snake_case.c`, `snake_case.h`
- functions: `snake_case`
- variables: `snake_case`
- macros and constants: `UPPER_SNAKE_CASE`
- enum values: `UPPER_SNAKE_CASE`
- modules: noun or domain names, such as `deck`, `scheduler`, `storage`, `ui`

Public module functions should use a module prefix:

```c
bool deck_load(struct deck *deck, const char *path);
void deck_free(struct deck *deck);
void scheduler_rate(struct review_state *state, enum rating rating);
```

Static helpers can use shorter names when the file gives enough context:

```c
static bool parse_card_line(struct card *card, const char *line);
```

Avoid vague abbreviations:

- prefer `card_count` over `cnt`
- prefer `selected_index` over `sel`
- `i`, `x`, `y`, `row`, and `col` are fine for tight local loops

## Types

Use plain structs and enums first.

```c
enum rating
{
	RATING_AGAIN,
	RATING_HARD,
	RATING_GOOD,
	RATING_EASY,
};

struct review_state
{
	u32 due_day;
	u32 interval_days;
	u16 ease;
	u16 lapses;
};
```

Guidelines:

- Avoid `_t` suffixes for project types.
- Prefer `struct name` over typedefs unless an opaque type genuinely improves
  the API.
- Use `size_t` for sizes and indexes into memory buffers.
- Use libctru integer types like `u32` when interacting with libctru APIs.
- Keep struct ownership clear: the module that allocates memory frees it.

## Abstractions

Add an abstraction only when it removes real complexity or protects a clear
boundary.

Good early module boundaries:

- `ui`: screens, text layout, button prompts
- `deck`: card/deck data loaded from files
- `storage`: SD-card paths, safe reads, safe writes
- `scheduler`: review state transitions
- `app`: main loop and high-level state machine

Avoid early:

- generic containers
- callback-heavy APIs
- object systems in C
- deeply nested state machines
- global service locators
- large header-only utility files

The first working version can use fixed-size arrays and explicit limits. Replace
them only when a real deck or hardware test proves they are too restrictive.

## State

Keep mutable state in one top-level app struct when possible.

```c
struct app_state
{
	enum screen screen;
	size_t selected_deck_index;
	size_t current_card_index;
	bool answer_visible;
};
```

Rules:

- Avoid global mutable state.
- File-local `static const` tables are fine.
- Make ownership visible in field names and module APIs.
- Keep UI state separate from saved review state.

## Error Handling

The app should fail visibly and recoverably.

- Return `bool` for simple success/failure.
- Use small enums when callers need to distinguish error causes.
- Do not crash or exit immediately for expected SD-card/file problems.
- Show concise on-device error messages.
- Keep enough detail in logs or debug text to reproduce hardware issues.

Example:

```c
enum deck_load_result
{
	DECK_LOAD_OK,
	DECK_LOAD_NOT_FOUND,
	DECK_LOAD_BAD_FORMAT,
	DECK_LOAD_TOO_LARGE,
};
```

## Memory

The original 3DS is constrained, so memory behavior should be predictable.

- Prefer stack values for small temporary objects.
- Prefer fixed buffers for bounded text parsing.
- Allocate deck/card arrays deliberately and free them from the same owner.
- Check allocation results.
- Avoid recursion.
- Avoid holding multiple full decks in memory before there is a measured need.

## File IO

SD-card behavior matters more than speed.

- Keep imported card content separate from app-owned review state.
- Write save files through a temporary file, then replace the old file.
- Save after each rating once save-state support exists.
- Treat missing state as normal for a new deck.
- Treat corrupted state as recoverable, with a visible warning.

## Testability

Keep non-3DS logic portable enough to test on the desktop.

Good candidates for host-side tests:

- TSV escaping and parsing
- scheduler transitions
- deck update/state preservation
- safe path construction

libctru-specific code should stay near the app shell and UI layer. The scheduler
and deck parser should not need 3DS graphics or input headers.

## Comments

Comment intent, constraints, and surprising hardware behavior. Do not narrate
obvious assignments.

Good:

```c
/* Keep this below one screen so hardware test photos show the full error. */
```

Not useful:

```c
/* Increment i by one. */
```

## Review Checklist

Before committing C changes:

- Does the code build with `make`?
- Is ownership obvious?
- Are errors handled without crashing?
- Is new logic isolated enough to test?
- Did we avoid adding a generic abstraction before it was needed?
- Would the code still be readable on a small hardware-debugging day?
