#!/bin/bash

# Script to generate coverage badge information
# Usage: ./generate_coverage_badge.sh

set -e

# Check if coverage data exists
if [ ! -f "target/coverage/coverage_filtered.info" ]; then
    echo "Error: Coverage data not found. Run 'make coverage' first."
    exit 1
fi

# Parse coverage data directly from the .info file to ensure consistency with comment script
total_lines_executed=0
total_lines_total=0

while IFS= read -r line; do
    case $line in
        SF:*)
            current_file=${line#SF:}
            ;;
        LH:*)
            if [[ "$current_file" == */src/* ]]; then
                total_lines_executed=$((total_lines_executed + ${line#LH:}))
            fi
            ;;
        LF:*)
            if [[ "$current_file" == */src/* ]]; then
                total_lines_total=$((total_lines_total + ${line#LF:}))
            fi
            ;;
    esac
done < target/coverage/coverage_filtered.info

# Calculate overall percentage
if [ "$total_lines_total" -gt 0 ]; then
    coverage_percent=$(echo "$total_lines_executed $total_lines_total" | awk '{printf "%.1f", ($1/$2)*100}')
else
    coverage_percent="0.0"
fi

# Determine badge color based on coverage percentage
coverage_int=$(echo "$coverage_percent" | cut -d'.' -f1)
if [ "$coverage_int" -ge 80 ]; then
    color="brightgreen"
elif [ "$coverage_int" -ge 60 ]; then
    color="yellow"
elif [ "$coverage_int" -ge 40 ]; then
    color="orange"
else
    color="red"
fi

echo "Coverage: ${coverage_percent}% (Color: $color)"
echo "Badge URL: https://img.shields.io/badge/coverage-${coverage_percent}%25-${color}"