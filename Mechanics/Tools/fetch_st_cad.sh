#!/usr/bin/env bash
# Re-fetch the vendor STEP models in Mechanics/Cad from ST.
#
# The models are committed, so this is only needed to pick up a newer board
# revision or to check that what is committed is what ST ships. ST publishes the
# 3D models inside the board manufacturing packs, not on the CAD Resources tab.
#
# curl does not work here: st.com sits behind Akamai, which drops curl's
# connection without a status code (both HTTP/2 and HTTP/1.1, any user agent).
# wget gets through. That is the whole reason this script exists.

set -euo pipefail

readonly BASE_URL="https://www.st.com/resource/en/board_manufacturing_specification"
readonly USER_AGENT="Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0"
readonly CAD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../Cad" && pwd)"

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

fetch() {
    wget --quiet --tries=2 --timeout=60 --user-agent="${USER_AGENT}" \
        -O "${work_dir}/$1" "${BASE_URL}/$1"
}

echo "fetching the Nucleo pack (48 MB) ..."
fetch mb1841-manufacturing.zip
echo "fetching the display shield pack (15 MB) ..."
fetch mb1642_manufacturing_v3-0.zip

unzip -o -j "${work_dir}/mb1841-manufacturing.zip" \
    "MB1841D/Mechanical/MB1841-U545REQ-D01.step" -d "${CAD_DIR}"
unzip -o -j "${work_dir}/mb1642_manufacturing_v3-0.zip" \
    "MB1642D/Mechanical/*.step" -d "${CAD_DIR}"

echo
echo "in ${CAD_DIR}:"
ls -l "${CAD_DIR}"
