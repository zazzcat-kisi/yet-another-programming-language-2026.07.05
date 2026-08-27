#!/usr/bin/env bash
# Fetch a sample of ~10 respectably large real-world source files per language/framework.
# Usage: fetch_corpus.sh <manifest.tsv> <corpus_dir> <clone_dir> [name_filter]
set -uo pipefail
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
MANIFEST="$1"; CORPUS="$2"; CLONES="$3"; FILTER="${4:-}"
mkdir -p "$CORPUS" "$CLONES"

MIN_BYTES=6000
MAX_BYTES=200000
MIN_LINES=120
WANT=10
MAX_PER_DIR=3

pick_files() {  # $1=root $2=exts(csv) $3=allow_tests $4=extra exclude regex
  local root="$1" exts="$2" allow="$3" excl="${4:-}"
  local findargs=() first=1
  IFS=',' read -ra E <<< "$exts"
  for e in "${E[@]}"; do
    if [ $first -eq 1 ]; then findargs+=( -name "*.${e}" ); first=0
    else findargs+=( -o -name "*.${e}" ); fi
  done
  find "$root" -type f \( "${findargs[@]}" \) -printf '%s\t%p\n' 2>/dev/null \
  | awk -F'\t' -v mn=$MIN_BYTES -v mx=$MAX_BYTES '$1>=mn && $1<=mx' \
  | { if [ "$allow" = "1" ]; then cat; else grep -Eiv '(^|/)(tests?|specs?|__tests__|testdata|test_data|fixtures?|node_modules|vendor|third_party|dist|build|generated|gen|\.git)/' \
      | grep -Eiv '(_test|\.test|\.spec|_spec|-test|\.min|\.generated|_pb2?|\.pb)\.'; fi; } \
  | grep -Eiv '(^|/)(resources?/data|locales?|i18n|intl|translations?|emoji|icu|unicode|snapshots?|vendored?|third[-_]?party)/' \
  | grep -Eiv '/(ucd|unicode-data)[^/]*$' \
  | { if [ -n "$excl" ]; then grep -Eiv "$excl"; else cat; fi; } \
  | sort -t$'\t' -k1,1nr
}

while IFS=$'\t' read -r name kind repo sparse exts allow exclude; do
  [ "$name" = "name" ] && continue
  [ -z "${name:-}" ] && continue
  if [ -n "$FILTER" ] && [ "$name" != "$FILTER" ]; then continue; fi
  slug=$(echo "$name" | tr 'A-Z' 'a-z' | sed 's/#/sharp/g; s/+/p/g; s/[^a-z0-9]\+/_/g; s/^_//; s/_$//')
  out="$CORPUS/$kind/$slug"
  have=$(ls -1 "$out" 2>/dev/null | grep -v '^\.' | wc -l)
  want=$((WANT - have))
  if [ "$want" -le 0 ]; then echo "SKIP $name (already have $have files)"; continue; fi
  work="$CLONES/$slug"
  rm -rf "$work"
  echo "=== $name <- $repo (sparse: $sparse)"
  if [ "$sparse" = "-" ]; then
    timeout 900 git clone --quiet --depth 1 "$repo" "$work" </dev/null >/dev/null 2>&1 || { echo "FAIL clone $name"; continue; }
    root="$work"
  else
    timeout 900 git clone --quiet --depth 1 --filter=blob:none --sparse "$repo" "$work" </dev/null >/dev/null 2>&1 || { echo "FAIL clone $name"; continue; }
    IFS='|' read -ra P <<< "$sparse"
    ( cd "$work" && timeout 900 git sparse-checkout set "${P[@]}" </dev/null >/dev/null 2>&1 ) || { echo "FAIL sparse $name"; }
    root="$work"
  fi
  mkdir -p "$out"
  declare -A dircount=()
  n=0
  while IFS=$'\t' read -r sz path; do
    [ -z "${path:-}" ] && continue
    lines=$(wc -l < "$path" 2>/dev/null || echo 0)
    [ "$lines" -lt $MIN_LINES ] && continue
    maxlen=$(awk '{ if (length($0)>m) m=length($0) } END{print m+0}' "$path")
    [ "$maxlen" -gt 2000 ] && continue
    node "$SCRIPTDIR/datafilter.js" "$path" 2>/dev/null | grep -q ' DATA$' && continue
    d=$(dirname "$path")
    c=${dircount[$d]:-0}
    [ "$c" -ge $MAX_PER_DIR ] && continue
    dircount[$d]=$((c+1))
    rel=${path#$root/}
    dest="$out/$(echo "$rel" | sed 's#/#__#g')"
    [ -e "$dest" ] && continue
    cp "$path" "$dest"
    echo "$rel" >> "$out/.sources"
    n=$((n+1))
    [ "$n" -ge $want ] && break
  done < <(pick_files "$root" "$exts" "$allow" "${exclude:-}")
  echo "$repo" > "$out/.repo"
  echo "  -> $n files"
  [ "$n" -lt 5 ] && echo "WARN $name only $n files"
  rm -rf "$work"
  unset dircount
done < "$MANIFEST"
echo "DONE $MANIFEST"
