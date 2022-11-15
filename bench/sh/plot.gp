#!/usr/bin/env gnuplot

set terminal pdf size 16, 11 enhanced color font 'Helvetica,30' linewidth 2
set output 'out.pdf'

# plot titles for each version
title_freeze = "Freeze Compression"
title_role = "Freeze w/ Roles Compression"
title_counter = "Counter Compression"
title_nocompress = "No Compression"

data_folder = "./data/"

# file names for each test samples
test_hs = "half-searches"
test_ia = "insert-all"
test_ra = "remove-all-preinserted"
test_sa = "search-all-preinserted"
test_se = "search-all-empty"

# file suffix for each version
version_f = "-freeze"
version_c = "-counter"
version_n = "-nocompress"
version_r = "-role"

# file prefix for each metric
metric_apl = "apl-"
metric_fail = "fail-"
metric_path = "paths-"
metric_speed = "speedup-"
metric_put = "-put"

# title of plots by test name
label_hs="[25 25 25 25] Half Searches"
label_ia="[100 0 0 0] Reinsert All"
label_ra="[0 100 0 0] Remove All"
label_sa="[0 0 100 0] Search All In Preinserted Map"
label_se="[0 0 0 100] Search All In Empty Map"
label_put=" (Throughput)"
label_apl=" (Average Path Length)"
label_path=" (Paths)"
label_speed=" (Speedup)"
label_fail=" (Failure Rate)"
label_compare=" (Compare Versions)"

# Histogram
set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set grid xtics lt 0 lw 1 lc rgb "#bbbbbb"
set key horiz
set key reverse outside top center Left enhanced spacing 1
set xlabel "Cores"
set ytics auto
set style data histogram
set style histogram clustered gap 2 title textcolor lt -1
set style fill pattern 4
#set style fill solid border 0
set boxwidth 1
set style histogram errorbars lw 2
set linetype 1 linecolor rgb "#4477AA"
set linetype 2 linecolor rgb "#CCBB44"
set linetype 3 linecolor rgb "#AA3377"
set linetype 4 linecolor rgb "#66CCEE"
set linetype 5 linecolor rgb "#EE6677"
set linetype 6 linecolor rgb "#228833"
set linetype 7 linecolor rgb "#E69F00"
set linetype 8 linecolor rgb "#AA3377"

set ylabel "Time [s]"
set title label_hs
plot \
	data_folder.test_hs.version_n using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_hs.version_f using 2:3:xtic(1) t title_freeze, \
	data_folder.test_hs.version_c using 2:3:xtic(1) t title_counter

set title label_ia
plot \
	data_folder.test_ia.version_n using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_ia.version_f using 2:3:xtic(1) t title_freeze, \
	data_folder.test_ia.version_c using 2:3:xtic(1) t title_counter

set title label_ra
plot \
	data_folder.test_ra.version_n using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_ra.version_f using 2:3:xtic(1) t title_freeze, \
	data_folder.test_ra.version_c using 2:3:xtic(1) t title_counter

set title label_sa
plot \
	data_folder.test_sa.version_n using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_sa.version_f using 2:3:xtic(1) t title_freeze, \
	data_folder.test_sa.version_c using 2:3:xtic(1) t title_counter

set title label_se
plot \
	data_folder.test_se.version_n using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_se.version_f using 2:3:xtic(1) t title_freeze, \
	data_folder.test_se.version_c using 2:3:xtic(1) t title_counter

set ylabel "Throughput [operations/s]"
set title label_hs.label_put
plot \
	data_folder.test_hs.version_n.metric_put using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_hs.version_f.metric_put using 2:3:xtic(1) t title_freeze, \
	data_folder.test_hs.version_c.metric_put using 2:3:xtic(1) t title_counter

set title label_ia.label_put
plot \
	data_folder.test_ia.version_n.metric_put using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_ia.version_f.metric_put using 2:3:xtic(1) t title_freeze, \
	data_folder.test_ia.version_c.metric_put using 2:3:xtic(1) t title_counter

set title label_ra.label_put
plot \
	data_folder.test_ra.version_n.metric_put using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_ra.version_f.metric_put using 2:3:xtic(1) t title_freeze, \
	data_folder.test_ra.version_c.metric_put using 2:3:xtic(1) t title_counter

set title label_sa.label_put
plot \
	data_folder.test_sa.version_n.metric_put using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_sa.version_f.metric_put using 2:3:xtic(1) t title_freeze, \
	data_folder.test_sa.version_c.metric_put using 2:3:xtic(1) t title_counter

set title label_se.label_put
plot \
	data_folder.test_se.version_n.metric_put using 2:3:xtic(1) t title_nocompress, \
	data_folder.test_se.version_f.metric_put using 2:3:xtic(1) t title_freeze, \
	data_folder.test_se.version_c.metric_put using 2:3:xtic(1) t title_counter

