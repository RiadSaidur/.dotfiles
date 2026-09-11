#!/usr/bin/env bash
# Runtime bench for shadow-cat — CPU% (jiffies) + RSS. Reference for perf bake-off.
set -euo pipefail
DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$DIR/shadow-cat"
SAMPLES="${1:-4}"
SLEEP_S="${2:-3}"

make -C "$DIR" -s

pkill -x shadow-cat 2>/dev/null || true
sleep 0.1
setsid "$BIN" </dev/null >/tmp/shadow-cat-bench.log 2>&1 &
sleep 1

pid=""
for _ in $(seq 1 20); do
  pid=$(python3 - <<'PY'
import json,subprocess
try:
 j=json.loads(subprocess.check_output(['hyprctl','layers','-j'],text=True))
except Exception:
 raise SystemExit
for mon,data in j.items():
 for arr in data.get('levels',{}).values():
  for L in arr:
   if L.get('namespace')=='shadow-cat':
    print(L['pid']); raise SystemExit
PY
)
  [[ -n "$pid" ]] && break
  sleep 0.25
done

if [[ -z "$pid" ]]; then
  echo "FAIL: shadow-cat layer not found"
  exit 2
fi

python3 - <<PY
import time
pid=int("$pid")
samples=int("$SAMPLES")
sleep_s=float("$SLEEP_S")

def cpu_rss(p):
    st=open(f'/proc/{p}/stat').read().split()
    rss=int(open(f'/proc/{p}/status').read().split('VmRSS:')[1].split()[0])
    return int(st[13])+int(st[14]), rss

vals=[]
rss_vals=[]
for i in range(samples):
    a,_=cpu_rss(pid)
    time.sleep(sleep_s)
    b,rss=cpu_rss(pid)
    vals.append((b-a)/sleep_s)
    rss_vals.append(rss)

avg=sum(vals)/len(vals)
rss=sum(rss_vals)/len(rss_vals)
print(f"pid={pid}")
print(f"cpu_pct_avg={avg:.3f}")
print(f"rss_kb_avg={rss:.0f}")
print(f"cpu_samples={','.join(f'{v:.3f}' for v in vals)}")
print(f"ok_layer=1")
PY
