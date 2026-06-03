#include "app_review.h"

bool app_review_should_show_queue(
	enum review_state_load_result state_load_result,
	const struct scheduler_session *session
)
{
	if (!review_state_load_result_allows_save(state_load_result))
		return false;
	if (session == NULL)
		return false;

	return !scheduler_is_complete(session);
}
