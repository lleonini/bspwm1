#include <stdlib.h>
#include <math.h>
#include "types.h"
#include "tree.h"
#include "layout.h"

/* ============================== SHARED ============================== */
/* Used only by LAYOUT_TALL, LAYOUT_WIDE and LAYOUT_GRID, all of which
 * render leaves as an ordered master+stack/grid rather than by tree
 * topology. */

static int cmp_leaf_seq(const void *a, const void *b) {
	node_t *na = *(node_t * const *) a;
	node_t *nb = *(node_t * const *) b;

	if (na->insertion_seq < nb->insertion_seq) {
		return -1;
	}
	if (na->insertion_seq > nb->insertion_seq) {
		return 1;
	}
	return 0;
}

static node_t **collect_ordered_leaves(desktop_t *d, int *out_count) {
	int capacity = 16;
	int count = 0;
	node_t **leaves = malloc(capacity * sizeof(node_t *));

	if (leaves == NULL) {
		*out_count = 0;
		return NULL;
	}

	for (node_t *f = first_extrema(d->root); f != NULL; f = next_leaf(f, d->root)) {
		if (f->hidden || f->client == NULL) {
			continue;
		}
		if (count == capacity) {
			capacity *= 2;
			node_t **rleaves = realloc(leaves, capacity * sizeof(node_t *));
			if (rleaves == NULL) {
				free(leaves);
				*out_count = 0;
				return NULL;
			}
			leaves = rleaves;
		}
		leaves[count++] = f;
	}

	qsort(leaves, count, sizeof(node_t *), cmp_leaf_seq);
	*out_count = count;
	return leaves;
}

/* ============================== TALL ============================== */
/* Master column + stacked column. VARIANT_NORMAL puts master on the
 * left, VARIANT_REVERSED on the right. */

void layout_tall_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect) {
	int n_leaves;
	node_t **leaves = collect_ordered_leaves(d, &n_leaves);
	if (leaves == NULL) {
		return;
	}

	int tiled_n = 0;
	for (int i = 0; i < n_leaves; i++) {
		if (leaves[i]->vacant) {
			render_node(m, d, leaves[i], rect);
		} else {
			leaves[tiled_n++] = leaves[i];
		}
	}
	n_leaves = tiled_n;

	if (n_leaves == 0) {
		free(leaves);
		return;
	}

	unsigned int master_width = (n_leaves == 1) ? rect.width : (unsigned int) (rect.width * d->master_ratio);
	unsigned int stack_width = rect.width - master_width;

	bspwm_rect_t master_rect;
	bspwm_rect_t stack_rect;

	if (d->layout_variant == VARIANT_NORMAL) {
		master_rect = (bspwm_rect_t) {rect.x, rect.y, master_width, rect.height};
		stack_rect = (bspwm_rect_t) {(int16_t) (rect.x + master_width), rect.y, stack_width, rect.height};
	} else {
		stack_rect = (bspwm_rect_t) {rect.x, rect.y, stack_width, rect.height};
		master_rect = (bspwm_rect_t) {(int16_t) (rect.x + stack_width), rect.y, master_width, rect.height};
	}

	render_node(m, d, leaves[0], master_rect);

	int stack_n = n_leaves - 1;
	for (int i = 1; i < n_leaves; i++) {
		int si = i - 1;
		unsigned int h = stack_rect.height / stack_n;
		bspwm_rect_t r = {
			stack_rect.x,
			(int16_t) (stack_rect.y + si * h),
			stack_rect.width,
			(uint16_t) ((si == stack_n - 1) ? (stack_rect.height - si * h) : h)
		};
		render_node(m, d, leaves[i], r);
	}

	free(leaves);
}

/* ============================== WIDE ============================== */
/* Master row + stacked row. VARIANT_NORMAL puts master on top,
 * VARIANT_REVERSED on the bottom. Same shape as TALL, axes swapped. */

void layout_wide_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect) {
	int n_leaves;
	node_t **leaves = collect_ordered_leaves(d, &n_leaves);
	if (leaves == NULL) {
		return;
	}

	int tiled_n = 0;
	for (int i = 0; i < n_leaves; i++) {
		if (leaves[i]->vacant) {
			render_node(m, d, leaves[i], rect);
		} else {
			leaves[tiled_n++] = leaves[i];
		}
	}
	n_leaves = tiled_n;

	if (n_leaves == 0) {
		free(leaves);
		return;
	}

	unsigned int master_height = (n_leaves == 1) ? rect.height : (unsigned int) (rect.height * d->master_ratio);
	unsigned int stack_height = rect.height - master_height;

	bspwm_rect_t master_rect;
	bspwm_rect_t stack_rect;

	if (d->layout_variant == VARIANT_NORMAL) {
		master_rect = (bspwm_rect_t) {rect.x, rect.y, rect.width, master_height};
		stack_rect = (bspwm_rect_t) {rect.x, (int16_t) (rect.y + master_height), rect.width, stack_height};
	} else {
		stack_rect = (bspwm_rect_t) {rect.x, rect.y, rect.width, stack_height};
		master_rect = (bspwm_rect_t) {rect.x, (int16_t) (rect.y + stack_height), rect.width, master_height};
	}

	render_node(m, d, leaves[0], master_rect);

	int stack_n = n_leaves - 1;
	for (int i = 1; i < n_leaves; i++) {
		int si = i - 1;
		unsigned int w = stack_rect.width / stack_n;
		bspwm_rect_t r = {
			(int16_t) (stack_rect.x + si * w),
			stack_rect.y,
			(uint16_t) ((si == stack_n - 1) ? (stack_rect.width - si * w) : w),
			stack_rect.height
		};
		render_node(m, d, leaves[i], r);
	}

	free(leaves);
}

