---
name: handoff
description: Prepare a clean handoff before the user clears context. Captures the current work state (what's done, what's next, blockers, key facts) into the durable session-handoff + memory + docs and commits it, so the NEXT Claude can resume with zero prior context. Invoke when the user says they're about to /clear, wants a handoff/checkpoint, or asks to "save context" / "write a handoff".
---

# Handoff: prepare for a context clear

The user is about to wipe the conversation. Everything not written to a **durable, committed**
location is lost. Your job: distill the current state so a fresh Claude (no memory of this session)
can pick up exactly where you left off. Be concrete and honest — over-include facts, paths, and
gotchas; don't summarize away the details that cost this session to learn.

## Procedure

1. **Take stock.** Determine the active work: current branch, the active feature
   (`specs/<NNN>-*/` if any), recent commits (`git log --oneline -15`), and what you were doing /
   about to do. Note any decisions, corrections, or dead-ends discovered this session.

2. **Don't lose uncommitted work.** Run `git status`. If there's work-in-progress code, either
   commit it (branch first if on `master`) or explicitly note in the handoff that it's
   uncommitted and where. **Verify the working tree matches HEAD for anything that matters** — a
   value in the working tree but not committed will vanish on a fresh checkout. If code changed,
   confirm the host tests pass (`cd build && make && make test`, or at minimum `./smalltests`)
   and `./lint.sh check` is clean — CI enforces both.

3. **Write/update the SESSION HANDOFF** (the primary thing the next Claude reads):
   - If there's an active spec feature, **prepend a new dated handoff to the top of
     `specs/<NNN>-*/plan.md`** and mark the previous one `SUPERSEDED` (don't delete history).
   - Otherwise put it in the most relevant Claude memory file.
   - It MUST contain, written for someone with ZERO context:
     - **START HERE** pointer (what to read first — e.g. `docs/index.md`,
       `docs/architecture.md`, the relevant `docs/<subsystem>.md`, this plan).
     - **One line: what this work is.**
     - **DONE + committed** — with commit subjects/hashes and test counts.
     - **NOT done / NOT proven** — be honest; retract anything earlier-claimed-but-false.
       Distinguish "passes host tests" from "verified on hardware" — they are not the same.
     - **NEXT STEP** — concrete and actionable, not vague.
     - **BLOCKERS / open questions** (incl. anything to ask the user/another dev, and anything
       that needs a physical device or radio range test to confirm).
     - **KEY FACTS / infra** — paths, commands, gotchas, IDs (e.g. which PlatformIO env and
       upload method, `--gtest_filter` for the relevant suite, pin/flash quirks, library-fork
       pins) the next session needs and would otherwise re-derive.

4. **Update Claude memory** (`memory/MEMORY.md` index + the specific `memory/*.md` files): durable
   facts, corrected premises, decisions, and gotchas — one fact per file, per the memory rules.
   Memory persists across context clears, so this is your safety net even if a doc is missed.

5. **Update any docs that drifted** this session. The `docs/` research notes (architecture,
   radio-network, led-effects, devices, build-and-test, hardware) are the project's durable
   knowledge base — CLAUDE.md says to update them when documented behavior changes. If this
   session worked out any non-obvious subsystem behavior — mesh election/timing nuances, effect
   rendering details, a hardware or flashing gotcha, a build quirk — fold it into the relevant
   `docs/*.md` before the context is wiped. If a change touched an invariant listed in CLAUDE.md,
   make sure CLAUDE.md still tells the truth. When in doubt, ask the user whether there's an
   insight worth capturing rather than silently skipping.

6. **Commit everything in the repo** (handoff/spec/doc changes — these are NOT auto-saved). Run
   `./lint.sh check` first if any code changed. Use a clear message like
   `handoff: <feature> — <one-line state>`. Memory files live outside the repo but persist on
   disk; no commit needed for those.

7. **Confirm to the user** it's safe to clear, in 3–5 lines: the one-line state, the single most
   important NEXT STEP, and exactly where the next Claude should START reading. Keep it short — the
   detail lives in the committed handoff, not the chat.

8. **Emit a next-session prompt — AND copy it to the clipboard.** As the LAST thing, output a single
   copy-pasteable prompt (in a fenced code block) that the user can paste into a fresh session to
   direct the next Claude, **and put that exact prompt on the macOS clipboard so the user doesn't
   have to select/copy it**: write the prompt verbatim to a scratchpad file and run `pbcopy < file`
   (piping from a file avoids shell-quoting/backtick mangling that an inline `echo | pbcopy` would
   cause). Confirm in the chat that it's on the clipboard (and still show the fenced block as a
   fallback). It tells that Claude — which has ZERO context — what to read, the state, the task, and
   how to work.
   It must contain:
   - **Orientation:** what work is continuing, which project + branch, and "you have zero prior
     context — read the handoff before doing anything."
   - **READ FIRST, IN ORDER:** the exact files to read (the START-HERE doc(s), the dated Session
     Handoff at the top of the active `specs/<NNN>-*/plan.md`, and the relevant `[[memory]]`).
   - **CURRENT STATE:** a 2–4 line summary (what's done/committed + test count, and — bluntly —
     what is NOT done/proven; retract any earlier false claim).
   - **YOUR TASK:** the concrete next step(s), specific enough to act on.
   - **WORKING STYLE:** the durable preferences + corrections the user gave (e.g. don't claim
     "verified" without running it — and don't claim hardware behavior from host tests alone; run
     smalltests + `./lint.sh check` after changes; don't break the CLAUDE.md invariants; commit in
     chunks; `/handoff` before a clear) — carry these forward so the next Claude doesn't relearn
     them.
   - **Start instruction:** read the docs first, then state a plan or proceed.
   Keep it directive and tight; the detail lives in the committed handoff it points to, not the prompt.
   Then ask the user if they want it tweaked (terser, skip the plan check-in, etc.).

## Notes
- Bias toward MORE detail in the committed handoff, LESS in the chat reply.
- If you discovered this session that an earlier claim was wrong, the handoff MUST say so plainly —
  a confident-but-wrong handoff is worse than none.
- Don't start new implementation work here; this skill is purely about durably capturing state.
