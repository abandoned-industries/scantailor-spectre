#!/bin/bash
# Deploy only a staged copy. Explicit trusted prefixes are required; never
# fall back to the host's package manager or alter OS deployment metadata.
set -euo pipefail
exec python3 "$(dirname "$0")/bundle_closure.py" repair "$@"
