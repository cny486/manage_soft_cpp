"""Call the inventory-enrichment API directly with the same request shape as the app.

The API key is read only from MANAGE_SOFT_AI_API_KEY and is never printed.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request


DEFAULT_API_URL = "https://api.deepseek.com/chat/completions"
DEFAULT_MODEL = "deepseek-v4-pro"
DEFAULT_TIMEOUT_SECONDS = 30


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Test the ManageSoft inventory AI-enrichment request.")
    parser.add_argument("manufacturer_part", help="Manufacturer Part to enrich, for example NE555P")
    parser.add_argument("--api-url", default=os.getenv("MANAGE_SOFT_AI_API_URL", DEFAULT_API_URL))
    parser.add_argument("--model", default=os.getenv("MANAGE_SOFT_AI_MODEL", DEFAULT_MODEL))
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_SECONDS)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    api_key = os.getenv("MANAGE_SOFT_AI_API_KEY", "").strip()
    if not api_key:
        print("MANAGE_SOFT_AI_API_KEY is not set.", file=sys.stderr)
        return 2
    if args.timeout <= 0:
        print("--timeout must be positive.", file=sys.stderr)
        return 2

    fields = [
        "footprint", "value", "manufacturer", "category", "precision", "feature",
    ]
    system_prompt = (
        "Extract concise, high-confidence electronics-component metadata. "
        "Return ONLY one JSON object; no Markdown, explanation, analysis, or reasoning. "
        "Return exactly these six fields: footprint, value, manufacturer, category, precision, feature. "
        "For resistor and capacitor parts, feature must be the Voltage Rating (耐压), including its unit; "
        "do not use Voltage-Supply(Max) for either. For other component categories, feature is a short distinguishing specification. "
        "For a known value, include key, value, sourceTitle, and sourceUrl using an official HTTPS manufacturer/distributor source. "
        "If a value is unavailable, still include that key with value '-' and omit sourceTitle/sourceUrl. "
        "Never guess or attempt extended research. "
        f"Only use these field keys: {', '.join(fields)}. "
        "Use short normalized values."
    )
    user_prompt = (
        f"Manufacturer Part: {args.manufacturer_part.strip()}\n"
        "Current record (reference data only):\n<record>\n(empty)\n</record>\n\n"
        "Identify this exact part and return all six fields in this JSON shape:\n"
        '{"manufacturerPart":"...","provider":"...","fields":['
        '{"key":"manufacturer","value":"...","sourceTitle":"...","sourceUrl":"https://..."}]}'
    )
    request_body = json.dumps({
        "model": args.model.strip(),
        "temperature": 0.2,
        "max_tokens": 1200,
        "thinking": {"type": "disabled"},
        "response_format": {"type": "json_object"},
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ],
    }).encode("utf-8")
    request = urllib.request.Request(
        args.api_url.strip(),
        data=request_body,
        headers={"Content-Type": "application/json", "Authorization": f"Bearer {api_key}"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            payload = response.read().decode("utf-8", "replace")
            print(f"HTTP {response.status}")
            print(json.dumps(json.loads(payload), ensure_ascii=False, indent=2))
            return 0
    except urllib.error.HTTPError as error:
        print(f"HTTP {error.code}", file=sys.stderr)
        print(error.read().decode("utf-8", "replace"), file=sys.stderr)
    except Exception as error:
        print(f"Request failed: {error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
