set terminal png size 1200,600
set output '/home/tung_pham/embedded_lab3/logs/latency_histogram.png'
set title 'Cyclictest Latency Histogram - Standard Kernel (7.0.0-38-generic) + stress-ng'
set xlabel 'Latency (microseconds)'
set ylabel 'Number of occurrences (log scale)'
set logscale y
set xrange [0:200]
set grid
set key top right
set style fill solid 0.4 noborder
plot for [c=2:5] '/home/tung_pham/embedded_lab3/logs/hist_standard.txt' using 1:c with boxes title sprintf('Thread %d', c-2)