/* ============================== GRID ============================== */
/* Cells as close to a square arrangement as possible (cols =
 * ceil(sqrt(n)), rows = ceil(n / cols)), filled in insertion order.
 * VARIANT_NORMAL fills row-major (left to right, wrapping to the next
 * row); VARIANT_REVERSED fills column-major (top to bottom, wrapping to
 * the next column). Whichever line (row, in row-major; column, in
 * column-major) ends up short on cells because n doesn't divide evenly
 * has its cells stretched along the *other* axis so no space is left
 * empty - same "leftover stack member absorbs the remainder" approach
 * TALL/WIDE use for their stack, just applied per-line here. */

void layout_grid_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect) {
	int n_leaves;
	node_t **leaves = collect_ordered_leaves(d, &n_leaves);
	if (leaves == NULL) {
		return;
	}

	int tiled_n = 0;
	for (int i = 0; i < n_leaves; i++) {
		if (leaves[i]->vacant) {
			render_node(m, d, leaves[i], rect);
		} else {
			leaves[tiled_n++] = leaves[i];
		}
	}
	n_leaves = tiled_n;

	if (n_leaves == 0) {
		free(leaves);
		return;
	}

	bool col_major = (d->layout_variant == VARIANT_REVERSED);

	/* "per_line" = cells along the major axis before wrapping (a row's
	 * width in row-major, a column's height in column-major).
	 * "lines" = how many lines that wraps into along the minor axis. */
	int per_line = (int) ceil(sqrt((double) n_leaves));
	int lines = (int) ceil((double) n_leaves / (double) per_line);
	int last_line_count = n_leaves - (lines - 1) * per_line;

	unsigned int total_major = col_major ? rect.height : rect.width;
	unsigned int total_minor = col_major ? rect.width : rect.height;
	unsigned int line_minor = total_minor / lines;

	for (int i = 0; i < n_leaves; i++) {
		int line = i / per_line;
		int pos = i % per_line;
		bool is_last_line = (line == lines - 1);
		int line_count = is_last_line ? last_line_count : per_line;

		unsigned int cell_major = total_major / line_count;
		unsigned int major_off = pos * cell_major;
		unsigned int major_size = (pos == line_count - 1) ? (total_major - major_off) : cell_major;

		unsigned int minor_off = line * line_minor;
		unsigned int minor_size = is_last_line ? (total_minor - minor_off) : line_minor;

		bspwm_rect_t r;
		if (col_major) {
			/* major axis runs down a column (height), minor axis
			 * steps across columns (width). */
			r = (bspwm_rect_t) {
				(int16_t) (rect.x + minor_off),
				(int16_t) (rect.y + major_off),
				(uint16_t) minor_size,
				(uint16_t) major_size
			};
		} else {
			/* major axis runs along a row (width), minor axis
			 * steps down rows (height). */
			r = (bspwm_rect_t) {
				(int16_t) (rect.x + major_off),
				(int16_t) (rect.y + minor_off),
				(uint16_t) major_size,
				(uint16_t) minor_size
			};
		}

		render_node(m, d, leaves[i], r);
	}

	free(leaves);
}

/* ============================== STRINGS ============================== */
/* Canonical name for LAYOUT_TILED stays "tiled" (unlike the nex fork
 * this was ported from, which renamed it to "binary" - not renaming
 * here to avoid touching every existing caller/config that already
 * says "tiled"). */

const char *layout_str(layout_t l) {
	switch (l) {
		case LAYOUT_TILED:
			return "tiled";
		case LAYOUT_MONOCLE:
			return "monocle";
		case LAYOUT_TALL:
			return "tall";
		case LAYOUT_WIDE:
			return "wide";
		case LAYOUT_GRID:
			return "grid";
	}
	return "tiled";
}

char layout_chr(layout_t l) {
	switch (l) {
		case LAYOUT_TILED:
			return 'T';
		case LAYOUT_MONOCLE:
			return 'M';
		case LAYOUT_TALL:
			return 'L';
		case LAYOUT_WIDE:
			return 'W';
		case LAYOUT_GRID:
			return 'G';
	}
	return 'T';
}

const char *layout_variant_str(layout_variant_t v) {
	switch (v) {
		case VARIANT_NORMAL:
			return "normal";
		case VARIANT_REVERSED:
			return "reversed";
	}
	return "normal";
}
