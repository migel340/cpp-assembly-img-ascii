#!/usr/bin/env bash
set -euo pipefail

BIN=build/img_to_ascii
# Allow overriding image via environment: IMG=/path/to/file ./scripts/full_benchmark.sh
IMG=${IMG:-imgs/test.jpg}
REPS=10
OUT=benchmark_results.csv
SUMMARY=benchmark_summary.txt

# Ensure binary exists
if [ ! -x "$BIN" ]; then
  echo "Binary $BIN not found or not executable. Build first." >&2
  exit 1
fi

# Prepare output files
echo "Test,Run,Edge_ms,HSV_ms,Total_ms" > "$OUT"
echo "Benchmark summary" > "$SUMMARY"
echo "Image: $IMG" >> "$SUMMARY"

declare -A tests

# Define tests: key => flags
tests[noasm_edges_colors]="--edges --no-hsv --no-sobel-asm --no-hsv-asm --colors --no-render"
tests[noasm_edges_colors_hsv]="--edges --hsv --no-sobel-asm --no-hsv-asm --colors --no-render"

# ASM combinations (sobel,hsv)
tests[asm_none]="--edges --no-hsv --no-sobel-asm --no-hsv-asm --colors --no-render"
tests[asm_sobel_only]="--edges --no-hsv --sobel-asm --no-hsv-asm --colors --no-render"
tests[asm_hsv_only]="--edges --hsv --no-sobel-asm --hsv-asm --colors --no-render"
tests[asm_both]="--edges --hsv --sobel-asm --hsv-asm --colors --no-render"

# Helper: extract numeric ms from "Edge Detection: X ms/μs" and TOTAL line
extract_metric() {
  local out="$1"
  local key="$2"
  # look for METRIC lines: METRIC:Key:Value
  local val
  val=$(printf "%s" "$out" | sed -n "s/^METRIC:${key}:\(.*\)$/\1/p" | tail -n1 || true)
  if [ -z "$val" ]; then
    echo "nan"
  else
    echo "$val"
  fi
}

# Run tests
for name in "noasm_edges_colors" "noasm_edges_colors_hsv" "asm_none" "asm_sobel_only" "asm_hsv_only" "asm_both"; do
  flags=${tests[$name]}
  echo "Running test: $name  flags: $flags"
  echo "Test: $name" >> "$SUMMARY"
  sum_edge=0
  sum_total=0
  sum_hsv=0
  count_hsv=0
  for i in $(seq 1 $REPS); do
    echo -n "> run $i... "
    out=$($BIN $IMG $flags 2>&1 || true)
    edge_ms=$(extract_metric "$out" "EdgeDetection_ms")
    hsv_ms=$(extract_metric "$out" "HSV_ms")
    total_ms=$(extract_metric "$out" "TOTAL_ms")
    # fallback: also try lines 'Performance Summary' TOTAL printed as "TOTAL: X ms"
    if [ "$total_ms" = "nan" ]; then
      total_ms=$(printf "%s" "$out" | sed -n 's/.*TOTAL: *\([0-9.]*\) *ms.*/\1/p' | tail -n1 || true)
      if [ -z "$total_ms" ]; then total_ms="nan"; fi
    fi
    printf "%s,%d,%s,%s,%s\n" "$name" "$i" "$edge_ms" "$hsv_ms" "$total_ms" >> "$OUT"
    echo "edge=${edge_ms}ms hsv=${hsv_ms}ms total=${total_ms}ms"
    # accumulate if numeric
    if [ "$edge_ms" != "nan" ]; then
      sum_edge=$(awk -v a="$sum_edge" -v b="$edge_ms" 'BEGIN{printf "%f", a+b}')
    fi
    if [ "$hsv_ms" != "nan" ]; then
      sum_hsv=$(awk -v a="${sum_hsv:-0}" -v b="$hsv_ms" 'BEGIN{printf "%f", a+b}')
      count_hsv=$((count_hsv+1))
    fi
    if [ "$total_ms" != "nan" ]; then
      sum_total=$(awk -v a="$sum_total" -v b="$total_ms" 'BEGIN{printf "%f", a+b}')
    fi
  done
  avg_edge=$(awk -v s="$sum_edge" -v n="$REPS" 'BEGIN{if(n>0) printf "%f", s/n; else print "nan"}')
  if [ "$count_hsv" -gt 0 ]; then
    avg_hsv=$(awk -v s="${sum_hsv:-0}" -v n="$count_hsv" 'BEGIN{if(n>0) printf "%f", s/n; else print "nan"}')
  else
    avg_hsv="nan"
  fi
  avg_total=$(awk -v s="$sum_total" -v n="$REPS" 'BEGIN{if(n>0) printf "%f", s/n; else print "nan"}')
  echo "Average Edge_ms: $avg_edge ms" >> "$SUMMARY"
  echo "Average HSV_ms: $avg_hsv ms" >> "$SUMMARY"
  echo "Average Total_ms: $avg_total ms" >> "$SUMMARY"
  echo >> "$SUMMARY"
done

# Final note
echo "Done. Detailed results: $OUT; summary: $SUMMARY"
