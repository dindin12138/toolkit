## Language

**English** for anything that lands in the repo or on GitHub: issue titles and bodies, specs, tickets, PR bodies, commit messages, code identifiers, code comments, `GLOSSARY.md`, ADRs. The repo is public.

**Chinese** for the conversation: chat replies, explanations, summaries of what you did.

## Dev Environment

Toolchain is provided by nix + devenv. No dev tools are installed globally on this machine; do not install any.

Prefix dev commands with `direnv exec . ` (e.g. `direnv exec . cargo test`); if direnv is unavailable, use `devenv shell -- <command>`.

If that still fails, stop and tell me. Do not install tools or change environment config on your own.

## Agent skills

### Issue tracker

Issues live as GitHub issues on `dindin12138/toolkit`, driven by the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Five canonical triage roles, each mapped to a same-named label. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
