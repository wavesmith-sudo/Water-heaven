#!/usr/bin/env bash
set -euo pipefail
BASE_URL="${BASE_URL:-http://127.0.0.1:8000}"
curl --fail --silent --show-error "${BASE_URL}/health" | python3 -m json.tool
printf '
Smoke check only; does not verify authentication, streaming or hardware.
'
