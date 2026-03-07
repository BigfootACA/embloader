#include "internal.h"

list *log_items = NULL;
size_t log_size_limit = 1024 * 1024;
size_t log_size_cur = 0;

static void log_size_limit_check() {
	list *l;
	l = list_first(log_items);
	while (log_size_cur > log_size_limit && log_items && l) {
		if (!l) break;
		LIST_DATA_DECLARE(item, l, log_item*);
		if (item) {
			log_size_cur -= item->size;
			list_obj_del(&log_items, l, list_default_free);
		}
		l = list_first(log_items);
	}
}

/**
 * @brief Append a log item to the log store.
 * Adds the item to the global log_items list and enforces the size limit
 * by evicting oldest items if necessary.
 *
 * @param item the log item to append (must not be NULL, size must be < log_size_limit)
 * @return true on success, false if item is NULL, too large, or allocation fails
 */
bool log_append(struct log_item *item) {
	int ret;
	if (!item || item->size >= log_size_limit) return false;
	log_size_limit_check();
	item->log_flushed = false;
	ret = list_obj_add_new(&log_items, item);
	if (ret < 0) return false;
	log_size_cur += item->size;
	log_size_limit_check();
	return true;
}

/**
 * @brief Flush a single log item to a specific backend.
 * Writes the item to the backend and marks it as flushed.
 *
 * @param backend the log backend to write to
 * @param item    the log item to flush (safe to pass NULL)
 * @param force   if true, flush even if already marked as flushed
 */
void log_flush_one_to(log_backend *backend, log_item *item, bool force) {
	if (!item) return;
	if (item->log_flushed && !force) return;
	log_backend_write(backend, item);
	item->log_flushed = true;
}

/**
 * @brief Flush all log items to all registered backends.
 * Iterates through every log item and writes unflushed ones to all backends.
 * Also recalculates the current log size.
 *
 * @param force if true, flush all items regardless of their flushed state
 */
void log_flush_all(bool force) {
	list *b, *i;
	log_size_cur = 0;
	if ((i = list_first(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (!item) continue;
		log_size_cur += item->size;
		if (item->log_flushed && !force) continue;
		if ((b = list_first(log_backends))) do {
			LIST_DATA_DECLARE(backend, b, log_backend*);
			if (log_backend_write(backend, item) < 0) continue; 
			item->log_flushed = true;
		} while ((b = b->next));
	} while ((i = i->next));
}

/**
 * @brief Fast-path flush of recently added log items to all backends.
 * Scans from the tail of the log list to find the first batch of unflushed
 * items, then writes only those items to all registered backends. This is
 * more efficient than log_flush_all for the common case of flushing newly
 * appended items.
 */
void log_flush_fast() {
	list *b, *i, *s = NULL;
	if ((i = list_last(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (!item) continue;
		if (item->log_flushed) break;
		s = i;
	} while ((i = i->prev));
	if (!s) s = list_first(log_items);
	if (!s) return;
	i = s;
	do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (!item) continue;
		if (item->log_flushed) break;
		if ((b = list_first(log_backends))) do {
			LIST_DATA_DECLARE(backend, b, log_backend*);
			if (log_backend_write(backend, item) < 0) continue; 
			item->log_flushed = true;
		} while ((b = b->next));
	} while ((i = i->next));
}
