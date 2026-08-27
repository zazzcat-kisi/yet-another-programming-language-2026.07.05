#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."
node scripts/render_tables.js > /dev/null
cat scripts/report_header.md > REPORT.md
echo "---" >> REPORT.md
echo "" >> REPORT.md
echo "## Results" >> REPORT.md
echo "" >> REPORT.md
cat data/tables.md >> REPORT.md
cat scripts/report_footer.md >> REPORT.md
echo "built REPORT.md ($(wc -l < REPORT.md) lines)"
