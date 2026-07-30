# Layouts

Ported from the [nex](https://github.com/dream-wa1ker/nex) fork onto the
`nex` branch of this repo (see `git log --oneline master..nex` for the
commit-by-commit port). Adds three layouts on top of the original
`tiled`/`monocle`: `tall`, `wide`, `grid`.

## Layouts

| Layout    | Description |
|-----------|--------------|
| `tiled`   | Original BSP tree-split layout. Unchanged. |
| `monocle` | One window full-screen at a time. Unchanged. |
| `tall`    | Master column + stacked column (dwm/xmonad style). |
| `wide`    | Master row + stacked row. Same shape as `tall`, axes swapped. |
| `grid`    | Windows arranged in a near-square grid, filled in creation order. |

`tall`/`wide`/`grid` order windows by **creation order** (`insertion_seq`),
not by tree position — the oldest surviving window on the desktop is
always the master / first cell, regardless of where it sits in the
underlying BSP tree.

## Switching layout

```
bspc desktop -l tiled      # or monocle / tall / wide / grid
bspc desktop -l next       # cycle forward through all 5 layouts
bspc desktop -l prev       # cycle backward
```

Bind these in `sxhkdrc` like any other `bspc` command, e.g.:

```
super + t
	bspc desktop -l tall

super + shift + t
	bspc desktop -l next
```

### Toggling back to the previous layout

Same `~` convention as `bspc node -t '~floating'`: each desktop remembers
the layout you were on before your last change, in `last_layout`.

```
bspc desktop -l ~          # recall the previous layout, whatever it was
bspc desktop -l ~tall      # go to tall, unless tall is already active -
                            # in which case go back to whatever was
                            # active before tall
```

`~<layout>` is the useful one for a single keybind that both "enters" and
"exits" a layout with the same key, e.g.:

```
super + m
	bspc desktop -l ~monocle
```

pressing it switches to `monocle`; pressing it again while already in
`monocle` returns you to whatever you were in before (not necessarily
`tiled` — whatever it actually was).

Note `last_layout` only updates on an explicit user-requested layout
change (`bspc desktop -l ...`) — it's untouched by `single_monocle`
auto-switching the effective `layout` behind the scenes, so `~` always
recalls a real previous choice, never a transient auto-monocle state.

**Scope**: `~` only means something on `-l`/`--layout`'s argument. It has
no effect anywhere else in `bspc desktop` - not on `-f`, `-n`, `-m`, `-s`,
`-b`, `-r`, nor on the leading `<DESKTOP_SEL>`. (`bspc node -t
'~floating'` is a separate, pre-existing bspwm convention on `cmd_node`'s
`-t`/`--state`, unrelated to this one beyond sharing the `~` idea.)

Also note `~<cycle-direction>` (e.g. `bspc desktop -l ~next`) isn't
handled specially - the `~` is silently ignored and it behaves exactly
like plain `next`. Stick to `~` alone or `~<layout>`.

## The master window

`tall`/`wide`/`grid` all have a "master" concept: the oldest window on the
desktop. It's addressable directly with a dedicated selector, in any
layout (not just tall/wide/grid), instead of guessing a direction:

```
bspc node -f master        # focus the master window
bspc node -s master        # swap focused window with the master
bspc node master -s focused
```

`-s`/`--swap` also exchanges the two nodes' creation order, so swapping
with the master always visibly swaps who's in the master slot — it's not
just a tree-topology swap that TALL/WIDE/GRID would otherwise ignore.

### `master` vs. "the first window in the tree"

These are not the same thing, and `master` isn't a shorthand for a
tree-position selector that already existed - there wasn't one.

`master` resolves via `insertion_seq` (`find_master` in `tree.c`): a
creation-order id attached to each node, independent of where it
currently sits in the BSP tree. The first leaf in tree order
(`first_extrema`) instead reflects the *current topology* - which drifts
away from creation order after any `bspc node -s`/`-m` in `tiled` (a
swap there only exchanges tree position, not `insertion_seq`), manual
presel insertion, or repeated resizing/moving. In `tall`/`wide`/`grid`
this matters a lot, since their rendering never looks at tree position at
all (`layout.c` sorts leaves by `insertion_seq`, full stop) - a
tree-position selector would frequently point at a window that isn't even
visually in the master slot. In `tiled`/`monocle`, bspwm never had an
equivalent selector to begin with (the closest, the `@/first` path
syntax, moves one step down the tree per component, can land on an
internal branch node instead of a leaf, and is just as
topology-dependent). `master` is a genuinely new, layout-independent
concept - not a redundant alias for something that already worked.

## Master side / variant

Two ways to set it, depending on whether you want a toggle or an
absolute value.

**`bspc node -F`** — unchanged from stock bspwm: flips tree topology,
repurposed as a `layout_variant` **toggle** for tall/wide/grid (no tree
to flip there):

```
bspc node -F horizontal    # tall: master left <-> right
bspc node -F vertical      # wide: master top <-> bottom
                            # grid: row-major <-> column-major fill
```

Any focused node works as the target. Being a toggle, calling it twice
undoes itself — fine for a single keybind you press repeatedly, less so
if you want a specific, predictable side every time.

**`bspc desktop -F left|right`** — sets `layout_variant` to an absolute
value instead of toggling it, so it's idempotent and combines cleanly
with `-l` in one call:

```
bspc desktop -l tall -F right    # always lands on master-right,
                                  # no matter how many times you run it
```

`left` → `normal`, `right` → `reversed`. Same restriction as the toggle:
fails with an explicit error on `tiled`/`monocle`, which have no
`layout_variant` to set.

## Master ratio

The master/stack split is one ratio per desktop (`master_ratio`), separate
from `tiled`'s per-node `split_ratio`.

```
bspc config master_ratio 0.6              # global default, applies now + future desktops
bspc config -d focused master_ratio 0.6   # this desktop only

bspc node -r 0.6      # absolute, tall/wide only (dispatches to master_ratio automatically)
bspc node -r +0.05    # relative: grow master by 5%
bspc node -r -0.05    # shrink master by 5%
```

`bspc node -r` auto-detects whether the focused desktop is `tall`/`wide`
and adjusts `master_ratio` instead of the node's `split_ratio` — same
command as in `tiled`, no separate syntax to remember.

Mouse-drag resize (dragging a window edge) also works on the master/stack
boundary in `tall`/`wide`. Grabbing any other edge (the outer edge of the
monitor) is a no-op, since there's nothing to push against. `monocle` and
`grid` have no adjustable boundary at all — resize is refused there.

## Selectors

```
bspc query -D -d .tall            # desktops currently in tall layout
bspc query -D -d .user_wide       # desktops whose *user* layout is wide
                                   # (differs from .wide when single_monocle
                                   #  auto-switched the effective layout)
```

Available: `.tiled` `.monocle` `.tall` `.wide` `.grid`, and their
`.user_*` equivalents.

## Inspecting state

```
bspc query -T -d
```

JSON includes `"layout"`, `"userLayout"`, `"lastLayout"`,
`"layoutVariant"` (`"normal"`/`"reversed"`), and `"masterRatio"` on every
desktop object.

## Known gaps

- `master_ratio`, `layout_variant` and `last_layout` are now written to
  and read back from state restore (`bspc wm -r` / `-s`/`-l`), via
  `restore.c`'s `RESTORE_DOUBLE`/`RESTORE_ANY` macros. Not yet verified
  live (no restart-cycle test performed) — worth double-checking with an
  actual `bspc wm -r` before relying on it.
- The mouse-drag resize dispatch for `tall`/`wide` is only implemented for
  the X11 backend (`src/window.c`). The wlroots backend
  (`src/window_ops.c`) still uses the pre-port resize logic.
