#!/usr/bin/env bash
set -euo pipefail
BIN=build/img_to_ascii
IMG=imgs/test.jpg
REPS=10
for mode in "--asm-off" "--asm-on"; do
  for t in 1 4 8; do
    echo "MODE=${mode} THREADS=${t}"
    vals=()
    for i in $(seq 1 $REPS); do
      out=$($BIN $IMG $mode --threads $t 2>&1)
      line=$(printf "%s" "$out" | sed -n 's/.*Edge Detection: *\([0-9.]*\) *\([μm]s\).*/\1 \2/p')
      if [ -z "$line" ]; then
        echo "(run $i) Edge Detection line not found"
        echo "$out" | sed -n '1,120p'
        exit 1
      fi
      num=$(printf "%s" "$line" | awk '{print $1}')
      unit=$(printf "%s" "$line" | awk '{print $2}')
      if [ "$unit" = "μs" ]; then
        ms=$(awk -v v="$num" 'BEGIN{printf "%f", v/1000.0}')
      else
        ms=$(awk -v v="$num" 'BEGIN{printf "%f", v}')
      fi
      vals+=("$ms")
      printf " run %2d: %8s ms\n" "$i" "$ms"
    done
    sum=0
    for v in "${vals[@]}"; do
      sum=$(awk -v a="$sum" -v b="$v" 'BEGIN{printf "%f", a+b}')
    done
    avg=$(awk -v s="$sum" -v n="$REPS" 'BEGIN{printf "%f", s/n}')
    echo " Average Edge Detection: ${avg} ms"
    echo
  done
done
