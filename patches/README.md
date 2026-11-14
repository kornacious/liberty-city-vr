# Shared re3 patch

`01-re3-liberty-vr.patch` is the complete upstream-to-port diff against public re3
commit `b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. It replaces the 90 ordered
patches shipped before 0.9.0. Assembly applies it before copying `overlay/` and
the bundled librw fork. The upstream checkout is never modified.

One flat patch suits the current architecture: Quest, Windows and Linux all
assemble the same engine source, with platform differences guarded in that
source and services provided by the overlay. Splitting by historical patch name
would separate interdependent edits to the same files and reintroduce ordering
dependencies. No platform-specific patch selection is needed.

The consolidation was checked against a clean LF export of the pinned base:
applying the old stack and applying this patch produced byte-identical source
trees (678 files). This comparison precedes the overlay copy and requires no
game data. The patch uses LF endings and full Git blob IDs.

To maintain it, export the pinned upstream source to a private scratch tree,
record its base tree in a temporary Git index, apply this patch, make engine
changes, then regenerate a single `git diff --binary --full-index --no-renames`
against that base. Include added/deleted files and preserve LF endings. Do not
diff an overlay-assembled tree: overlay and librw files are maintained separately.
Check application to a fresh base, run the manager's `test-patch-series` tool
with the local pinned re3 checkout, then assemble and build the selected target.
Never include game assets or local generated files in the diff.
