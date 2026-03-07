#include "internal.h"

list *log_backends = NULL;

/**
 * @brief Write a log item to a backend.
 * Delegates to the backend's base write function.
 *
 * @param backend the log backend to write to
 * @param item    the log item to write
 * @return 0 on success, -1 on failure or if backend is invalid
 */
int log_backend_write(log_backend *backend, log_item *item) {
	if (!backend || !backend->base || !backend->base->write) return -1;
	return backend->base->write(backend, item);
}

/**
 * @brief Initialize a log backend.
 * Calls the backend's base init function if one is provided.
 *
 * @param backend the log backend to initialize
 * @return 0 on success or if no init function exists, -1 on failure
 */
int log_backend_init(log_backend *backend) {
	if (!backend || !backend->base || !backend->base->init) return 0;
	return backend->base->init(backend);
}

/**
 * @brief Deinitialize a log backend.
 * Calls the backend's base deinit function if one is provided.
 *
 * @param backend the log backend to deinitialize
 * @return 0 on success or if no deinit function exists, -1 on failure
 */
int log_backend_deinit(log_backend *backend) {
	if (!backend || !backend->base || !backend->base->deinit) return 0;
	return backend->base->deinit(backend);
}

/**
 * @brief Flush all buffered log items to a specific backend.
 * Iterates through all stored log items and writes unflushed ones to the
 * given backend. Also recalculates the current log size.
 *
 * @param backend the log backend to flush to
 * @param force   if true, flush all items regardless of their flushed state
 */
void log_flush_to(log_backend *backend, bool force) {
	list *l;
	log_size_cur = 0;
	if ((l = list_first(log_items))) do {
		LIST_DATA_DECLARE(item, l, log_item*);
		if (!item) continue;
		log_size_cur += item->size;
		if (item->log_flushed && !force) continue;
		if (log_backend_write(backend, item) < 0) continue;
		item->log_flushed = true;
	} while ((l = l->next));
}

/**
 * @brief Create a new log backend instance.
 * Allocates a backend, optionally allocates a context of ctx_size bytes,
 * initializes it, flushes existing log items to it, and adds it to the
 * global backend list.
 *
 * @param base   the backend base type (must not be NULL)
 * @param name   optional name for the backend (auto-generated if NULL)
 * @param config optional configuration node for the backend
 * @return the newly created log_backend, or NULL on failure
 */
log_backend* log_backend_create(
	log_backend_base *base,
	const char *name,
	confignode *config
) {
	log_backend *backend;
	if (!base) return NULL;
	if (!(backend = malloc(sizeof(log_backend)))) return NULL;
	memset(backend, 0, sizeof(log_backend));
	backend->base = base;
	backend->config = config;
	if (!name) {
		int id = (log_backends ? list_count(log_backends) : 0) + 1;
		int ret = asprintf(&backend->name, "%s-%d", base->name, id);
		if (ret < 0 || !backend->name) goto fail;
	} else  if (!(backend->name = strdup(name))) goto fail;
	if (base->ctx_size > 0) {
		backend->ctx = malloc(base->ctx_size);
		if (!backend->ctx) goto fail;
		memset(backend->ctx, 0, base->ctx_size);
	}
	if (log_backend_init(backend) < 0) goto fail;
	log_flush_to(backend, true);
	if (list_obj_add_new(&log_backends, backend) < 0) goto fail;
	log_info("log backend %s created from %s", backend->name, base->name);
	return backend;
fail:
	if (backend) log_backend_destroy(backend);
	return NULL;
}

/**
 * @brief Destroy a log backend and free all associated resources.
 * Removes the backend from the global backend list, calls its deinit
 * function, and frees the name, context, and backend structure.
 *
 * @param backend the log backend to destroy (safe to pass NULL)
 */
void log_backend_destroy(log_backend *backend) {
	if (!backend) return;
	list_obj_del_data(&log_backends, backend, NULL);
	log_backend_deinit(backend);
	if (backend->name) free(backend->name);
	if (backend->ctx && backend->base && backend->base->ctx_size > 0)
		free(backend->ctx);
	free(backend);
}

/**
 * @brief Find a registered backend base type by name.
 * Searches through the global log_backend_bases array for a matching name
 * using case-insensitive comparison.
 *
 * @param name the backend type name to search for
 * @return pointer to the matching log_backend_base, or NULL if not found
 */
log_backend_base *log_backend_base_find(const char *name) {
	int i = 0;
	log_backend_base *b;
	for (i = 0; (b = log_backend_bases[i]); i++) {
		if (!b || !b->name) continue;
		if (strcasecmp(b->name, name) == 0) return b;
	}
	return NULL;
}

/**
 * @brief Create a log backend from a configuration node.
 * Reads "backend" and "name" keys from the config map to determine the
 * backend type and instance name, then creates the backend.
 *
 * @param config configuration node (must be a CONFIGNODE_TYPE_MAP)
 * @return the newly created log_backend, or NULL on failure
 */
log_backend* log_backend_create_from(confignode *config) {
	log_backend_base *base;
	if (!config) return NULL;
	if (!confignode_is_type(config, CONFIGNODE_TYPE_MAP)) return NULL;
	char *type = confignode_path_get_string(config, "backend", NULL, NULL);
	if (!type) return NULL;
	base = log_backend_base_find(type);
	free(type);
	if (!base) return NULL;
	char *name = confignode_path_get_string(config, "name", NULL, NULL);
	log_backend *backend = log_backend_create(base, name, config);
	if (name) free(name);
	return backend;
}

/**
 * @brief Initialize log backends from a configuration node.
 * Supports both array (multiple backends) and map (single backend)
 * configuration formats.
 *
 * @param config configuration node containing backend definitions
 */
void log_backends_init(confignode *config) {
	if (!config) return;
	if (confignode_is_type(config, CONFIGNODE_TYPE_ARRAY))
		confignode_foreach(iter, config)
			if (!log_backend_create_from(iter.node))
				log_warning("failed to create log backend from config at index %d", iter.index);
	if (confignode_is_type(config, CONFIGNODE_TYPE_MAP))
		if (!log_backend_create_from(config))
			log_warning("failed to create log backend from config");
}
