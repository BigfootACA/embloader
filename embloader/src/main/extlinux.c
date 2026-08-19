#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "bootmenu.h"
#include "embloader.h"
#include "efi-utils.h"
#include "file-utils.h"
#include "log.h"
#include "str-utils.h"

#define EXTLINUX_MAX_CONFIG_SIZE (1024 * 1024)

struct extlinux_label {
	char *label;
	char *name;
};

struct extlinux_parser {
	confignode *entries;
	list *labels;
	confignode *current;
	char *current_label;
	char *default_label;
	char *ontimeout;
	char *fdt_dir;
	char *menu_title;
	int timeout;
	bool timeout_set;
	bool prompt_set;
	bool prompt;
};

static int extlinux_label_free(void *data) {
	struct extlinux_label *label = data;
	if (!label) return 0;
	free(label->label);
	free(label->name);
	free(label);
	return 0;
}

static void extlinux_parser_clean(struct extlinux_parser *parser) {
	if (!parser) return;
	if (parser->entries) confignode_clean(parser->entries);
	if (parser->current) confignode_clean(parser->current);
	if (parser->labels) list_free_all(parser->labels, extlinux_label_free);
	free(parser->current_label);
	free(parser->default_label);
	free(parser->ontimeout);
	free(parser->fdt_dir);
	free(parser->menu_title);
	memset(parser, 0, sizeof(*parser));
}

static char *extlinux_trim(char *value) {
	char *end;
	if (!value) return NULL;
	while (*value && isspace((unsigned char)*value)) value++;
	end = value + strlen(value);
	while (end > value && isspace((unsigned char)end[-1])) end--;
	*end = 0;
	return value;
}

static bool extlinux_set_string(confignode *node, const char *path, const char *value) {
	return node && path && value && confignode_path_set_string(node, path, value);
}

static char *extlinux_keyword_value(char *value, const char *keyword) {
	size_t length;
	if (!value || !keyword) return NULL;
	length = strlen(keyword);
	if (strncasecmp(value, keyword, length) != 0) return NULL;
	if (value[length] && !isspace((unsigned char)value[length])) return NULL;
	return extlinux_trim(value + length);
}

static bool extlinux_is_absolute_path(const char *path) {
	return path && (path[0] == '/' || path[0] == '\\');
}

static char *extlinux_join_path(const char *directory, const char *path) {
	const char *separator;
	char *joined = NULL;
	if (!directory || !path) return NULL;
	separator = endwith(directory, '/') || endwith(directory, '\\') ? "" : "/";
	if (asprintf(&joined, "%s%s%s", directory, separator, path) < 0)
		return NULL;
	return joined;
}

static bool extlinux_append_values(
	confignode *node,
	const char *path,
	const char *value
) {
	char *copy, *cursor, *token;
	bool added = false;
	if (!node || !path || !value) return false;
	if (!(copy = strdup(value))) return false;
	for (cursor = copy; *cursor;) {
		while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ',')) cursor++;
		if (!*cursor) break;
		token = cursor;
		while (*cursor && !isspace((unsigned char)*cursor) && *cursor != ',') cursor++;
		if (*cursor) *cursor++ = 0;
		confignode *item = confignode_new_string(token);
		if (!item || !confignode_path_array_append(node, path, item)) {
			if (item) confignode_clean(item);
			free(copy);
			return false;
		}
		added = true;
	}
	free(copy);
	return added;
}

static bool extlinux_name_exists(struct extlinux_parser *parser, const char *name) {
	list *p;
	if (!parser || !name) return false;
	if (embloader_find_loader(name)) return true;
	if ((p = list_first(parser->labels))) do {
		LIST_DATA_DECLARE(label, p, struct extlinux_label*);
		if (label && label->name && strcasecmp(label->name, name) == 0) return true;
	} while ((p = p->next));
	return false;
}

static bool extlinux_finish_label(struct extlinux_parser *parser) {
	struct extlinux_label *label;
	char *name = NULL;
	char *kernel;
	if (!parser || !parser->current) return true;
	if (!parser->current_label) return false;
	kernel = confignode_path_get_string(parser->current, "kernel", NULL, NULL);
	if (!kernel || !kernel[0]) {
		free(kernel);
		confignode_clean(parser->current);
		parser->current = NULL;
		free(parser->current_label);
		parser->current_label = NULL;
		return true;
	}
	free(kernel);
	if (asprintf(&name, "extlinux-%s", parser->current_label) < 0 || !name)
		return false;
	if (extlinux_name_exists(parser, name)) {
		char *base = name;
		unsigned int suffix = 2;
		name = NULL;
		while (true) {
			if (asprintf(&name, "%s-%u", base, suffix++) < 0 || !name) {
				free(base);
				return false;
			}
			if (!extlinux_name_exists(parser, name)) break;
			free(name);
		}
		free(base);
	}
	label = calloc(1, sizeof(*label));
	if (!label) {
		free(name);
		return false;
	}
	label->label = strdup(parser->current_label);
	label->name = name;
	if (!label->label || !confignode_map_set(parser->entries, name, parser->current)) {
		extlinux_label_free(label);
		return false;
	}
	parser->current = NULL;
	free(parser->current_label);
	parser->current_label = NULL;
	if (list_obj_add_new(&parser->labels, label) != 0) {
		extlinux_label_free(label);
		return false;
	}
	return true;
}

