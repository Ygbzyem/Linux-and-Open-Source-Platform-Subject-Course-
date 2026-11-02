#!/bin/bash
# LAB-03 buoc 8: dong goi bai nop theo muc 5.1 + goi log gui cho Claude viet bao cao
set -e
L3=$HOME/embedded_lab3
OUT=$HOME/SE204386_PhamMinhTung_Lab03
rm -rf "$OUT" && mkdir -p "$OUT"/{apps,output,logs,bao_cao}
cp "$L3"/apps/{buggy_app.c,buggy_app,leaky_app.c,leaky_app,leaky_app_fixed,perf_target.c} "$OUT/apps/"
cp "$L3/output/initramfs_lab3.cpio.gz" "$OUT/output/"
for f in gdb_session.txt valgrind_report.txt valgrind_fixed.txt strace_summary.txt perf_report.txt \
         perf_stat.txt hist_standard.txt latency_histogram.png; do
  [ -f "$L3/logs/$f" ] && cp "$L3/logs/$f" "$OUT/logs/" || echo "!! THIEU logs/$f"
done
[ -f "$L3/logs/flamegraph.svg" ] && cp "$L3/logs/flamegraph.svg" "$OUT/logs/"
echo "Dat file bao cao PDF vao: $OUT/bao_cao/SE204386_Lab03_BaoCao.pdf roi chay lai buoc nay"
tar czf "$HOME/SE204386_PhamMinhTung_Lab03.tar.gz" -C "$HOME" SE204386_PhamMinhTung_Lab03
tar tzf "$HOME/SE204386_PhamMinhTung_Lab03.tar.gz"

# Goi log de gui cho Claude (khong co perf.data cho nhe)
tar czf "$HOME/lab3_logs_for_report.tar.gz" -C "$L3" --exclude='perf.data*' logs
ls -lh "$HOME/lab3_logs_for_report.tar.gz"
