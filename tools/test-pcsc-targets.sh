#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

TARGETS="dreambox-dm900 vuplus-4k dreambox-one dreambox-mipsel"

for target in $TARGETS; do
    printf "\n============================================================\n"
    printf "PC/SC target test: %s\n" "$target"
    printf "============================================================\n"
    ./build.sh build "$target" --pcsc on --clean
done

printf "\nALL PC/SC TARGET BUILDS COMPLETED\n"
