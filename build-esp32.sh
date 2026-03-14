#!/usr/bin/env bash
# Build space-ham for the ESP32 platform using ESP-IDF.
#
# Usage: build-esp32.sh [--clean] [--verbose] [--jobs N] [--flash] [--port PORT]

set -euo pipefail

usage()
{
	printf 'Usage: %s [--clean] [--verbose] [--jobs N] [--flash] [--port PORT]\n' "$0"
	printf '\n'
	printf '  --clean        Run idf.py fullclean before building\n'
	printf '  --verbose      Enable verbose build output\n'
	printf '  --jobs N       Number of parallel make jobs (default: nproc)\n'
	printf '  --flash        Flash firmware after successful build\n'
	printf '  --port PORT    Serial port for flashing (default: /dev/ttyUSB0)\n'
	printf '  --baud BAUD    Flash baud rate (default: 921600)\n'
	return 0
}

CLEAN=0
VERBOSE=0
JOBS=$(nproc)
FLASH=0
PORT="/dev/ttyUSB0"
BAUD=3000000

while [ $# -gt 0 ]; do
	case "$1" in
		--clean)
			CLEAN=1
			shift
			;;
		--verbose)
			VERBOSE=1
			shift
			;;
		--jobs)
			JOBS="$2"
			shift 2
			;;
		--flash)
			FLASH=1
			shift
			;;
		--port)
			PORT="$2"
			shift 2
			;;
		--baud)
			BAUD="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			printf 'Unknown option: %s\n' "$1" >&2
			usage >&2
			exit 1
			;;
	esac
done

if [ -z "${IDF_PATH:-}" ]; then
	printf 'IDF_PATH is not set; source the ESP-IDF export.sh before running this script\n' >&2
	exit 1
fi

SCRIPT_DIR="$(dirname "$0")"

cd "$SCRIPT_DIR"

if [ "$CLEAN" -eq 1 ]; then
	idf.py fullclean
fi

IDF_FLAGS=""
if [ "$VERBOSE" -eq 1 ]; then
	IDF_FLAGS="--verbose"
fi

NINJAFLAGS="-j$JOBS" idf.py $IDF_FLAGS build

if [ "$FLASH" -eq 1 ]; then
	if [ "$CLEAN" -eq 1 ]; then
		idf.py --port "$PORT" --baud "$BAUD" flash
	else
		idf.py --port "$PORT" --baud "$BAUD" app-flash
	fi
fi
