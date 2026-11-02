#!/bin/bash
# LAB-03 buoc 5 (HOST, terminal 2): ket noi gdbserver trong QEMU va ghi lai toan bo phien debug.
# Truoc do trong QEMU phai chay:  net_up.sh   roi   gdbserver :1234 /usr/bin/buggy_app
A=$HOME/embedded_lab3/apps
L=$HOME/embedded_lab3/logs
OOB=$(grep -n 'sum += data\[i\]' "$A/buggy_app.c" | cut -d: -f1)
AFTER_FREE=$(( $(grep -n '^    free(buf);' "$A/buggy_app.c" | cut -d: -f1) + 1 ))
RES=$(( $(grep -n 'int result = a \* b' "$A/buggy_app.c" | cut -d: -f1) + 1 ))
G=/tmp/lab3_session.gdb
cat > "$G" <<GDB
set pagination off
set confirm off
set print pretty on
set trace-commands on
set logging file $L/gdb_session.txt
set logging overwrite on
set logging enabled on
set architecture arm
file $A/buggy_app
target remote localhost:1234
break main
break process_data
break buggy_app.c:$OOB if i == len
break create_message
break buggy_app.c:$AFTER_FREE
break calculate
break buggy_app.c:$RES
info breakpoints
echo \n===== MAIN =====\n
continue
backtrace
info registers pc sp lr
echo \n===== BUG 1: ARRAY OUT-OF-BOUNDS =====\n
continue
backtrace
info args
print *data@len
up
print arr
print arr[0]@5
print &arr[0]
print &arr[5]
down
continue
info locals
print i
print len
print data[i]
print &data[i]
print sum
echo \n===== BUG 2: USE AFTER FREE =====\n
continue
info args
continue
backtrace
print buf
x/s buf
x/8xw buf
info registers r0 pc lr
echo \n===== BUG 3: INTEGER OVERFLOW =====\n
continue
info args
print a * b
print (long long) a * b
print 2147483647
continue
print result
print/x result
print (long long) a * b - result
backtrace
info registers
x/8xw \$sp
x/6i \$pc
echo \n===== CHAY TOI HET =====\n
continue
GDB
gdb-multiarch -batch -x "$G"
echo
echo "Log da luu: $L/gdb_session.txt ($(wc -l < "$L/gdb_session.txt") dong)"
