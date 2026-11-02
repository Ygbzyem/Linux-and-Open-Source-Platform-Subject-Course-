#!/bin/bash
# LAB-03 buoc 7 (HOST): Phan D - cyclictest co tai stress-ng + bieu do gnuplot
L=$HOME/embedded_lab3/logs
{ uname -a; echo; grep -E 'CONFIG_PREEMPT|CONFIG_HZ=|CONFIG_HIGH_RES' "/boot/config-$(uname -r)"; } \
  | tee "$L/kernel_info.txt"

echo "== Tao tai (40s) va do latency (10000 vong x 1ms) =="
stress-ng --cpu "$(nproc)" --io 4 --vm 2 --vm-bytes 256M --timeout 40s > "$L/stress_ng.txt" 2>&1 &
SP=$!
sleep 3
sudo cyclictest --mlockall --threads=4 --priority=80 --interval=1000 --distance=500 \
     --loops=10000 --histogram=200 --histfile="$L/hist_standard.txt" -q \
     | tee "$L/cyclictest_standard.txt"
wait $SP
sudo chown "$USER:$USER" "$L"/hist_standard.txt

echo "== Thong ke =="
grep -E '^# (Min|Avg|Max) Latencies|^# Histogram Overflows|^# Total' "$L/hist_standard.txt" | tee "$L/latency_summary.txt"
awk '!/^#/ {t+=$2; if ($1<50) g+=$2} END {if (t) printf "Thread0: %.1f%% mau < 50us\n", g/t*100}' \
    "$L/hist_standard.txt" | tee -a "$L/latency_summary.txt"

cat > "$L/plot_latency.gnuplot" <<GP
set terminal png size 1200,600
set output '$L/latency_histogram.png'
set title 'Cyclictest Latency Histogram - Standard Kernel ($(uname -r)) + stress-ng'
set xlabel 'Latency (microseconds)'
set ylabel 'Number of occurrences (log scale)'
set logscale y
set xrange [0:200]
set grid
set key top right
set style fill solid 0.4 noborder
plot for [c=2:5] '$L/hist_standard.txt' using 1:c with boxes title sprintf('Thread %d', c-2)
GP
gnuplot "$L/plot_latency.gnuplot" && ls -lh "$L/latency_histogram.png"