# Lines
set key reverse inside top center enhanced spacing 1
set key vertical
set xlabel "Cores"
set format x '{%g}'
set xrange [ * : * ] noreverse writeback
set ytics auto
set yrange [ 0 : * ] noreverse writeback
set linetype 1 lw 2 pt 1 ps 2
set linetype 2 lw 2 pt 2 ps 2
set linetype 3 lw 2 pt 3 ps 2
set linetype 4 lw 2 pt 4 ps 2
set linetype 5 lw 2 pt 5 ps 2
set linetype 6 lw 2 pt 6 ps 2
set linetype 7 lw 2 pt 7 ps 2
set linetype 8 lw 2 pt 8 ps 2
set linetype 9 lw 2 pt 9 ps 2
set linetype 10 lw 2 pt 10 ps 2
set xrange [0:33]

set style data yerrorlines
set ylabel "Time [s]"

set title label_hs
plot \
	data_folder.test_hs.version_n using ($1):($2):($3) t title_nocompress, \
	data_folder.test_hs.version_f using ($1):($2):($3) t title_freeze, \
	data_folder.test_hs.version_c using ($1):($2):($3) t title_counter

set title label_ia
plot \
	data_folder.test_ia.version_n using ($1):($2):($3) t title_nocompress, \
	data_folder.test_ia.version_f using ($1):($2):($3) t title_freeze, \
	data_folder.test_ia.version_c using ($1):($2):($3) t title_counter

set title label_ra
plot \
	data_folder.test_ra.version_n using ($1):($2):($3) t title_nocompress, \
	data_folder.test_ra.version_f using ($1):($2):($3) t title_freeze, \
	data_folder.test_ra.version_c using ($1):($2):($3) t title_counter

set title label_sa
plot \
	data_folder.test_sa.version_n using ($1):($2):($3) t title_nocompress, \
	data_folder.test_sa.version_f using ($1):($2):($3) t title_freeze, \
	data_folder.test_sa.version_c using ($1):($2):($3) t title_counter

set title label_se
plot \
	data_folder.test_se.version_n using ($1):($2):($3) t title_nocompress, \
	data_folder.test_se.version_f using ($1):($2):($3) t title_freeze, \
	data_folder.test_se.version_c using ($1):($2):($3) t title_counter

set ylabel "Throughput [operations/s]"
set title label_hs.label_put
plot \
	data_folder.test_hs.version_n.metric_put using ($1):($2):($3) t title_nocompress, \
	data_folder.test_hs.version_f.metric_put using ($1):($2):($3) t title_freeze, \
	data_folder.test_hs.version_c.metric_put using ($1):($2):($3) t title_counter

set title label_ia.label_put
plot \
	data_folder.test_ia.version_n.metric_put using ($1):($2):($3) t title_nocompress, \
	data_folder.test_ia.version_f.metric_put using ($1):($2):($3) t title_freeze, \
	data_folder.test_ia.version_c.metric_put using ($1):($2):($3) t title_counter

set title label_ra.label_put
plot \
	data_folder.test_ra.version_n.metric_put using ($1):($2):($3) t title_nocompress, \
	data_folder.test_ra.version_f.metric_put using ($1):($2):($3) t title_freeze, \
	data_folder.test_ra.version_c.metric_put using ($1):($2):($3) t title_counter

set title label_sa.label_put
plot \
	data_folder.test_sa.version_n.metric_put using ($1):($2):($3) t title_nocompress, \
	data_folder.test_sa.version_f.metric_put using ($1):($2):($3) t title_freeze, \
	data_folder.test_sa.version_c.metric_put using ($1):($2):($3) t title_counter

set title label_se.label_put
plot \
	data_folder.test_se.version_n.metric_put using ($1):($2):($3) t title_nocompress, \
	data_folder.test_se.version_f.metric_put using ($1):($2):($3) t title_freeze, \
	data_folder.test_se.version_c.metric_put using ($1):($2):($3) t title_counter

set ylabel "Similarity with No-Compress"
set yzeroaxis
set ytics axis
set style data linespoints

set yrange [0.825:1.175]
set title label_hs.label_compare
plot \
	"< join ".data_folder.test_hs.version_n." ".data_folder.test_hs.version_n using 1:($4/$2) t title_nocompress, \
	"< join ".data_folder.test_hs.version_f." ".data_folder.test_hs.version_n using 1:($4/$2) t title_freeze, \
	"< join ".data_folder.test_hs.version_c." ".data_folder.test_hs.version_n using 1:($4/$2) t title_counter

set title label_ia.label_compare
plot \
	"< join ".data_folder.test_ia.version_n." ".data_folder.test_ia.version_n using 1:($4/$2) t title_nocompress, \
	"< join ".data_folder.test_ia.version_f." ".data_folder.test_ia.version_n using 1:($4/$2) t title_freeze, \
	"< join ".data_folder.test_ia.version_c." ".data_folder.test_ia.version_n using 1:($4/$2) t title_counter

