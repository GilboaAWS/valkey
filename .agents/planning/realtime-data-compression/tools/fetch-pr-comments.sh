#!/usr/bin/env bash
# fetch-pr-comments.sh — pull PR data from GitHub into a local JSON cache.
#
# Usage:
#   ./fetch-pr-comments.sh <owner> <repo> <pr_number> [cache_dir]
#
# Example:
#   ./fetch-pr-comments.sh ikolomi valkey 1
#
# Output files (in cache_dir, default ./.pr-cache):
#   pr.json              — PR metadata
#   issue-comments.json  — conversation-level (issue) comments
#   pr-comments.json     — inline review comments (attached to file/line)
#   pr-reviews.json      — PR review summaries (the review wrappers)
#
# Authentication: reads GITHUB_TOKEN from env if set (raises rate limit from
# 60 req/hr to 5000 req/hr). Unauthenticated access works for public repos.

set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "usage: $0 <owner> <repo> <pr_number> [cache_dir]" >&2
    exit 2
fi

OWNER="$1"
REPO="$2"
PR_NUM="$3"
CACHE_DIR="${4:-$(dirname "$0")/../.pr-cache}"

mkdir -p "$CACHE_DIR"

API="https://api.github.com/repos/${OWNER}/${REPO}"

auth_header=()
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    auth_header=(-H "Authorization: Bearer ${GITHUB_TOKEN}")
fi

fetch() {
    local endpoint="$1"
    local out="$2"
    # GitHub paginates at 100; collect all pages.
    local page=1
    local tmp
    tmp=$(mktemp)
    : >"$out"
    # For single-object endpoints (no pagination) the first response is final.
    # We detect this by whether the top-level is an array.
    local first
    first=$(curl -sS \
        -H 'Accept: application/vnd.github+json' \
        -H 'X-GitHub-Api-Version: 2022-11-28' \
        "${auth_header[@]}" \
        "${API}${endpoint}?per_page=100&page=${page}")
    # Detect shape.
    if echo "$first" | jq -e 'type == "array"' >/dev/null 2>&1; then
        # Array — paginate.
        echo "$first" >"$tmp"
        echo "[" >"$out"
        local first_item=true
        while read -r item; do
            if $first_item; then first_item=false; else echo "," >>"$out"; fi
            echo "$item" >>"$out"
        done < <(jq -c '.[]' "$tmp")
        # Fetch remaining pages until an empty page.
        while true; do
            page=$((page + 1))
            local next
            next=$(curl -sS \
                -H 'Accept: application/vnd.github+json' \
                -H 'X-GitHub-Api-Version: 2022-11-28' \
                "${auth_header[@]}" \
                "${API}${endpoint}?per_page=100&page=${page}")
            local len
            len=$(echo "$next" | jq 'length')
            if [[ "$len" == "0" ]]; then break; fi
            while read -r item; do
                echo "," >>"$out"
                echo "$item" >>"$out"
            done < <(echo "$next" | jq -c '.[]')
        done
        echo "]" >>"$out"
    else
        # Single object.
        echo "$first" >"$out"
    fi
    rm -f "$tmp"
}

echo "Fetching PR #${PR_NUM} from ${OWNER}/${REPO} into ${CACHE_DIR}..." >&2
fetch "/pulls/${PR_NUM}"                "${CACHE_DIR}/pr.json"
fetch "/issues/${PR_NUM}/comments"      "${CACHE_DIR}/issue-comments.json"
fetch "/pulls/${PR_NUM}/comments"       "${CACHE_DIR}/pr-comments.json"
fetch "/pulls/${PR_NUM}/reviews"        "${CACHE_DIR}/pr-reviews.json"

# Minimal sanity checks.
for f in pr.json issue-comments.json pr-comments.json pr-reviews.json; do
    if ! jq -e . >/dev/null 2>&1 <"${CACHE_DIR}/${f}"; then
        echo "ERROR: invalid JSON in ${CACHE_DIR}/${f}" >&2
        exit 1
    fi
done

# Report counts.
pr_title=$(jq -r '.title' "${CACHE_DIR}/pr.json")
issue_n=$(jq 'length' "${CACHE_DIR}/issue-comments.json")
review_n=$(jq 'length' "${CACHE_DIR}/pr-comments.json")
rev_n=$(jq 'length' "${CACHE_DIR}/pr-reviews.json")

echo "  PR: ${pr_title}" >&2
echo "  conversation comments : ${issue_n}" >&2
echo "  inline review comments: ${review_n}" >&2
echo "  review entries        : ${rev_n}" >&2
echo "Done." >&2
