#include "internal.h"

list *log_items = NULL;
size_t log_size_limit = 1024 * 1024;
size_t log_size_cur = 0;

static int log_item_free(void *data) {
	log_item *item = data;
	if (!item) return 0;
	if (item->flushed_backends)
		list_free_all(item->flushed_backends, NULL);
	free(item);
	return 0;
}

static bool log_item_was_flushed_to(log_item *item, log_backend *backend) {
	list *l;
	if (!item || !backend) return false;
	if ((l = list_first(item->flushed_backends))) do {
		if (l->data == backend) return true;
	} while ((l = l->next));
	return false;
}

static bool log_item_is_fully_flushed(log_item *item) {
	list *b;
	if (!item || !(b = list_first(log_backends))) return false;
	do {
		LIST_DATA_DECLARE(backend, b, log_backend*);
		if (!log_item_was_flushed_to(item, backend)) return false;
	} while ((b = b->next));
	return true;
}

static int log_item_write_to(log_backend *backend, log_item *item, bool force) {
	bool marked;
	int ret;
	if (!backend || !item) return -1;
	marked = log_item_was_flushed_to(item, backend);
	if (marked && !force) return 0;
	if (!marked && list_obj_add_new(&item->flushed_backends, backend) < 0)
		return -1;
	ret = log_backend_write(backend, item);
	if (ret < 0 && !marked)
		list_obj_del_data(&item->flushed_backends, backend, NULL);
	return ret;
}

void log_item_invalidate_flush_state() {
	list *i;
	if ((i = list_first(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (item) item->log_flushed = false;
	} while ((i = i->next));
}

void log_item_forget_backend(log_backend *backend) {
	list *i;
	if (!backend) return;
	if ((i = list_first(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (!item) continue;
		if (item->flushed_backends)
			list_obj_del_data(&item->flushed_backends, backend, NULL);
		item->log_flushed = log_item_is_fully_flushed(item);
	} while ((i = i->next));
}

static void log_size_limit_check() {
	list *l;
	l = list_first(log_items);
	while (log_size_cur > log_size_limit && log_items && l) {
		if (!l) break;
		LIST_DATA_DECLARE(item, l, log_item*);
		if (item) {
			log_size_cur -= item->size;
			list_obj_del(&log_items, l, log_item_free);
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
	if (!item) return false;
	log_size_limit_check();
	if (item->size >= log_size_limit) return false;
	item->log_flushed = false;
	ret = list_obj_add_new(&log_items, item);
	if (ret < 0) return false;
	log_size_cur += item->size;
	log_size_limit_check();
	return true;
}

/**
 * @brief Flush a single log item to a specific backend.
 * Writes the item to the backend and records delivery for that backend.
 *
 * @param backend the log backend to write to
 * @param item    the log item to flush (safe to pass NULL)
 * @param force   if true, flush even if already marked as flushed
 * @return the backend write result, or -1 for invalid input or tracking failure
 */
int log_flush_one_to(log_backend *backend, log_item *item, bool force) {
	int ret;
	if (!item) return -1;
	ret = log_item_write_to(backend, item, force);
	item->log_flushed = log_item_is_fully_flushed(item);
	return ret;
}

/**
 * @brief Flush all log items to all registered backends.
 * Iterates through every backend and delivers entries in order, independently
 * retaining progress for each backend. Also recalculates the current log size.
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
	} while ((i = i->next));
	if ((b = list_first(log_backends))) do {
		LIST_DATA_DECLARE(backend, b, log_backend*);
		if ((i = list_first(log_items))) do {
			LIST_DATA_DECLARE(item, i, log_item*);
			if (!item) continue;
			if (log_item_write_to(backend, item, force) < 0) break;
		} while ((i = i->next));
	} while ((b = b->next));
	if ((i = list_first(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (item) item->log_flushed = log_item_is_fully_flushed(item);
	} while ((i = i->next));
}

/**
 * @brief Fast-path flush of recently added log items to all backends.
 * Scans from the tail to find the unflushed suffix, then advances each backend
 * from its own recorded position without duplicating successful deliveries.
 */
void log_flush_fast() {
	list *b, *i, *s = NULL;
	if ((i = list_last(log_items))) do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (!item) continue;
		if (item->log_flushed) break;
		s = i;
	} while ((i = i->prev));
	if (!s) return;
	if ((b = list_first(log_backends))) do {
		LIST_DATA_DECLARE(backend, b, log_backend*);
		i = s;
		do {
			LIST_DATA_DECLARE(item, i, log_item*);
			if (!item) continue;
			if (log_item_write_to(backend, item, false) < 0) break;
		} while ((i = i->next));
	} while ((b = b->next));
	i = s;
	do {
		LIST_DATA_DECLARE(item, i, log_item*);
		if (item) item->log_flushed = log_item_is_fully_flushed(item);
	} while ((i = i->next));
}
