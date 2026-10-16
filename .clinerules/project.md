# Project rules

- At the start of every task read `AGENTS.md` in the repo root and follow it.
- Before changing a module, read the matching section of `docs/ru` (index: `docs/ru/README.md`).

# Memory bank

Keep task memory in `memory-bank/` (create it if missing):
- `activeContext.md` — current task, what is done, next steps, open questions.
- `progress.md` — finished work with commit hashes, known problems.

At the start of a task read both files. Update `activeContext.md` after each meaningful step
and before the context gets long, so the work can continue after the history is truncated.
When the user says "update memory bank", rewrite both files from the current state.
