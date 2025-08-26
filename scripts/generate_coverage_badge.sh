#!/bin/bash

# Script to generate coverage badge information
# Usage: ./generate_coverage_badge.sh

set -e

# Check if coverage data exists
if [ ! -f "target/coverage/coverage_filtered.info" ]; then
    echo "Error: Coverage data not found. Run 'make coverage' first."
    exit 1
fi

# Calculate coverage percentage from detailed list to ensure consistency with comment script
coverage_data=$(lcov --list target/coverage/coverage_filtered.info 2>/dev/null | grep -A 100 "src/]" | tail -n +2 | grep -E "^[^=|].*\|")

# Create temporary file to store the data
temp_file=$(mktemp)

echo "$coverage_data" | while IFS='|' read -r filename line_info func_info branch_info; do
    # Clean up the filename
    filename=$(echo "$filename" | sed 's/^ *//g' | sed 's/ *$//g')
    
    # Skip empty lines or header lines
    if [ -n "$filename" ] && [ "$filename" != "Total:" ] && [[ "$filename" != *"="* ]]; then
        # Extract line coverage numbers
        line_percent=$(echo "$line_info" | grep -o '[0-9.]*%' | head -1)
        line_total=$(echo "$line_info" | grep -o '[0-9]*' | tail -1)
        
        # Default to 0 if no data found
        [ -z "$line_total" ] && line_total="0"
        
        # Calculate executed counts (only if we have valid data)
        if [ "$line_total" != "0" ] && [ -n "$line_percent" ]; then
            line_executed=$(echo "$line_percent $line_total" | awk '{printf "%.0f", ($1/100) * $2}')
        else
            line_executed=0
        fi
        
        # Store the data
        echo "$line_executed $line_total" >> "$temp_file"
    fi
done

# Calculate totals from the temp file
total_lines_executed=0
total_lines_total=0

while read -r line_executed line_total; do
    total_lines_executed=$((total_lines_executed + line_executed))
    total_lines_total=$((total_lines_total + line_total))
done < "$temp_file"

# Clean up
rm -f "$temp_file"

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