#!/bin/bash

# Script to generate coverage badge information
# Usage: ./generate_coverage_badge.sh

set -e

# Check if coverage data exists
if [ ! -f "target/coverage/coverage_filtered.info" ]; then
    echo "Error: Coverage data not found. Run 'make coverage' first."
    exit 1
fi

# Get overall line coverage percentage
coverage_percent=$(lcov --summary target/coverage/coverage_filtered.info 2>/dev/null | grep "lines" | grep -o '[0-9]*\.[0-9]*%' | head -1 | sed 's/%//')

# If no decimal percentage found, try integer percentage
if [ -z "$coverage_percent" ]; then
    coverage_percent=$(lcov --summary target/coverage/coverage_filtered.info 2>/dev/null | grep "lines" | grep -o '[0-9]*%' | head -1 | sed 's/%//')
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