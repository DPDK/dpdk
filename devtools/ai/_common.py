#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2026 Stephen Hemminger

"""Common utilities shared by the DPDK AI review scripts."""

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from typing import Any, NoReturn
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

# Optional dependency for Vertex AI
try:
    from google.auth import default as google_auth_default
    from google.auth.exceptions import GoogleAuthError
    from google.auth.transport.requests import Request as GoogleAuthRequest
    VERTEX_AI_AVAILABLE = True
except ImportError:
    VERTEX_AI_AVAILABLE = False

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
    print(f"{'Provider':<12} {'Default Model':<30} {'API Key (Direct Auth)'}")
    print(f"{'--------':<12} {'-------------':<30} {'---------------------'}")
    for name, config in PROVIDERS.items():
        print(f"{name:<12} {config['default_model']:<30} {config['env_var']}")
    if VERTEX_AI_AVAILABLE:
        print("\nVertex AI authentication is available (use --auth vertex)")
    else:
        print("\nVertex AI authentication requires: pip install google-auth")
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


def get_vertex_credentials() -> tuple[str, str]:
    """Get Google Cloud access token and project for Vertex AI.

    Uses Application Default Credentials (ADC).
    Requires: gcloud auth application-default login

    Returns: (access_token, project_id)
    """
    credentials, project = google_auth_default()

    # Refresh credentials to get access token
    auth_request = GoogleAuthRequest()
    credentials.refresh(auth_request)

    project = (os.environ.get("GOOGLE_CLOUD_PROJECT")
               or os.environ.get("GCP_PROJECT")
               or project)

    if not project:
        error("Could not detect GCP project. "
              "Set GOOGLE_CLOUD_PROJECT environment variable "
              "or run: gcloud config set project PROJECT_ID")

    return credentials.token, project


def model_to_vertex(model: str, provider: str) -> str:
    """Convert model name to Vertex AI format.

    Anthropic models use @ for version dates:
    - API format: claude-sonnet-4-5-20250929
    - Vertex format: claude-sonnet-4-5@20250929

    OpenAI/xAI models need publisher prefix:
    - Vertex requires: openai/gpt-oss-120b-maas

    Other providers use the same format for both.
    """
    if provider == "anthropic":
        # Match pattern: ends with -YYYYMMDD (8 digits)
        if model.count('-') >= 3:
            parts = model.rsplit('-', 1)
            if len(parts) == 2 and len(parts[1]) == 8 and parts[1].isdigit():
                return f"{parts[0]}@{parts[1]}"
    elif provider in ("openai", "xai"):
        # Add publisher prefix if not already present
        if "/" not in model:
            return f"{provider}/{model}"
    return model


def detect_auth_method(provider: str) -> str:
    """Detect authentication method for a provider.

    Args:
        provider: The provider name (e.g., "anthropic", "openai")

    Returns:
        "direct" or "vertex"
    """
    env_var = PROVIDERS[provider]["env_var"]
    if os.environ.get(env_var):
        return "direct"
    if VERTEX_AI_AVAILABLE:
        try:
            credentials, project = google_auth_default()
            if credentials and project:
                return "vertex"
        except GoogleAuthError:
            pass
    return "direct"


def get_auth_string(auth_choice: str, provider: str) -> str:
    """Get authentication string for API requests.

    Args:
        auth_choice: User's auth choice ("auto", "direct", or "vertex")
        provider: Provider name

    Returns:
        Authentication string - either "vertex" or "direct:<api_key>"
    """
    config = PROVIDERS[provider]

    # Determine actual auth method
    if auth_choice == "auto":
        auth_method = detect_auth_method(provider)
    else:
        auth_method = auth_choice

    # Build auth string based on method
    if auth_method == "vertex":
        if not VERTEX_AI_AVAILABLE:
            error("Vertex AI support requires 'google-auth' library. "
                  "Install with: pip install google-auth")
        return "vertex"

    api_key = os.environ.get(config["env_var"])
    if not api_key:
        error(f"{config['env_var']} environment variable not set")
    return f"direct:{api_key}"


def _build_request_meta(
    provider: str, auth: str, model: str, request_data: dict[str, Any]
) -> tuple[str, dict[str, str], dict[str, Any]]:
    """Return (url, headers, request_data) for a provider request.

    Args:
        provider: Provider name
        auth: Authentication string - either "direct:<api_key>" or "vertex"
        model: Model identifier
        request_data: The request payload (may be modified for Vertex)

    Returns:
        Tuple of (url, headers, modified_request_data)
    """
    config = PROVIDERS[provider]

    if auth.startswith("direct:"):
        api_key = auth[7:]
        if provider == "anthropic":
            request_data["model"] = model
            return config["endpoint"], {
                "Content-Type": "application/json",
                "x-api-key": api_key,
                "anthropic-version": "2023-06-01",
            }, request_data
        if provider == "google":
            url = f"{config['endpoint']}/{model}:generateContent?key={api_key}"
            return url, {"Content-Type": "application/json"}, request_data
        # openai, xai
        request_data["model"] = model
        return config["endpoint"], {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}",
        }, request_data

    # Vertex AI authentication
    if auth != "vertex":
        error(f"Invalid auth format: {auth}")

    access_token, project_id = get_vertex_credentials()
    location = os.environ.get("CLOUD_ML_REGION", "global")

    if location == "global":
        vertex_base = "https://aiplatform.googleapis.com"
    else:
        vertex_base = f"https://{location}-aiplatform.googleapis.com"
    vertex_base += f"/v1/projects/{project_id}/locations/{location}"

    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {access_token}",
    }
    vertex_model = model_to_vertex(model, provider)

    if provider == "anthropic":
        request_data["anthropic_version"] = "vertex-2023-10-16"
        url = f"{vertex_base}/publishers/anthropic/models/{vertex_model}:rawPredict"
    elif provider == "google":
        url = f"{vertex_base}/publishers/google/models/{vertex_model}:generateContent"
    else:  # openai, xai
        request_data["model"] = vertex_model
        url = f"{vertex_base}/endpoints/openapi/chat/completions"

    return url, headers, request_data


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
    auth: str,
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

    Args:
        provider: Provider name (anthropic, openai, xai, google)
        auth: Authentication string - either "direct:<api_key>" or "vertex"
        model: Model identifier
        request_data: Provider-specific request payload
        timeout: Request timeout in seconds
        verbose: Show detailed token usage

    Returns:
        Tuple of (response_text, token_usage)
    """
    url, headers, request_data = _build_request_meta(provider, auth, model, request_data)
    body = json.dumps(request_data).encode("utf-8")
    req = Request(url, data=body, headers=headers)

    try:
        with urlopen(req, timeout=timeout) as response:
            result = json.loads(response.read().decode("utf-8"))
    except HTTPError as e:
        error_body = e.read().decode("utf-8")
        try:
            error_data = json.loads(error_body)
            if isinstance(error_data, list) and error_data:
                error_data = error_data[0]
            if isinstance(error_data, dict):
                error(f"API error: {error_data.get('error', error_body)}")
            else:
                error(f"API error: {error_body}")
        except json.JSONDecodeError:
            error(f"API error ({e.code}): {error_body}")
    except URLError as e:
        if isinstance(e.reason, TimeoutError):
            error(f"Request timed out after {timeout} seconds")
        error(f"Connection error: {e.reason}")
    except TimeoutError:
        error(f"Request timed out after {timeout} seconds")

    usage = _extract_usage(provider, result)
    if verbose:
        _print_verbose_usage(usage)
    return _extract_text(provider, result), usage
