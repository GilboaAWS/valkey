#!/usr/bin/env python3
"""post-pr-replies.py — post resolution replies to GitHub PR review threads and resolve them.

Inputs:
  <out_dir>/pr-feedback.json    — curated tasks with decision fields
  <cache_dir>/pr.json           — PR metadata (head sha for doc permalinks)
  ~/kiro-cli.tok (or $GITHUB_TOKEN)

What it does, per task:
  1. POST a reply to the inline-comment thread (REST API), body derived from `decision`.
  2. Resolve the thread via GraphQL.

Default is --dry-run: prints what would be posted, does not call the API.

Usage:
    post-pr-replies.py <owner> <repo> <pr_number> <feedback_json> [--dry-run | --post]
                       [--only TASK_ID] [--skip-resolved] [--skip-already-replied]
                       [--head-ref BRANCH]

Output: prints per-task action and a summary at the end. Non-zero exit on any API error.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any


# -------------------- auth --------------------


def load_token() -> str:
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        return token
    tok_path = Path.home() / "kiro-cli.tok"
    if tok_path.is_file():
        t = tok_path.read_text(encoding="utf-8").strip()
        if t:
            return t
    print("ERROR: no token in $GITHUB_TOKEN and ~/kiro-cli.tok is missing/empty", file=sys.stderr)
    sys.exit(1)


# -------------------- http helpers --------------------


def _req(method: str, url: str, token: str, data: Any = None, accept: str = "application/vnd.github+json"):
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": accept,
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "valkey-pdd-pr-walkthrough",
    }
    body = None
    if data is not None:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req) as r:
            return r.getcode(), json.loads(r.read().decode("utf-8") or "null")
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8") or "null")


def rest_get(url, token):
    return _req("GET", url, token)


def rest_post(url, token, data):
    return _req("POST", url, token, data)


def graphql(token, query, variables=None):
    return _req("POST", "https://api.github.com/graphql", token, {"query": query, "variables": variables or {}})


# -------------------- reply formatting --------------------


def build_reply_body(task: dict, head_ref: str, owner: str, repo: str) -> str:
    """Turn a task's `decision` field into a GitHub reply body."""
    decision = (task.get("decision") or "").strip()
    task_id = task["task_id"]
    design_todo_url = (
        f"https://github.com/{owner}/{repo}/blob/{head_ref}/"
        f".agents/planning/realtime-data-compression/DESIGN_TODO.md#{task_id.lower()}"
    )
    if not decision:
        decision = "Resolved; see tracking link."
    parts = [
        "**Resolved via 2026-05-10 design walkthrough.**",
        "",
        decision,
        "",
        f"_Tracking: [`DESIGN_TODO.md` · {task_id}]({design_todo_url})_",
    ]
    return "\n".join(parts)


# -------------------- review-thread fetch --------------------


def fetch_review_threads(owner, repo, pr, token):
    """Return list of {thread_node_id, root_comment_database_id, is_resolved}."""
    query = """
    query($owner:String!, $repo:String!, $pr:Int!, $after:String) {
      repository(owner:$owner, name:$repo) {
        pullRequest(number:$pr) {
          reviewThreads(first:100, after:$after) {
            pageInfo { hasNextPage endCursor }
            nodes {
              id
              isResolved
              comments(first:1) {
                nodes { databaseId }
              }
            }
          }
        }
      }
    }
    """
    out = []
    after = None
    while True:
        code, resp = graphql(token, query, {"owner": owner, "repo": repo, "pr": pr, "after": after})
        if code != 200:
            raise RuntimeError(f"GraphQL {code}: {resp}")
        if "errors" in resp:
            raise RuntimeError(f"GraphQL errors: {resp['errors']}")
        data = resp["data"]["repository"]["pullRequest"]["reviewThreads"]
        for node in data["nodes"]:
            comments = node["comments"]["nodes"]
            if not comments:
                continue
            out.append({
                "thread_node_id": node["id"],
                "root_comment_database_id": comments[0]["databaseId"],
                "is_resolved": node["isResolved"],
            })
        if data["pageInfo"]["hasNextPage"]:
            after = data["pageInfo"]["endCursor"]
        else:
            break
    return out


# -------------------- actions --------------------


def post_reply(owner, repo, pr, root_comment_id, body, token):
    """Post a reply to an inline review comment. Returns (code, resp)."""
    url = f"https://api.github.com/repos/{owner}/{repo}/pulls/{pr}/comments/{root_comment_id}/replies"
    return rest_post(url, token, {"body": body})


