#!/bin/bash
# LAB-03 buoc 3: rootfs LAB-03 = rootfs LAB-02 + app + gdbserver + strace + script
set -e
L3=$HOME/embedded_lab3
R=$L3/rootfs/initramfs
sudo rm -rf "$R" && sudo mkdir -p "$R"
sudo cp -a "$HOME/embedded_lab2/rootfs/initramfs/." "$R/"
sudo install -m 755 "$L3"/apps/{buggy_app,leaky_app_arm,perf_target_arm} "$R/usr/bin/"
sudo install -m 755 "$L3"/tools/{gdbserver,strace} "$R/usr/bin/"

sudo tee "$R/usr/bin/net_up.sh" > /dev/null <<'NET'
#!/bin/sh
# Cau hinh mang cho QEMU user-net (bat buoc truoc khi chay gdbserver)
ifconfig lo up
ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up
route add default gw 10.0.2.2
ifconfig eth0
NET

sudo tee "$R/usr/bin/lab3_target.sh" > /dev/null <<'TGT'
#!/bin/sh
# Phan B.5 + C.4 tren target ARM
show() { grep -E 'VmSize|VmRSS|VmData|VmStk|VmExe|VmLib|Threads' /proc/$1/status; }
echo "================ B.5 leaky_app_arm: /proc/PID ================"
leaky_app_arm &
PID=$!
sleep 1
echo "--- T=1s: TRUOC khi cap phat (PID=$PID) ---"
show $PID
echo "--- /proc/$PID/maps ---"
cat /proc/$PID/maps
sleep 4
echo "--- T=5s: SAU khi leak ---"
show $PID
wait $PID
echo "================ C.4 perf_target_arm: time ================"
time perf_target_arm
echo "================ C.4 perf_target_arm: strace -c ================"
strace -c -o /tmp/strace_arm.txt perf_target_arm > /dev/null
cat /tmp/strace_arm.txt
echo "================ HET ================"
TGT
sudo chmod 755 "$R"/usr/bin/{net_up.sh,lab3_target.sh}

( cd "$R" && find . | sudo cpio -H newc -o 2>/dev/null | gzip > "$L3/output/initramfs_lab3.cpio.gz" )
ls -lh "$L3/output/initramfs_lab3.cpio.gz"
zcat "$L3/output/initramfs_lab3.cpio.gz" | cpio -t 2>/dev/null | grep -E 'usr/bin/(buggy|leaky|perf_t|gdbserver|strace|net_up|lab3)'
