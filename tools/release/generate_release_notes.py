#!/usr/bin/env python3
"""
Generate release notes using AI.
Collects commits since last release and creates a summary via LLM API.
"""

import os
import re
import subprocess
import sys
from typing import Dict, List, Optional, Tuple

import requests


def run_git_command(cmd: List[str]) -> str:
    try:
        result = subprocess.run(["git"] + cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as exc:
        print(f"Git command failed: {exc}", file=sys.stderr)
        return ""


def parse_semver(tag: str) -> Optional[Tuple[int, int, int]]:
    cleaned = tag[1:] if tag.startswith("v") else tag
    parts = cleaned.split(".")
    if len(parts) != 3:
        return None
    try:
        return tuple(int(p) for p in parts)
    except ValueError:
        return None


def get_last_release_tag() -> str:
    describe = run_git_command(["describe", "--tags", "--abbrev=0"])
    if parse_semver(describe):
        return describe

    tags = run_git_command(["tag", "--sort=-version:refname", "--merged"])
    if tags:
        for tag in tags.split("\n"):
            if parse_semver(tag):
                return tag
    return ""


def get_previous_semver_tag(current_version: str) -> str:
    tags_output = run_git_command(["tag", "--list"])
    if not tags_output:
        return ""

    current_tuple = parse_semver(current_version) or (0, 0, 0)
    candidates = []
    for tag in tags_output.split("\n"):
        value = parse_semver(tag)
        if value and value < current_tuple:
            candidates.append((value, tag))

    if not candidates:
        return ""

    candidates.sort(reverse=True)
    return candidates[0][1]


def get_commits_since_tag(tag: str) -> List[Dict[str, str]]:
    if not tag:
        cmd = ["log", "--oneline", "-20", "--pretty=format:%H|%s|%an|%ad", "--date=short"]
    else:
        cmd = ["log", f"{tag}..HEAD", "--oneline", "--pretty=format:%H|%s|%an|%ad", "--date=short"]

    output = run_git_command(cmd)
    if not output:
        return []

    commits = []
    for line in output.split("\n"):
        if "|" not in line:
            continue
        parts = line.split("|", 3)
        if len(parts) != 4:
            continue
        commits.append(
            {
                "hash": parts[0][:8],
                "message": parts[1],
                "author": parts[2],
                "date": parts[3],
            }
        )
    return commits


def filter_commits(commits: List[Dict[str, str]]) -> List[Dict[str, str]]:
    skip_patterns = [
        r"Auto-increment version",
        r"Merge pull request",
        r"Merge branch",
        r"Update version\.txt",
        r"^\d+\.\d+\.\d+$",
    ]

    filtered = []
    for commit in commits:
        message = commit["message"]
        if any(re.search(pattern, message, re.IGNORECASE) for pattern in skip_patterns):
            continue
        if message.strip():
            filtered.append(commit)
    return filtered


def generate_basic_summary(commits: List[Dict[str, str]], version: str) -> str:
    if not commits:
        return f"# Release Notes\n\n## Version {version}\n\nMinor improvements and bug fixes."

    summary = f"# Release Notes\n\n## Version {version}\n\n"
    features: List[str] = []
    fixes: List[str] = []
    improvements: List[str] = []

    for commit in commits[:10]:
        message = commit["message"].lower()
        if any(word in message for word in ["add", "new", "feature", "implement"]):
            features.append(commit["message"])
        elif any(word in message for word in ["fix", "bug", "error", "issue"]):
            fixes.append(commit["message"])
        else:
            improvements.append(commit["message"])

    if features:
        summary += "## Features\n"
        for item in features:
            summary += f"- {item}\n"
        summary += "\n"

    if fixes:
        summary += "## Bug Fixes\n"
        for item in fixes:
            summary += f"- {item}\n"
        summary += "\n"

    if improvements:
        summary += "## Improvements\n"
        for item in improvements:
            summary += f"- {item}\n"
        summary += "\n"

    return summary.strip()


def generate_ai_summary(commits: List[Dict[str, str]], version: str) -> str:
    api_key = os.getenv("LLM_API_KEY")
    base_url = os.getenv("LLM_BASE_URL", "").strip()
    model = os.getenv("LLM_MODEL", "gpt-4o-mini").strip()
    provider = os.getenv("LLM_PROVIDER", "").strip().lower()

    if not api_key:
        print("No LLM_API_KEY found, generating basic summary", file=sys.stderr)
        return generate_basic_summary(commits, version)

    if not base_url:
        base_url = "https://api.openai.com"
    elif not base_url.startswith("http"):
        base_url = f"https://{base_url}"
    base_url = base_url.rstrip("/")

    if not provider:
        if "anthropic" in base_url or model.lower().startswith("claude"):
            provider = "anthropic"
        else:
            provider = "openai"

    commits_text = "\n".join(
        [f"- {commit['hash']}: {commit['message']} (by {commit['author']})" for commit in commits]
    )

    prompt = f"""You are generating release notes for HeatControl version {version}, an ESP32-based heating controller project.

Please analyze these commits and create professional release notes in Markdown format:

{commits_text}

Instructions:
- Group changes into logical categories (Features, Bug Fixes, Improvements, Documentation)
- Write in English
- Be concise and user-focused
- Do not include commit hashes or authors in the final output
- If changes are minor, summarize them briefly
"""

    try:
        if provider == "anthropic":
            headers = {
                "x-api-key": api_key,
                "anthropic-version": "2023-06-01",
                "content-type": "application/json",
            }
            data = {
                "model": model,
                "max_tokens": 1000,
                "messages": [{"role": "user", "content": prompt}],
            }
            response = requests.post(f"{base_url}/v1/messages", headers=headers, json=data, timeout=45)
            response.raise_for_status()
            result = response.json()
            parts = []
            for block in result.get("content", []):
                if isinstance(block, dict) and block.get("type") == "text":
                    parts.append(block.get("text", ""))
            content = "\n".join(parts).strip()
            return content if content else generate_basic_summary(commits, version)

        headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }
        model_to_use = model[7:] if model.startswith("openai/") else model
        data = {
            "model": model_to_use,
            "messages": [{"role": "user", "content": prompt}],
            "max_tokens": 1000,
            "temperature": 0.7,
        }
        response = requests.post(f"{base_url}/v1/chat/completions", headers=headers, json=data, timeout=45)
        response.raise_for_status()
        result = response.json()
        content = result.get("choices", [{}])[0].get("message", {}).get("content", "")
        return content.strip() if content else generate_basic_summary(commits, version)
    except Exception as exc:
        print(f"AI generation failed: {exc}", file=sys.stderr)
        return generate_basic_summary(commits, version)


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python generate_release_notes.py <version>", file=sys.stderr)
        return 1

    version = sys.argv[1]
    last_tag = get_previous_semver_tag(version)
    if not last_tag:
        last_tag = get_last_release_tag()

    print(f"Last release tag: {last_tag or 'None found'}", file=sys.stderr)
    commits = get_commits_since_tag(last_tag)
    print(f"Found {len(commits)} commits since last release", file=sys.stderr)

    filtered_commits = filter_commits(commits)
    print(f"Filtered to {len(filtered_commits)} relevant commits", file=sys.stderr)

    if not filtered_commits:
        notes = f"# Release Notes\n\n## Version {version}\n\nMinor improvements and bug fixes."
    else:
        notes = generate_ai_summary(filtered_commits, version)

    print(notes)
    return 0


if __name__ == "__main__":
    sys.exit(main())
