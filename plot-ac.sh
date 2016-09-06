#!/bin/sh
cat << EOF | gnuplot -p
set xdata time
set timefmt "%Y-%m-%d %H:%M:%S"
set format x "%d/%m\n%H:%M"
plot \
	"$1" using 1:4 title "$1" with line, \
	"$1" using 1:(\$5*10) title "$1" with line
EOF
