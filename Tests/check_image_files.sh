#!/bin/bash
# -*- coding: utf-8, tab-width: 2 -*-


function check_image_files () {
  local SELFPATH="$(readlink -m "$BASH_SOURCE"/..)"
  cd -- "$SELFPATH"/.. || return $?

  local IMG_DIFF="$(diff -sU 0 <(find_images_used) <(find_images_available
    ) | sed -re '1d;2d;/^@@/d' | norm_sort_paths)"
  local IMG_UNUSED=() IMG_MISS=()

  readarray -t IMG_UNUSED < <(<<<"$IMG_DIFF" sed -nre 's!^\+!!p')
  echo "unused images: ${#IMG_UNUSED[@]}"
  printf '  * %s\n' "${IMG_UNUSED[@]}"

  readarray -t IMG_MISS < <(<<<"$IMG_DIFF" sed -nre 's!^\-!!p')
  local N_MISS="${#IMG_MISS[@]}"
  [ "$N_MISS" == 0 ] || exec >&2
  echo "missing images: $N_MISS"
  printf '  ! %s\n' "${IMG_MISS[@]}"

  [ "$N_MISS" == 0 ] || return 4
}


function norm_sort_paths () {
  sed -re '
    s~(^|/)\./~\1~g
    ' | sort --version-sort --unique
}


function find_images_available () {
  local FIND_OPT=(
    .
    -type f
    '(' -false
      -o -name '*.png'
      -o -name '*.svg'
      -o -name '*.jpeg'
      ')'
    )
  find "${FIND_OPT[@]}" | norm_sort_paths
}


function find_images_used () {
  local HTML_FILES=()
  readarray -t HTML_FILES < <(find . -name '*.html')
  grep -HoPe '<img[^<>]*>' -- "${HTML_FILES[@]}" | sed -re '
    s~\s+~ ~g
    s~<img.* src="(\./|)~~
    s~".*$~~
    s~[^/]+:~~
    ' | norm_sort_paths
}










check_image_files "$@"; exit $?