set title label_ra.label_compare
plot \
	"< join ".data_folder.test_ra.version_n." ".data_folder.test_ra.version_n using 1:($4/$2) t title_nocompress, \
	"< join ".data_folder.test_ra.version_f." ".data_folder.test_ra.version_n using 1:($4/$2) t title_freeze, \
	"< join ".data_folder.test_ra.version_c." ".data_folder.test_ra.version_n using 1:($4/$2) t title_counter

set title label_sa.label_compare
plot \
	"< join ".data_folder.test_sa.version_n." ".data_folder.test_sa.version_n using 1:($4/$2) t title_nocompress, \
	"< join ".data_folder.test_sa.version_f." ".data_folder.test_sa.version_n using 1:($4/$2) t title_freeze, \
	"< join ".data_folder.test_sa.version_c." ".data_folder.test_sa.version_n using 1:($4/$2) t title_counter

set title label_se.label_compare
plot \
	"< join ".data_folder.test_se.version_n." ".data_folder.test_se.version_n using 1:($4/$2) t title_nocompress, \
	"< join ".data_folder.test_se.version_f." ".data_folder.test_se.version_n using 1:($4/$2) t title_freeze, \
	"< join ".data_folder.test_se.version_c." ".data_folder.test_se.version_n using 1:($4/$2) t title_counter
unset yzeroaxis
set ytics auto
set yrange [ 0 : * ] noreverse writeback

set ylabel "Speedup"
set title label_hs.label_speed
plot \
	data_folder.metric_speed.test_hs.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_speed.test_hs.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_speed.test_hs.version_c using ($1):($2) t title_counter

set title label_ia.label_speed
plot \
	data_folder.metric_speed.test_ia.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_speed.test_ia.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_speed.test_ia.version_c using ($1):($2) t title_counter

set title label_ra.label_speed
plot \
	data_folder.metric_speed.test_ra.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_speed.test_ra.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_speed.test_ra.version_c using ($1):($2) t title_counter

set title label_sa.label_speed
plot \
	data_folder.metric_speed.test_sa.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_speed.test_sa.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_speed.test_sa.version_c using ($1):($2) t title_counter

set title label_se.label_speed
plot \
	data_folder.metric_speed.test_se.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_speed.test_se.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_speed.test_se.version_c using ($1):($2) t title_counter

set ylabel "Average path length [hops/lookup]"
set title label_hs.label_apl
plot \
	data_folder.metric_apl.test_hs.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_apl.test_hs.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_apl.test_hs.version_c using ($1):($2) t title_counter

set title label_ia.label_apl
plot \
	data_folder.metric_apl.test_ia.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_apl.test_ia.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_apl.test_ia.version_c using ($1):($2) t title_counter

set title label_ra.label_apl
plot \
	data_folder.metric_apl.test_ra.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_apl.test_ra.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_apl.test_ra.version_c using ($1):($2) t title_counter

set title label_sa.label_apl
plot \
	data_folder.metric_apl.test_sa.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_apl.test_sa.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_apl.test_sa.version_c using ($1):($2) t title_counter

set title label_se.label_apl
plot \
	data_folder.metric_apl.test_se.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_apl.test_se.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_apl.test_se.version_c using ($1):($2) t title_counter

set ylabel "Traversed Paths"
set title label_hs.label_path
plot \
	data_folder.metric_path.test_hs.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_path.test_hs.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_path.test_hs.version_c using ($1):($2) t title_counter

set title label_ia.label_path
plot \
	data_folder.metric_path.test_ia.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_path.test_ia.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_path.test_ia.version_c using ($1):($2) t title_counter

set title label_ra.label_path
plot \
	data_folder.metric_path.test_ra.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_path.test_ra.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_path.test_ra.version_c using ($1):($2) t title_counter

set title label_sa.label_path
plot \
	data_folder.metric_path.test_sa.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_path.test_sa.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_path.test_sa.version_c using ($1):($2) t title_counter

set title label_se.label_path
plot \
	data_folder.metric_path.test_se.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_path.test_se.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_path.test_se.version_c using ($1):($2) t title_counter

set ylabel "Fail rate [retrials/operation]"
set title label_hs.label_fail
plot \
	data_folder.metric_fail.test_hs.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_fail.test_hs.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_fail.test_hs.version_c using ($1):($2) t title_counter

set title label_ia.label_fail
plot \
	data_folder.metric_fail.test_ia.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_fail.test_ia.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_fail.test_ia.version_c using ($1):($2) t title_counter

set title label_ra.label_fail
plot \
	data_folder.metric_fail.test_ra.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_fail.test_ra.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_fail.test_ra.version_c using ($1):($2) t title_counter

set title label_sa.label_fail
plot \
	data_folder.metric_fail.test_sa.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_fail.test_sa.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_fail.test_sa.version_c using ($1):($2) t title_counter

set title label_se.label_fail
plot \
	data_folder.metric_fail.test_se.version_n using ($1):($2) t title_nocompress, \
	data_folder.metric_fail.test_se.version_f using ($1):($2) t title_freeze, \
	data_folder.metric_fail.test_se.version_c using ($1):($2) t title_counter

