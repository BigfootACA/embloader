#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "lvgl.h"
#include "log.h"
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "str-utils.h"

#define image_cache_draw_buf_handlers &(LV_GLOBAL_DEFAULT()->image_cache_draw_buf_handlers)

static const char* data_from_dsc(lv_image_decoder_dsc_t* dsc) {
	if (!dsc || dsc->src_type != LV_IMAGE_SRC_VARIABLE) return NULL;
	const lv_image_dsc_t* img_dsc = dsc->src;
	if (!img_dsc || !img_dsc->data) return NULL;
	return (const char*) img_dsc->data;
}

static bool is_svg(const char* data) {
	return startwiths(data, "<svg") || startwiths(data, "<?xml");
}

static lv_result_t nanosvg_info(lv_image_decoder_t* decoder, lv_image_decoder_dsc_t* dsc, lv_image_header_t* header) {
	if (!decoder || !dsc || !header) return LV_RESULT_INVALID;
	if (dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;
	if (!is_svg(data_from_dsc(dsc))) return LV_RESULT_INVALID;
	const lv_image_dsc_t* img_dsc = dsc->src;
	if (!img_dsc || !img_dsc->data) return LV_RESULT_INVALID;
	header->cf = LV_COLOR_FORMAT_ARGB8888;
	header->w = img_dsc->header.w;
	header->h = img_dsc->header.h;
	header->stride = img_dsc->header.w * sizeof(lv_color_t);
	return LV_RESULT_OK;
}

static lv_draw_buf_t* svg_decode(const lv_image_dsc_t* src) {
	lv_draw_buf_t *buf = NULL;
	NSVGrasterizer *r = NULL;
	NSVGimage *m = NULL;
	float fdpi = (float) lv_display_get_dpi(NULL);
	if (!(m = nsvgParse((char*)src->data, "px", fdpi))) {
		LV_LOG_WARN("failed to parse svg");
		goto fail;
	}
	if (!(r = nsvgCreateRasterizer())){
		LV_LOG_WARN("failed to create rasterizer");
		goto fail;
	}
	float scale = 1.0f;
	if (src->header.w != 0 && src->header.h != 0) scale = MIN(
		(float) src->header.w / (float) m->width,
		(float) src->header.h / (float) m->height
	);
	size_t rw = m->width * scale, rh = m->height * scale;
	size_t rs = rw * sizeof(lv_color32_t);
	if (!(buf = lv_draw_buf_create_ex(
		image_cache_draw_buf_handlers,
		rw,rh,LV_COLOR_FORMAT_ARGB8888,rs
	))) {
		LV_LOG_WARN("failed to allocate svg image");
		goto fail;
	}
	if (buf->data_size != rh * rs) {
		LV_LOG_WARN("data size mismatch");
		goto fail;
	}
	if (!buf->data) {
		LV_LOG_WARN("failed to allocate svg image");
		goto fail;
	}
	nsvgRasterize(r, m, 0, 0, scale, buf->data, rw, rh, rs);
	lv_color32_t* p = (lv_color32_t*) buf->data;
	for (size_t i = 0; i < rw * rh; i++) {
		uint32_t old = p[i].blue;
		p[i].blue = p[i].red;
		p[i].red = old;
	}
	nsvgDeleteRasterizer(r);
	nsvgDelete(m);
	return buf;
	fail:
	if (buf) lv_draw_buf_destroy(buf);
	if (r) nsvgDeleteRasterizer(r);
	if (m) nsvgDelete(m);
	return NULL;
}

static lv_result_t nanosvg_open(
	lv_image_decoder_t* decoder,
	lv_image_decoder_dsc_t* dsc
) {
	lv_draw_buf_t *decoded = NULL, *adjusted = NULL;
	if (!decoder || !dsc) return LV_RESULT_INVALID;
	if (dsc->src_type != LV_IMAGE_SRC_VARIABLE) return LV_RESULT_INVALID;
	const lv_image_dsc_t *img_dsc = dsc->src;
	if (!img_dsc || !img_dsc->data) return LV_RESULT_INVALID;
	if (!is_svg(data_from_dsc(dsc))) return LV_RESULT_INVALID;
	lv_draw_buf_t* buf = NULL;
	if (!(decoded = svg_decode(img_dsc))) {
		LV_LOG_WARN("failed to decode svg");
		goto fail;
	}
	if (!(adjusted = lv_image_decoder_post_process(dsc, decoded))) {
		LV_LOG_WARN("failed to post process svg");
		goto fail;
	}
	if (decoded != adjusted)
		lv_draw_buf_destroy(decoded);
	decoded = NULL;
	buf = adjusted, dsc->decoded = buf;
	if (dsc->args.no_cache || !lv_image_cache_is_enabled())
		return LV_RESULT_OK;
	lv_image_cache_data_t search_key;
	search_key.src_type = dsc->src_type;
	search_key.src = dsc->src;
	search_key.slot.size = dsc->decoded->data_size;
	lv_cache_entry_t *entry = lv_image_decoder_add_to_cache(
		decoder, &search_key, dsc->decoded, NULL
	);
	if (!entry) {
		lv_draw_buf_destroy(buf);
		dsc->decoded = NULL;
		return LV_RESULT_INVALID;
	}
	dsc->cache_entry = entry;
	return LV_RESULT_OK;
fail:
	if (decoded) lv_draw_buf_destroy(decoded);
	if (adjusted) lv_draw_buf_destroy(adjusted);
	return LV_RESULT_INVALID;
}

static void nanosvg_close(
	lv_image_decoder_t* decoder,
	lv_image_decoder_dsc_t* dsc
) {
	if (dsc->args.no_cache || !lv_image_cache_is_enabled())
		lv_draw_buf_destroy((lv_draw_buf_t*) dsc->decoded);
}

static lv_image_decoder_t nanosvg_decoder = {
	.info_cb = nanosvg_info,
	.open_cb = nanosvg_open,
	.close_cb = nanosvg_close,
	.name = "nanosvg",
};

lv_image_decoder_t* lv_nanosvg_init() {
	lv_image_decoder_t* ret = lv_image_decoder_create();
	if (!ret) return NULL;
	memcpy(ret, &nanosvg_decoder, sizeof(lv_image_decoder_t));
	return ret;
}
