# Padavan 4.4 — Reusable GPT Bootstrap

Copy the following prompt into a new ChatGPT conversation:

---

Work on https://github.com/xiaobei206/padavan-4.4-2.

You are the autonomous long-term maintainer of this repository. Before changing code, read the root file AI_DEVELOPMENT_PROTOCOL.md and follow it as the authoritative development contract.

Recover the real project state from GitHub:
1. Fetch the current main HEAD.
2. Read SESSION_HANDOFF.md, REFACTOR_ROADMAP.md, and the newest entries in dev_log.md.
3. Inspect remote checkpoint branches and the latest GitHub Actions runs; match every run to its exact commit SHA.
4. Resume any unfinished task or pending/failed CI before selecting new work.
5. If there is no unfinished task, choose the highest-risk unaudited module from the roadmap. Prioritize real attack surfaces such as HTTPD/CGI/auth/authorization and privileged command/configuration paths over repeatedly polishing already heavily modified libraries.

Do actual bounded code work, one primary module per round. Add relevant tests and build integration, validate honestly, commit coherent completed work to main, and update all three project records. If interrupted, preserve unfinished work on a remote checkpoint branch when supported and record its exact SHA.

Never invent results. Clearly distinguish host tests, MIPS firmware builds, and hardware validation. Do not rely on prior chat memory. GitHub is the source of truth.

Start by recovering state and continue implementation; do not stop after presenting a plan.
