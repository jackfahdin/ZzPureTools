# Task 4 Round 2 Targeted Review

Review range: `0249377..567e91e`. This review is read-only. The main agent separately reran the reported `ZzWorkspaceCrossTransferTest` and `ZzSplitWorkspaceTest` suites; this review did not rerun the complete suite.

## Previous Finding

### Edge transfer updates the model tree but not the visible layout

**ADDRESSED.** The cross-workspace edge branch still creates its temporary group with `splitGroup(..., false)` (`ZzWorkspaceCrossTransferTransactionPrivate.cpp:248-250`), so the temporary structure remains invisible until the nested center transaction and its ownership/metadata audits have succeeded. It now calls `guardedTarget->d_ptr->rebuildView()` exactly once on that success path (`264-266`), before publishing `groupAdded`, `layoutChanged`, `activeGroupChanged`, and `tabTransferCommitted` (`267-278`).

The call is after the recursive center transfer returned success; its `emitSignals = false` invocation does not rebuild the target. Because the temporary split was created with `rebuildViewAfterCommit = false`, there is no earlier rebuild in this path. Failure exits at `258-263`, silently removes the empty temporary group with the private default no-rebuild call, and emits no success structural or transfer signals.

`rebuildView()` reuses each leaf's existing `ZzTabWidget`, reparents it before deleting an obsolete splitter, then rebuilds the splitter tree (`ZzSplitWorkspacePrivate.cpp:3279-3338`). It does not reconstruct tab widgets or pages, so the committed page remains in its original tab container. The expanded four-edge test shows the target, processes its events, and verifies every resulting tab widget is a target descendant with a non-empty geometry (`ZzWorkspaceCrossTransferTest.cpp:18-47`). This is a meaningful visible-layout assertion rather than a model-only assertion.

## Lifetime and Signal Safety

**ADDRESSED.** The added rebuild is guarded by `QPointer<ZzSplitWorkspace>`. `rebuildView()` itself guards deletion of the prior splitter and returns if that deletion destroys its workspace (`ZzSplitWorkspacePrivate.cpp:3293-3307`). After rebuilding, every externally observable signal in the edge branch checks the guarded target before a subsequent dereference (`ZzWorkspaceCrossTransferTransactionPrivate.cpp:267-278`). A source destroyed by one of those callbacks is held as a `QPointer` and is not dereferenced afterwards. No target use-after-free path was found.

## New Findings

### Critical

None.

### Important

None.

### Minor

None introduced by this repair. `git diff --check 0249377..567e91e` is clean.

## Scope Observations

The new layout assertion checks ancestry and non-empty geometry for all four edge zones. It does not count `rebuildView()` invocations directly, but the branch structure establishes one successful-path invocation and the UI assertion catches the prior missing-rebuild regression.

## Final Conclusion

The previous Important finding is resolved. The repair rebuilds the visible target layout only after a successful edge transaction, preserves pages while reconstructing splitters, keeps failed edge transactions silent and non-rebuilding, and retains QPointer protection across rebuild and notification boundaries. This targeted repair is acceptable.
