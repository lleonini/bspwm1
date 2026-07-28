#ifndef BSPWM_LAYOUT_H
#define BSPWM_LAYOUT_H

#include "types.h"

/* LAYOUT_TILED and LAYOUT_MONOCLE stay on tree.c's apply_layout() (tree
 * topology drives geometry directly). LAYOUT_TALL/WIDE/GRID render
 * leaves as an ordered master+stack or grid instead, and own their
 * geometry math here. None of these share state with each other. */

void layout_tall_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect);
void layout_wide_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect);
void layout_grid_arrange(monitor_t *m, desktop_t *d, bspwm_rect_t rect);

/* String/char representations, used by query.c (JSON), desktop.c (status
 * events) and subscribe.c (status bar feed). Kept here instead of a macro
 * in helpers.h since they need layout_t, which isn't visible that early. */
const char *layout_str(layout_t l);
char layout_chr(layout_t l);
const char *layout_variant_str(layout_variant_t v);

#endif
