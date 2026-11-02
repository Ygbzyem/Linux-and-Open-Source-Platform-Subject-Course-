#!/bin/bash
# LAB-03 buoc 6 (HOST): Phan B (Valgrind) + Phan C (strace, perf) - luu toan bo log
A=$HOME/embedded_lab3/apps
L=$HOME/embedded_lab3/logs
cd "$A"

echo "== B.3 Valgrind truoc khi sua =="
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes \
         --verbose --log-file="$L/valgrind_report.txt" ./leaky_app
echo "== B.4 Valgrind sau khi sua =="
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
         --log-file="$L/valgrind_fixed.txt" ./leaky_app_fixed
grep -E 'in use at exit|total heap usage|definitely|indirectly|possibly|still reachable|ERROR SUMMARY|All heap' \
     "$L/valgrind_report.txt" "$L/valgrind_fixed.txt"

echo "== C.2 strace =="
strace -o "$L/strace_full.txt" ./perf_target > /dev/null
echo "Tong so dong syscall: $(wc -l < "$L/strace_full.txt")" | tee "$L/strace_count.txt"
strace -c -o "$L/strace_summary.txt" ./perf_target > /dev/null
cat "$L/strace_summary.txt"
strace -e trace=file,write,read ./perf_target 2>&1 | head -30 > "$L/strace_filtered.txt"
{ time ./perf_target > /dev/null ; } 2> "$L/time_host.txt"; cat "$L/time_host.txt"
/usr/bin/time -v ./perf_target > /dev/null 2> "$L/time_verbose_host.txt" || true

echo "== C.3 perf =="
sudo perf record -g -o "$L/perf.data" ./perf_target > /dev/null
sudo perf report -i "$L/perf.data" --stdio > "$L/perf_report.txt" 2>/dev/null
sudo perf report -i "$L/perf.data" --stdio --no-children --sort symbol,dso 2>/dev/null \
     | grep -E '^ +[0-9]' | head -15 | tee "$L/perf_top.txt"
sudo perf stat ./perf_target 2>&1 >/dev/null | tee "$L/perf_stat.txt"

echo "== C.3 bonus: flame graph =="
[ -d "$HOME/FlameGraph" ] || git clone --depth 1 https://github.com/brendangregg/FlameGraph "$HOME/FlameGraph" || true
if [ -d "$HOME/FlameGraph" ]; then
  sudo perf record -F 99 -g -o /tmp/perf_flame.data ./perf_target > /dev/null
  sudo perf script -i /tmp/perf_flame.data 2>/dev/null > /tmp/perf_out.txt
  "$HOME/FlameGraph/stackcollapse-perf.pl" /tmp/perf_out.txt > /tmp/perf_folded.txt
  "$HOME/FlameGraph/flamegraph.pl" /tmp/perf_folded.txt > "$L/flamegraph.svg" && echo "flamegraph.svg OK"
fi
sudo chown -R "$USER:$USER" "$L"
