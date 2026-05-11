#!/usr/bin/env python3
"""normalize-pr-comments.py — turn raw GitHub API JSON into a curated review-task file.

Inputs:   <cache_dir>/{pr,issue-comments,pr-comments,pr-reviews}.json
Outputs:
  <out_dir>/pr-feedback.json  — structured, machine-readable
  <out_dir>/DESIGN_TODO.md    — reviewer-friendly markdown with stable task IDs

Design notes
------------
- One "thread" per root inline comment. Replies are nested.
- Stable task IDs derived from the root GitHub comment ID — survive re-runs.
- If an existing DESIGN_TODO.md is present we preserve operator-added fields
  (status, decision, resolved_by) on threads whose root comment ID still
  exists. New threads are appended.
- Thematic grouping is seeded from file path but the status/decision columns
  are where the iteration actually happens.

Usage:
  normalize-pr-comments.py <cache_dir> <out_dir>
"""

from __future__ import annotations

import json
import os
import re
import sys
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from textwrap import indent


# ------------------------- loading -------------------------


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


# ------------------------- threading -------------------------


def build_threads(inline_comments: list[dict]) -> list[dict]:
    """Return a list of top-level threads. Each thread is
    {root: {...}, replies: [{...}, ...]}.
    """
    by_id = {c["id"]: c for c in inline_comments}
    replies_by_parent: dict[int, list[dict]] = defaultdict(list)
    roots: list[dict] = []
    for c in inline_comments:
        if c.get("in_reply_to_id"):
            replies_by_parent[c["in_reply_to_id"]].append(c)
        else:
            roots.append(c)
    # Sort roots by (path, line, created_at) for stable ordering.
    roots.sort(key=lambda c: (c.get("path") or "", c.get("line") or 0, c["created_at"]))
    threads = []
    for root in roots:
        replies = sorted(replies_by_parent.get(root["id"], []), key=lambda c: c["created_at"])
        threads.append({"root": root, "replies": replies})
    # Orphaned replies (parent not in the fetched set — shouldn't happen with
    # a single-PR fetch, but guard): attach to a synthetic root.
    fetched_ids = set(by_id.keys())
    for c in inline_comments:
        if c.get("in_reply_to_id") and c["in_reply_to_id"] not in fetched_ids:
            threads.append({"root": c, "replies": [], "_orphan": True})
    return threads


# ------------------------- markdown rendering -------------------------


def short_quote(diff_hunk: str, max_lines: int = 3) -> str:
    """Return the last `max_lines` added lines from a diff hunk (what the
    comment is attached to) for context without drowning the reader."""
    if not diff_hunk:
        return ""
    lines = diff_hunk.splitlines()
    # Find the last run of added lines ("+...").
    added = [l[1:] for l in lines if l.startswith("+") and not l.startswith("+++")]
    if not added:
        return ""
    quoted = added[-max_lines:]
    return "\n".join(f"> {l}" for l in quoted)


def render_markdown(threads: list[dict], pr_meta: dict, existing: dict) -> str:
    """Render DESIGN_TODO.md, preserving operator-added fields from `existing`."""
    out: list[str] = []
    out.append("# DESIGN_TODO — PR review-task log\n")
    out.append(
        f"_Generated from [PR #{pr_meta['number']}]({pr_meta['html_url']}) "
        f"(\"{pr_meta['title']}\") at "
        f"{datetime.now(timezone.utc).isoformat(timespec='seconds')}._\n"
    )
    out.append(
        "_Workflow: each task has a stable ID (`T-<short>`). "
        "Agents read `status`/`decision` as the source of truth for what's still open. "
        "After updating the design, set `status: addressed` and fill `decision:`; "
        "re-running the normalizer preserves these fields._\n"
    )
    out.append(
        "_Legend: `status` ∈ {`open`, `needs-discussion`, `addressed`, `wont-fix`, `duplicate`}._\n"
    )

    # Group by file for readability.
    by_file: dict[str, list[dict]] = defaultdict(list)
    for t in threads:
        by_file[t["root"].get("path") or "(general)"].append(t)

    task_counter = 0
    for path in sorted(by_file.keys()):
        out.append(f"\n## {path}\n")
        for t in sorted(by_file[path], key=lambda x: (x["root"].get("line") or 0, x["root"]["created_at"])):
            task_counter += 1
            root = t["root"]
            root_id = root["id"]
            task_id = f"T-{root_id}"

            # Pull existing operator-authored fields, if any.
            prev = existing.get(str(root_id), {})
            status = prev.get("status", "open")
            decision = prev.get("decision", "")
            resolved_by = prev.get("resolved_by", "")

            author = root["user"]["login"]
            line = root.get("line") or root.get("original_line") or "?"
            url = root["html_url"]
            created = root["created_at"]

            out.append(f"### #{task_counter} · `{task_id}` · line {line} · @{author}  {{#{task_id}}}\n")
            out.append(f"- **status:** `{status}`")
            if decision:
                out.append(f"- **decision:** {decision}")
            if resolved_by:
                out.append(f"- **resolved_by:** {resolved_by}")
            out.append(f"- **created:** {created}")
            out.append(f"- **permalink:** {url}\n")

            # Context quote from diff hunk.
            quote = short_quote(root.get("diff_hunk") or "")
            if quote:
                out.append("**context (what was commented on):**\n")
                out.append(quote + "\n")

            # Root body.
            out.append("**comment:**\n")
            out.append(root.get("body", "").rstrip() + "\n")

            # Replies, if any.
            for reply in t.get("replies", []):
                r_author = reply["user"]["login"]
                out.append(f"\n**↳ reply by @{r_author} ({reply['created_at']}):**\n")
                out.append(reply.get("body", "").rstrip() + "\n")

            out.append("")  # blank line separator
    return "\n".join(out) + "\n"


