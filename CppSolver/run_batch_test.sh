#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SOLVER_DIR="$ROOT_DIR/CppSolver"
OUT_DIR="$ROOT_DIR/CppSolver/batch_logs"
mkdir -p "$OUT_DIR"

cd "$SOLVER_DIR"
echo "[1/3] Building solver..."
g++ -std=c++17 -O2 src/main.cpp src/Solver.cpp -I include -o solver_batch

echo "[2/3] Memory estimate (rough)"
python - <<'PY'
# Rough upper bound estimate for in-memory search footprint
# Assumes average map cell count around 400 tiles and ~5 bytes per tile payload overhead factor folded in.
# Real usage varies by STL allocator and branching behavior.
max_nodes = 20_000_000
avg_tiles = 400
bytes_per_tile = 1
state_core = 80
vector_overhead = 24
hash_overhead = 56
per_state = state_core + vector_overhead + avg_tiles * bytes_per_tile + hash_overhead
for n in [2_000_000, 5_000_000, 20_000_000]:
    gb = per_state * n / (1024**3)
    print(f"  nodes={n:>9,d} -> ~{gb:,.2f} GiB (very rough)")
print("  Note: 16GB machine建议把maxNodes控制在2M~5M，并优先困难子集。")
PY

echo "[3/3] Running batch strategies..."
COMMON=(--difficult-only 1 --max-nodes 5000000 --pruning 1 --deadlock-level 2)

declare -a CMDS=(
  "./solver_batch --strategy 0 --weight 3 ${COMMON[*]}"
  "./solver_batch --strategy 1 --weight 3 --anytime-min-weight 1 --anytime-decay 0.5 ${COMMON[*]}"
  "./solver_batch --strategy 2 --weight 3 --portfolio-pass-count 4 --portfolio-toggle-pruning 1 ${COMMON[*]}"
  "./solver_batch --strategy 3 ${COMMON[*]}"
  "./solver_batch --strategy 4 --weight 3 --ara-min-epsilon 1 --ara-decay 0.8 ${COMMON[*]}"
  "./solver_batch --strategy 5 --weight 3 --mha-secondary-weight 1.5 --mha-anchor-bias 1.2 ${COMMON[*]}"
)

for i in "${!CMDS[@]}"; do
  n=$((i+1))
  logfile="$OUT_DIR/strategy_${i}.log"
  echo "  -> run #$n: ${CMDS[$i]}"
  timeout 300 bash -lc "printf '\n\n\n\n\n\n\n\n\n\n' | ${CMDS[$i]}" > "$logfile" 2>&1 || true
  tail -n 6 "$logfile"
  echo "-----"
done

echo "Done. Logs in $OUT_DIR"
