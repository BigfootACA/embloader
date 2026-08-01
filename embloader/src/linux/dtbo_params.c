#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "linuxboot.h"
#include "configfile.h"
#include "log.h"
#include "str-utils.h"
#include <libfdt.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct override_target {
	uint32_t phandle;
	char *property;
	uint32_t offset;
} override_target;

static void free_override_targets(override_target *targets, int target_count) {
	if (!targets) return;
	for (int i = 0; i < target_count; i++)
		free(targets[i].property);
	free(targets);
}

static override_target* parse_override_spec(
	const fdt32_t *spec,
	int spec_len,
	int *target_count
) {
	if (!spec || spec_len < 6 || !target_count) return NULL;
	*target_count = 0;
	override_target *targets = NULL;
	const uint8_t *pos = (const uint8_t*) spec;
	const uint8_t *end = pos + spec_len;
	while (pos < end) {
		fdt32_t raw_phandle;
		if ((size_t)(end - pos) < sizeof(raw_phandle) + 2) goto fail;
		memcpy(&raw_phandle, pos, sizeof(raw_phandle));
		uint32_t phandle = fdt32_to_cpu(raw_phandle);
		pos += sizeof(raw_phandle);
		const uint8_t *prop_end = memchr(pos, 0, end - pos);
		if (!prop_end || prop_end == pos) goto fail;
		char *prop_copy = strndup((const char*)pos, prop_end - pos);
		if (!prop_copy) goto fail;
		char *colon = strchr(prop_copy, ':');
		uint32_t offset = 0;
		if (colon) {
			*colon = 0;
			char *value_end = NULL;
			errno = 0;
			unsigned long value = strtoul(colon + 1, &value_end, 0);
			if (
				!prop_copy[0] || value_end == colon + 1 || *value_end ||
				errno == ERANGE || value > UINT32_MAX
			) {
				free(prop_copy);
				goto fail;
			}
			offset = (uint32_t)value;
		}
		if (!prop_copy[0] || *target_count == INT_MAX) {
			free(prop_copy);
			goto fail;
		}
		size_t new_count = (size_t)*target_count + 1;
		if (new_count > SIZE_MAX / sizeof(override_target)) {
			free(prop_copy);
			goto fail;
		}
		override_target *new_targets = realloc(
			targets, new_count * sizeof(override_target)
		);
		if (!new_targets) {
			free(prop_copy);
			goto fail;
		}
		targets = new_targets;
		targets[*target_count].phandle = phandle;
		targets[*target_count].property = prop_copy;
		targets[*target_count].offset = offset;
		(*target_count)++;
		pos = prop_end + 1;
	}
	return targets;
fail:
	free_override_targets(targets, *target_count);
	*target_count = 0;
	return NULL;
}

static bool parse_override_value(const char *param_value, uint32_t *value) {
	if (!param_value || !value) return false;
	if (!param_value[0] || string_is_true(param_value)) {
		*value = 1;
		return true;
	}
	if (string_is_false(param_value)) {
		*value = 0;
		return true;
	}
	char *end = NULL;
	errno = 0;
	unsigned long long parsed = strtoull(param_value, &end, 0);
	if (
		end == param_value || *end || errno == ERANGE ||
		parsed > UINT32_MAX
	) return false;
	*value = (uint32_t)parsed;
	return true;
}

