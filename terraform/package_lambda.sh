#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
api_directory="${script_directory}/../api"
build_directory="${script_directory}/.build"
package_directory="${build_directory}/package"
requirements_file="${build_directory}/requirements.txt"

rm -rf "${package_directory}"
mkdir -p "${package_directory}"

# Export the lockfile before installing only Linux x86_64 runtime dependencies.
uv export --project "${api_directory}" --frozen --no-dev --no-editable \
  --format requirements-txt --output-file "${requirements_file}"
uv pip install --python 3.11 --python-platform x86_64-manylinux_2_17 \
  --only-binary :all: --target "${package_directory}" -r "${requirements_file}"

cp "${api_directory}/main.py" "${package_directory}/main.py"
