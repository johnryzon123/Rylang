#!/bin/bash

# --- Color Definitions ---
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BLUE='\033[34m'
CYAN='\033[36m'
BOLD='\033[1m'
RESET='\033[0m'

SEARCH_DIRS="backend interp middleend modules main.cpp"

# 2. Check for changes in the last 30 seconds
HEADER_CHANGES=$(find $SEARCH_DIRS -name "*.h" -o -name "*.hpp" -newermt "30 seconds ago" 2>/dev/null | wc -l)
SOURCE_CHANGES=$(find $SEARCH_DIRS -name "*.cpp" -newermt "30 seconds ago" 2>/dev/null | wc -l)

echo -e "${BLUE}${BOLD}--- Ry Build System ---${RESET}"

if [ "$HEADER_CHANGES" -gt 0 ]; then
    echo -e "🏗️  ${YELLOW}Detected $HEADER_CHANGES header changes.${RESET} Full rebuild risk: ${BOLD}Using -j1${RESET}"
    JOBS=1
elif [ "$SOURCE_CHANGES" -gt 0 ]; then
    echo -e "⚡ ${CYAN}Detected $SOURCE_CHANGES source changes.${RESET} Incremental build: ${BOLD}Using -j2${RESET}"
    JOBS=2
else
    echo -e "♻️  ${BLUE}No recent changes found.${RESET} Using -j1 for stability."
    JOBS=1
fi

# Execute Build
cmake --build build -j $JOBS

# 6. Success Check with Colors
if [ $? -eq 0 ]; then
    echo -e "\n${GREEN}${BOLD}✅ Build complete.${RESET}"
else
    echo -e "\n${RED}${BOLD}❌ Build failed.${RESET} Check the errors above."
    exit 1
fi