# ------------------------- existing-file merge -------------------------


_STATUS_RE = re.compile(r"^- \*\*status:\*\*\s*`([^`]+)`", re.MULTILINE)
_DECISION_RE = re.compile(r"^- \*\*decision:\*\*\s*(.+)$", re.MULTILINE)
_RESOLVED_RE = re.compile(r"^- \*\*resolved_by:\*\*\s*(.+)$", re.MULTILINE)
_TASK_HEAD_RE = re.compile(r"^### #\d+ · `T-(\d+)` ·")


def parse_existing(md_path: Path) -> dict:
    """Parse operator-authored fields from a previously generated DESIGN_TODO.md.

    Returns a dict keyed by root comment id (as string) containing the
    operator-facing fields we preserve across runs.
    """
    if not md_path.is_file():
        return {}
    out: dict[str, dict] = {}
    text = md_path.read_text(encoding="utf-8")
    # Split on task headers.
    parts = re.split(r"(?m)^(?=### #\d+ · `T-\d+` ·)", text)
    for part in parts:
        m = _TASK_HEAD_RE.search(part)
        if not m:
            continue
        task_id = m.group(1)
        entry: dict = {}
        if sm := _STATUS_RE.search(part):
            entry["status"] = sm.group(1).strip()
        if dm := _DECISION_RE.search(part):
            entry["decision"] = dm.group(1).strip()
        if rm := _RESOLVED_RE.search(part):
            entry["resolved_by"] = rm.group(1).strip()
        out[task_id] = entry
    return out


# ------------------------- JSON output -------------------------


def render_json(threads: list[dict], pr_meta: dict, existing: dict) -> dict:
    tasks = []
    for t in threads:
        root = t["root"]
        root_id = root["id"]
        prev = existing.get(str(root_id), {})
        tasks.append(
            {
                "task_id": f"T-{root_id}",
                "root_comment_id": root_id,
                "path": root.get("path"),
                "line": root.get("line") or root.get("original_line"),
                "side": root.get("side"),
                "author": root["user"]["login"],
                "created_at": root["created_at"],
                "permalink": root["html_url"],
                "diff_hunk": root.get("diff_hunk"),
                "body": root.get("body", ""),
                "replies": [
                    {
                        "comment_id": r["id"],
                        "author": r["user"]["login"],
                        "created_at": r["created_at"],
                        "body": r.get("body", ""),
                    }
                    for r in t.get("replies", [])
                ],
                "status": prev.get("status", "open"),
                "decision": prev.get("decision", ""),
                "resolved_by": prev.get("resolved_by", ""),
            }
        )
    return {
        "pr": {
            "number": pr_meta["number"],
            "title": pr_meta["title"],
            "state": pr_meta["state"],
            "html_url": pr_meta["html_url"],
            "base": pr_meta["base"]["ref"],
            "head": pr_meta["head"]["ref"],
        },
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "tasks": tasks,
    }


# ------------------------- entry point -------------------------


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: normalize-pr-comments.py <cache_dir> <out_dir>", file=sys.stderr)
        return 2
    cache_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    pr_meta = load_json(cache_dir / "pr.json")
    inline_comments = load_json(cache_dir / "pr-comments.json")
    # issue_comments and pr_reviews exist for completeness — we currently only
    # surface inline comments, since that's where all PR #1 feedback lives.
    # Extend here if conversation comments or review bodies gain content.

    threads = build_threads(inline_comments)

    md_path = out_dir / "DESIGN_TODO.md"
    existing = parse_existing(md_path)

    md = render_markdown(threads, pr_meta, existing)
    (out_dir / "DESIGN_TODO.md").write_text(md, encoding="utf-8")

    j = render_json(threads, pr_meta, existing)
    (out_dir / "pr-feedback.json").write_text(
        json.dumps(j, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    total = len(threads)
    open_n = sum(1 for t in j["tasks"] if t["status"] == "open")
    addressed = sum(1 for t in j["tasks"] if t["status"] == "addressed")
    print(
        f"Wrote {total} threads "
        f"({open_n} open, {addressed} addressed) → {out_dir}/",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