def resolve_thread(thread_node_id, token):
    query = """
    mutation($id:ID!) {
      resolveReviewThread(input:{threadId:$id}) {
        thread { id isResolved }
      }
    }
    """
    return graphql(token, query, {"id": thread_node_id})


# -------------------- driver --------------------


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("owner")
    ap.add_argument("repo")
    ap.add_argument("pr_number", type=int)
    ap.add_argument("feedback_json", type=Path, help="path to pr-feedback.json")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", default=True, help="default: print only")
    mode.add_argument("--post", action="store_true", help="actually call the API")
    ap.add_argument("--only", default=None, help="single task id (e.g. T-3194682164)")
    ap.add_argument("--skip-resolved", action="store_true",
                    help="skip threads already resolved on GitHub")
    ap.add_argument("--head-ref", default=None,
                    help="branch/ref for doc permalinks (default: read from pr-cache/pr.json)")
    ap.add_argument("--sleep-ms", type=int, default=250,
                    help="delay between API calls")
    args = ap.parse_args()

    if args.post:
        args.dry_run = False

    token = load_token()

    feedback = json.loads(args.feedback_json.read_text(encoding="utf-8"))
    tasks = feedback["tasks"]
    if args.only:
        tasks = [t for t in tasks if t["task_id"] == args.only]
        if not tasks:
            print(f"ERROR: task {args.only} not found", file=sys.stderr)
            return 2

    # Resolve head ref if not given.
    head_ref = args.head_ref
    if not head_ref:
        pr_json = args.feedback_json.parent / ".pr-cache" / "pr.json"
        if pr_json.is_file():
            head_ref = json.loads(pr_json.read_text(encoding="utf-8"))["head"]["ref"]
        else:
            head_ref = "main"

    # Fetch review-thread map (needed to resolve).
    print(f"Fetching review threads for {args.owner}/{args.repo}#{args.pr_number} ...", file=sys.stderr)
    threads = fetch_review_threads(args.owner, args.repo, args.pr_number, token)
    by_root_id = {t["root_comment_database_id"]: t for t in threads}
    print(f"  found {len(threads)} review threads ({sum(1 for t in threads if t['is_resolved'])} already resolved)",
          file=sys.stderr)

    # Walk tasks.
    posted = resolved = skipped = errors = 0
    for i, task in enumerate(tasks, 1):
        task_id = task["task_id"]
        root_id = task["root_comment_id"]
        thread_info = by_root_id.get(root_id)
        if not thread_info:
            print(f"[{i:2}/{len(tasks)}] {task_id}: no review thread found for root comment {root_id}", file=sys.stderr)
            errors += 1
            continue

        is_resolved = thread_info["is_resolved"]
        thread_node_id = thread_info["thread_node_id"]

        if args.skip_resolved and is_resolved:
            print(f"[{i:2}/{len(tasks)}] {task_id}: already resolved, skipping", file=sys.stderr)
            skipped += 1
            continue

        body = build_reply_body(task, head_ref, args.owner, args.repo)

        mode_label = "DRY" if args.dry_run else "POST"
        print(f"[{i:2}/{len(tasks)}] {mode_label} {task_id} (thread {thread_node_id}, resolved={is_resolved})")
        print(f"    root_comment_id: {root_id}")
        print(f"    reply body ({len(body)} chars):")
        # Indent the body for readability.
        for line in body.splitlines():
            print(f"      {line}")
        print()

        if args.dry_run:
            continue

        # 1. Post reply.
        code, resp = post_reply(args.owner, args.repo, args.pr_number, root_id, body, token)
        if code >= 300:
            print(f"    ERROR posting reply ({code}): {resp}", file=sys.stderr)
            errors += 1
            time.sleep(args.sleep_ms / 1000.0)
            continue
        posted += 1
        print(f"    reply posted: {resp.get('html_url')}")
        time.sleep(args.sleep_ms / 1000.0)

        # 2. Resolve the thread (unless already resolved).
        if not is_resolved:
            code, resp = resolve_thread(thread_node_id, token)
            if code != 200 or "errors" in resp:
                print(f"    ERROR resolving thread ({code}): {resp}", file=sys.stderr)
                errors += 1
            else:
                resolved += 1
                print(f"    thread resolved")
            time.sleep(args.sleep_ms / 1000.0)

    print("", file=sys.stderr)
    print(f"Summary: posted={posted} resolved={resolved} skipped={skipped} errors={errors} total_tasks={len(tasks)}",
          file=sys.stderr)
    return 0 if errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
