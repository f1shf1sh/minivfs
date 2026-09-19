#!/bin/sh
set -eu

if [ "$#" -gt 1 ]; then
    printf 'Usage: %s [disk image path]\n' "$0" >&2
    exit 1
fi

project_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
image=${1:-"$project_dir/disk.img"}
case "$image" in
    /*) ;;
    *) image="$(pwd)/$image" ;;
esac

make -C "$project_dir" all
if [ ! -e "$image" ]; then
    "$project_dir/bin/mkfs" "$image"
fi
exec "$project_dir/bin/sh" "$image"
