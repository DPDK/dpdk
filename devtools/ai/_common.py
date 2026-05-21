#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Stephen Hemminger

"""Common utilities shared by the DPDK AI review scripts."""

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from typing import Any, NoReturn
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

# Provider configurations (model defaults; override with --model).
PROVIDERS: dict[str, dict[str, str]] = {
    "anthropic": {
        "name": "Claude",
        "endpoint": "https://api.anthropic.com/v1/messages",
        "default_model": "claude-sonnet-4-5-20250929",
        "env_var": "ANTHROPIC_API_KEY",
    },
    "openai": {
        "name": "ChatGPT",
        "endpoint": "https://api.openai.com/v1/chat/completions",
        "default_model": "gpt-4.1",
        "env_var": "OPENAI_API_KEY",
    },
    "xai": {
        "name": "Grok",
        "endpoint": "https://api.x.ai/v1/chat/completions",
        "default_model": "grok-4-1-fast-non-reasoning",
        "env_var": "XAI_API_KEY",
    },
    "google": {
        "name": "Gemini",
        "endpoint": "https://generativelanguage.googleapis.com/v1beta/models",
        "default_model": "gemini-3-flash-preview",
        "env_var": "GOOGLE_API_KEY",
    },
}


def error(msg: str) -> NoReturn:
    """Print error message to stderr and exit with status 1."""
    print(f"Error: {msg}", file=sys.stderr)
    sys.exit(1)


