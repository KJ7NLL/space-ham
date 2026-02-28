#!/usr/bin/env bash
# Build space-ham for the Linux platform.
#
# Usage: build-linux.sh [--clean] [--verbose] [--jobs N]

set -euo pipefail

usage()
{
	printf 'Usage: %s [--clean] [--verbose] [--jobs N]\n' "$0"
	printf '\n'
	printf '  --clean        Remove build directory before configuring\n'
	printf '  --verbose      Enable verbose make output\n'
	printf '  --jobs N       Number of parallel make jobs (default: nproc)\n'
	return 0
}

CLEAN=0
VERBOSE=0
JOBS=$(nproc)

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

BUILD_DIR="$(dirname "$0")/build-linux"

if [ "$CLEAN" -eq 1 ]; then
	rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Debug"

cmake $CMAKE_FLAGS -S "$(dirname "$0")" -B "$BUILD_DIR"

MAKE_FLAGS="--jobs=$JOBS"
if [ "$VERBOSE" -eq 1 ]; then
	MAKE_FLAGS="$MAKE_FLAGS VERBOSE=1"
fi

make -C "$BUILD_DIR" $MAKE_FLAGS