static const char *extlinux_find_name(struct extlinux_parser *parser, const char *label) {
	list *p;
	if (!parser || !label) return NULL;
	if ((p = list_first(parser->labels))) do {
		LIST_DATA_DECLARE(item, p, struct extlinux_label*);
		if (item && item->label && strcasecmp(item->label, label) == 0)
			return item->name;
	} while ((p = p->next));
	return NULL;
}

static bool extlinux_parse_timeout(const char *value, int *timeout) {
	char *end = NULL;
	unsigned long long deciseconds;
	if (!value || !timeout || !value[0]) return false;
	errno = 0;
	deciseconds = strtoull(value, &end, 10);
	if (errno == ERANGE || !end || *end || deciseconds > UINT64_MAX - 9) return false;
	deciseconds = (deciseconds + 9) / 10;
	if (deciseconds > INT_MAX) *timeout = INT_MAX;
	else *timeout = (int) deciseconds;
	return true;
}

static bool extlinux_parse_line(struct extlinux_parser *parser, char *line) {
	char *key, *value, *split;
	size_t line_length;
	if (!parser || !line) return false;
	line = extlinux_trim(line);
	line_length = strlen(line);
	if (
		line_length >= 3 &&
		(unsigned char)line[0] == 0xef &&
		(unsigned char)line[1] == 0xbb &&
		(unsigned char)line[2] == 0xbf
	) line = extlinux_trim(line + 3);
	if (!line[0] || line[0] == '#') return true;
	split = line;
	while (*split && !isspace((unsigned char)*split)) split++;
	if (!*split) value = split;
	else {
		*split++ = 0;
		value = extlinux_trim(split);
	}
	key = line;
	if (strcasecmp(key, "LABEL") == 0) {
		if (!value[0]) return false;
		if (!extlinux_finish_label(parser)) return false;
		parser->current = confignode_new_map();
		if (!parser->current) return false;
		parser->current_label = strdup(value);
		if (!parser->current_label ||
			!extlinux_set_string(parser->current, "type", "linux") ||
			!extlinux_set_string(parser->current, "title", value)) return false;
		return true;
	}
	if (strcasecmp(key, "MENU") == 0) {
		char *option = extlinux_keyword_value(value, "LABEL");
		if (option) {
			return parser->current && option[0] &&
				extlinux_set_string(parser->current, "title", option);
		}
		if (strcasecmp(value, "DEFAULT") == 0) {
			if (!parser->current || !parser->current_label) return false;
			free(parser->default_label);
			parser->default_label = strdup(parser->current_label);
			return parser->default_label != NULL;
		}
		if (strcasecmp(value, "HIDDEN") == 0) {
			parser->prompt_set = true;
			parser->prompt = false;
			return true;
		}
		option = extlinux_keyword_value(value, "TITLE");
		if (option) {
			free(parser->menu_title);
			parser->menu_title = option[0] ? strdup(option) : NULL;
			return !option[0] || parser->menu_title != NULL;
		}
		log_warning("unsupported extlinux MENU option: %s", value);
		return true;
	}
	if (strcasecmp(key, "DEFAULT") == 0 || strcasecmp(key, "ONTIMEOUT") == 0) {
		char **target = strcasecmp(key, "DEFAULT") == 0 ?
			&parser->default_label : &parser->ontimeout;
		if (!value[0]) return false;
		free(*target);
		*target = strdup(value);
		return *target != NULL;
	}
	if (strcasecmp(key, "TIMEOUT") == 0) {
		parser->timeout_set = extlinux_parse_timeout(value, &parser->timeout);
		return parser->timeout_set;
	}
	if (strcasecmp(key, "PROMPT") == 0) {
		if (!value[0] || (strcmp(value, "0") != 0 && strcmp(value, "1") != 0)) return false;
		parser->prompt_set = true;
		parser->prompt = value[0] == '1';
		return true;
	}
	if (strcasecmp(key, "FDTDIR") == 0) {
		if (!value[0]) return false;
		char *directory = strdup(value);
		if (!directory) return false;
		free(parser->fdt_dir);
		parser->fdt_dir = directory;
		return true;
	}
	if (!parser->current) {
		log_warning("unsupported extlinux global option: %s", key);
		return true;
	}
	if (strcasecmp(key, "LINUX") == 0 || strcasecmp(key, "KERNEL") == 0)
		return value[0] && extlinux_set_string(parser->current, "kernel", value);
	if (strcasecmp(key, "INITRD") == 0)
		return value[0] && extlinux_append_values(parser->current, "initramfs", value);
	if (strcasecmp(key, "APPEND") == 0)
		return extlinux_set_string(parser->current, "bootargs", strcmp(value, "-") == 0 ? "" : value);
	if (strcasecmp(key, "FDT") == 0)
	{
		char *path = NULL;
		if (!value[0]) return false;
		if (parser->fdt_dir && !extlinux_is_absolute_path(value))
			path = extlinux_join_path(parser->fdt_dir, value);
		else path = strdup(value);
		if (!path) return false;
		bool result = extlinux_set_string(parser->current, "devicetree", path);
		free(path);
		return result;
	}
	if (strcasecmp(key, "FDTOVERLAYS") == 0)
		return value[0] && extlinux_append_values(parser->current, "overlays", value);
	log_warning("unsupported extlinux entry option: %s", key);
	return true;
}