static bool apply_override_parameter(
	fdt fdt,
	const char *param_name,
	const char *param_value,
	override_target *targets,
	int target_count
) {
	if (!fdt || !param_name || !param_value || !targets) return false;
	uint32_t value;
	if (!parse_override_value(param_value, &value)) {
		log_warning(
			"invalid value for override %s: %s",
			param_name, param_value
		);
		return false;
	}
	uint32_t value_be = cpu_to_fdt32(value);
	log_debug(
		"applying override %s = %s (%u) to %d targets",
		param_name, param_value, value, target_count
	);
	bool success = true;
	for (int i = 0; i < target_count; i++) {
		int node = fdt_node_offset_by_phandle(fdt, targets[i].phandle);
		if (node < 0) {
			log_warning(
				"target node with phandle 0x%x not found: %s",
				targets[i].phandle, fdt_strerror(node)
			);
			success = false;
			continue;
		}
		int prop_len = -1;
		const void *prop_data = fdt_getprop(fdt, node, targets[i].property, &prop_len);
		if (!prop_data || prop_len <= 0) {
			log_warning(
				"property %s not found in target node",
				targets[i].property, fdt_strerror(prop_len)
			);
			success = false;
			continue;
		}
		uint32_t byte_offset = targets[i].offset;
		if (
			byte_offset > (uint32_t)prop_len ||
			(uint32_t)prop_len - byte_offset < sizeof(value_be)
		) {
			log_warning(
				"offset %u exceeds property %s length %d",
				targets[i].offset, targets[i].property, prop_len
			);
			success = false;
			continue;
		}
		void *new_prop = malloc(prop_len);
		if (!new_prop) {
			log_warning("failed to allocate memory for property update");
			success = false;
			continue;
		}
		memcpy(new_prop, prop_data, prop_len);
		memcpy((char*)new_prop + byte_offset, &value_be, sizeof(value_be));
		int ret = fdt_setprop(fdt, node, targets[i].property, new_prop, prop_len);
		free(new_prop);
		if (ret < 0) {
			log_warning(
				"failed to update property %s: %s",
				targets[i].property, fdt_strerror(ret)
			);
			success = false;
			continue;
		}
		log_debug(
			"updated property %s[%u] in node with phandle 0x%x",
			targets[i].property, targets[i].offset, targets[i].phandle
		);
	}
	return success;
}

/**
 * @brief Apply device tree overlay parameters by parsing __overrides__
 *
 * This function processes device tree overlay parameters by parsing the __overrides__
 * node and applying the specified parameter values to the overlay.
 *
 * @param fdt Pointer to the flattened device tree overlay (must be writable)
 * @param overrides Map-type confignode containing parameter name-value pairs
 * @return bool Returns true on success, false on failure
 */
bool linux_dtbo_write_overrides(fdt fdt, confignode *overrides) {
	if (!fdt || !overrides) return false;
	if (confignode_is_empty(overrides)) return true;
	int overrides_node = fdt_path_offset(fdt, "/__overrides__");
	if (overrides_node < 0) {
		log_warning("overrides node not found in overlay");
		return false;
	}
	bool overall_success = true;
	confignode_foreach(iter, overrides) {
		if (!confignode_is_type(iter.node, CONFIGNODE_TYPE_VALUE)) continue;
		const char *key = confignode_get_key(iter.node);
		if (!key) continue;
		bool ok = false;
		char *param_value = confignode_value_get_string(iter.node, NULL, &ok);
		if (!ok || !param_value) {
			log_warning("failed to get value for parameter %s", key);
			overall_success = false;
			continue;
		}
		int spec_len = 0;
		const fdt32_t *spec = fdt_getprop(fdt, overrides_node, key, &spec_len);
		if (!spec || spec_len < 6) {
			log_warning("override specification for %s not found or invalid", key);
			free(param_value);
			overall_success = false;
			continue;
		}
		int target_count = 0;
		override_target *targets = parse_override_spec(spec, spec_len, &target_count);
		if (!targets || target_count <= 0) {
			log_warning("failed to parse override specification for %s", key);
			free(param_value);
			overall_success = false;
			continue;
		}
		bool param_success = apply_override_parameter(
			fdt, key, param_value, targets, target_count
		);
		if (!param_success) {
			log_warning("failed to apply parameter %s", key);
			overall_success = false;
		} else log_debug("successfully applied parameter %s = %s", key, param_value);
		free_override_targets(targets, target_count);
		free(param_value);
	}
	return overall_success;
}