def get_git_config(key: str) -> str | None:
    """Return the value of a git config key, or None if unset/git missing."""
    try:
        result = subprocess.run(
            ["git", "config", "--get", key],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def list_providers() -> NoReturn:
    """Print available providers and exit."""
    print("Available AI Providers:\n")
    print(f"{'Provider':<12} {'Default Model':<30} {'API Key Variable'}")
    print(f"{'--------':<12} {'-------------':<30} {'----------------'}")
    for name, config in PROVIDERS.items():
        print(f"{name:<12} {config['default_model']:<30} {config['env_var']}")
    sys.exit(0)


@dataclass
class TokenUsage:
    """Accumulated token usage across API calls."""

    input_tokens: int = 0
    output_tokens: int = 0
    cache_creation_tokens: int = 0
    cache_read_tokens: int = 0
    api_calls: int = 0

    def add(self, other: "TokenUsage") -> None:
        """Accumulate usage from another TokenUsage."""
        self.input_tokens += other.input_tokens
        self.output_tokens += other.output_tokens
        self.cache_creation_tokens += other.cache_creation_tokens
        self.cache_read_tokens += other.cache_read_tokens
        self.api_calls += other.api_calls


def format_token_summary(usage: TokenUsage, provider: str, model: str) -> str:
    """Format a token usage summary string."""
    provider_label = PROVIDERS.get(provider, {}).get("name", provider)
    lines = ["=== Token Usage Summary ==="]
    lines.append(f"Provider:      {provider_label} ({model})")
    lines.append(f"API calls:     {usage.api_calls}")
    lines.append(f"Input tokens:  {usage.input_tokens:,}")
    lines.append(f"Output tokens: {usage.output_tokens:,}")
    if usage.cache_creation_tokens:
        lines.append(f"Cache write:   {usage.cache_creation_tokens:,}")
    if usage.cache_read_tokens:
        lines.append(f"Cache read:    {usage.cache_read_tokens:,}")
    total = usage.input_tokens + usage.output_tokens
    lines.append(f"Total tokens:  {total:,}")
    lines.append("=" * 27)
    return "\n".join(lines)


def add_token_args(parser: argparse.ArgumentParser) -> None:
    """Add the --show-tokens flag to an ArgumentParser."""
    parser.add_argument(
        "--show-tokens",
        action="store_true",
        help="Show token usage summary on stderr after the run",
    )


def print_token_summary(
    usage: TokenUsage, provider: str, model: str, show: bool
) -> None:
    """Print token usage summary to stderr if requested and any calls were made."""
    if not show or usage.api_calls == 0:
        return
    print("", file=sys.stderr)
    print(format_token_summary(usage, provider, model), file=sys.stderr)


def _build_request_meta(
    provider: str, api_key: str, model: str
) -> tuple[str, dict[str, str]]:
    """Return (url, headers) for a provider request."""
    config = PROVIDERS[provider]
    if provider == "anthropic":
        return config["endpoint"], {
            "Content-Type": "application/json",
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
        }
    if provider == "google":
        url = f"{config['endpoint']}/{model}:generateContent?key={api_key}"
        return url, {"Content-Type": "application/json"}
    # openai, xai
    return config["endpoint"], {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {api_key}",
    }


def _extract_usage(provider: str, result: dict[str, Any]) -> TokenUsage:
    """Extract TokenUsage from a provider response."""
    usage = TokenUsage(api_calls=1)
    if provider == "anthropic":
        raw = result.get("usage", {})
        usage.input_tokens = raw.get("input_tokens", 0)
        usage.output_tokens = raw.get("output_tokens", 0)
        usage.cache_creation_tokens = raw.get("cache_creation_input_tokens", 0)
        usage.cache_read_tokens = raw.get("cache_read_input_tokens", 0)
    elif provider == "google":
        raw = result.get("usageMetadata", {})
        usage.input_tokens = raw.get("promptTokenCount", 0)
        usage.output_tokens = raw.get("candidatesTokenCount", 0)
    else:  # openai, xai
        raw = result.get("usage", {})
        usage.input_tokens = raw.get("prompt_tokens", 0)
        usage.output_tokens = raw.get("completion_tokens", 0)
        cache_details = raw.get("prompt_tokens_details", {})
        if cache_details:
            usage.cache_read_tokens = cache_details.get("cached_tokens", 0)
    return usage


def _extract_text(provider: str, result: dict[str, Any]) -> str:
    """Extract response text from a provider response. Calls error() on failure."""
    if "error" in result:
        error(f"API error: {result['error'].get('message', result)}")
    if provider == "anthropic":
        content = result.get("content", [])
        return "".join(
            block.get("text", "") for block in content if block.get("type") == "text"
        )
    if provider == "google":
        candidates = result.get("candidates", [])
        if not candidates:
            error("No response from Gemini")
        parts = candidates[0].get("content", {}).get("parts", [])
        return "".join(part.get("text", "") for part in parts)
    # openai, xai
    choices = result.get("choices", [])
    if not choices:
        error("No response from API")
    return choices[0].get("message", {}).get("content", "")


def _print_verbose_usage(usage: TokenUsage) -> None:
    """Print per-call token details to stderr."""
    print("=== Token Usage ===", file=sys.stderr)
    print(f"Input tokens: {usage.input_tokens:,}", file=sys.stderr)
    print(f"Output tokens: {usage.output_tokens:,}", file=sys.stderr)
    if usage.cache_creation_tokens:
        print(f"Cache creation: {usage.cache_creation_tokens:,}", file=sys.stderr)
    if usage.cache_read_tokens:
        print(f"Cache read: {usage.cache_read_tokens:,}", file=sys.stderr)
    print("===================", file=sys.stderr)


def send_request(
    provider: str,
    api_key: str,
    model: str,
    request_data: dict[str, Any],
    *,
    timeout: int = 120,
    verbose: bool = False,
) -> tuple[str, TokenUsage]:
    """Send a prebuilt request to a provider and return (response_text, usage).

    The caller assembles the provider-specific request body via its own
    build_*_request helpers (the prompts differ per script). This function
    handles transport, error reporting, and token-usage extraction.
    """
    url, headers = _build_request_meta(provider, api_key, model)
    body = json.dumps(request_data).encode("utf-8")
    req = Request(url, data=body, headers=headers)

    try:
        with urlopen(req, timeout=timeout) as response:
            result = json.loads(response.read().decode("utf-8"))
    except HTTPError as e:
        error_body = e.read().decode("utf-8")
        try:
            error_data = json.loads(error_body)
            error(f"API error: {error_data.get('error', error_body)}")
        except json.JSONDecodeError:
            error(f"API error ({e.code}): {error_body}")
    except URLError as e:
        if isinstance(e.reason, TimeoutError):
            error(f"Request timed out after {timeout} seconds")
        error(f"Connection error: {e.reason}")

    usage = _extract_usage(provider, result)
    if verbose:
        _print_verbose_usage(usage)
    return _extract_text(provider, result), usage