static bool extlinux_parse_content(struct extlinux_parser *parser, char *content) {
	char *line = content;
	if (!parser || !content) return false;
	while (line && *line) {
		char *next = strchr(line, '\n');
		if (next) *next++ = 0;
		if (!extlinux_parse_line(parser, line)) return false;
		line = next;
	}
	return extlinux_finish_label(parser);
}

static EFI_STATUS extlinux_read_file(
	EFI_FILE_PROTOCOL *root,
	const char *path,
	void **buffer,
	size_t *length
) {
	EFI_FILE_PROTOCOL *file = NULL;
	EFI_STATUS status;
	if (!root || !path || !buffer || !length) return EFI_INVALID_PARAMETER;
	*buffer = NULL;
	*length = 0;
	status = efi_open(root, &file, path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status) || !file) return status;
	status = efi_file_get_size(file, length);
	if (!EFI_ERROR(status) && *length > EXTLINUX_MAX_CONFIG_SIZE)
		status = EFI_BAD_BUFFER_SIZE;
	if (!EFI_ERROR(status)) status = efi_file_read_all(file, buffer, length);
	file->Close(file);
	return status;
}

static bool extlinux_publish(struct extlinux_parser *parser) {
	char *default_entry = NULL;
	char *menu_title = NULL;
	const char *name;
	if (!parser || !parser->entries) return false;
	name = extlinux_find_name(parser, parser->default_label);
	if (!name) name = extlinux_find_name(parser, parser->ontimeout);
	if (!name && parser->labels) {
		struct extlinux_label *first = list_first(parser->labels)->data;
		name = first ? first->name : NULL;
	}
	if (name && !confignode_path_lookup(g_embloader.config, "menu.default", false)) {
		default_entry = strdup(name);
		if (!default_entry) return false;
	}
	if (parser->menu_title && !confignode_path_lookup(g_embloader.config, "menu.title", false)) {
		menu_title = strdup(parser->menu_title);
		if (!menu_title) {
			free(default_entry);
			return false;
		}
	}
	if (!confignode_path_map_set(
		g_embloader.config, "extlinux.entries", parser->entries
	)) {
		free(default_entry);
		free(menu_title);
		return false;
	}
	parser->entries = NULL;
	confignode_path_foreach(iter, g_embloader.config, "extlinux.entries")
		embloader_load_loader(iter.node);
	if (default_entry) {
		free(g_embloader.menu->default_entry);
		g_embloader.menu->default_entry = default_entry;
	}
	if (parser->timeout_set && !confignode_path_lookup(g_embloader.config, "menu.timeout", false))
		g_embloader.menu->timeout = parser->timeout;
	if (parser->prompt_set) g_embloader.menu->disabled = !parser->prompt;
	if (menu_title) {
		free(g_embloader.menu->title);
		g_embloader.menu->title = menu_title;
	}
	embloader_sort_menu_loaders();
	return true;
}

EFI_STATUS embloader_extlinux_load_menu(void) {
	static bool loaded;
	static const char *paths[] = {
		"/extlinux/extlinux.conf",
		"/boot/extlinux/extlinux.conf",
		NULL,
	};
	struct extlinux_parser parser = {0};
	void *buffer = NULL;
	size_t length = 0;
	EFI_STATUS status = EFI_NOT_FOUND;
	if (!g_embloader.dir.root || !g_embloader.menu) return EFI_NOT_READY;
	if (loaded) return EFI_ALREADY_STARTED;
	if (!confignode_path_get_bool(
		g_embloader.config, "menu.extlinux.enabled", true, NULL
	)) return EFI_UNSUPPORTED;
	parser.entries = confignode_new_map();
	if (!parser.entries) return EFI_OUT_OF_RESOURCES;
	for (size_t i = 0; paths[i]; i++) {
		if (!efi_file_exists(g_embloader.dir.root, paths[i])) continue;
		status = extlinux_read_file(
			g_embloader.dir.root, paths[i], &buffer, &length
		);
		if (!EFI_ERROR(status)) break;
	}
	if (EFI_ERROR(status)) goto done;
	if (!length || !extlinux_parse_content(&parser, buffer)) {
		status = EFI_LOAD_ERROR;
		goto done;
	}
	if (!parser.labels) {
		status = EFI_NOT_FOUND;
		goto done;
	}
	status = extlinux_publish(&parser) ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
	if (!EFI_ERROR(status)) loaded = true;
done:
	free(buffer);
	extlinux_parser_clean(&parser);
	if (status == EFI_NOT_FOUND) log_debug("no extlinux.conf found");
	else if (EFI_ERROR(status)) log_warning("failed to load extlinux.conf: %s", efi_status_to_string(status));
	return status;
}
