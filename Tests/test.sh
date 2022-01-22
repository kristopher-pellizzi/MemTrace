EXE_PATH=$(readlink -f "./coreutils_8.32/install/bin")
PIN_PATH=$(readlink -f "../third_party/PIN/pin")
TOOL_PATH=$(readlink -f "../tool")
RESULT_PATH=$(readlink -f "./execution_times.csv")

echo "Utility;Original execution time;Null instrumented execution time;Instrumented execution time;Instrumentation overhead;Analysis overhead; Instrumentation+Analysis overhead" > $RESULT_PATH

# Call a utility with sudo so that it won't request password again later

sudo whoami

# Test dd utility to copy 1K of random data from /dev/urandom (to avoid blocking due to reduced entropy) to a file named test.iso

dd if=/dev/urandom of=./test2.iso iflag=count_bytes count=1K

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dd if=/dev/urandom of=./test.iso iflag=count_bytes count=1K
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/dd if=/dev/urandom of=./test.iso iflag=count_bytes count=1K
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/dd if=/dev/urandom of=./test.iso iflag=count_bytes count=1K
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)

utility="dd"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test b2sum utility to compute checksum of a file of 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/b2sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/b2sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/b2sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="b2sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test base32 utility to compute base32 encoding of a file with 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/base32 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/base32 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/base32 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="base32"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test base64 utility to compute the base64 encoding of a file with 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/base64 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/base64 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/base64 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="base64"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test basename utility to get the basename of the current folder

path=$(pwd)
echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/basename $path
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/basename $path
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/basename $path
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="basename"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test cat utility to concatenate 2 files containing 1K of random data each and print them to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cat ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/cat ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/cat ./test.iso ./test2.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="cat"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test chgrp utility to change owner group of a file to root
cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
sudo $EXE_PATH/chgrp root ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/chgrp root ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/chgrp root ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="chgrp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test chmod utility to change file modes to 700 (all permissions to the owner only)

cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
sudo $EXE_PATH/chmod 700 ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/chmod 700 ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/chmod 700 ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="chmod"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test chown utility to change the owner of a file to root
cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
sudo $EXE_PATH/chown root ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/chown root ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
sudo $PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/chown root ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="chown"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test cksum to compute the checksum of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cksum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/cksum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/cksum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="cksum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test sort utility to sort a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sort ./test.iso -o ./test.sorted
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.sorted

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sort ./test.iso -o ./test.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.sorted

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sort ./test.iso -o ./test.sorted
end_null=$(date +%s%N | cut -b1-13)

$EXE_PATH/sort ./test2.iso -o ./test2.sorted

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sort"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test comm to compare 2 files containing 1K of random data each

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/comm ./test.sorted ./test2.sorted
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/comm ./test.sorted ./test2.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/comm ./test.sorted ./test2.sorted
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="comm"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test cp utility to copy a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cp ./test.iso ./toDelete.tmp
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./toDelete.tmp

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/cp ./test.iso ./toDelete.tmp
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./toDelete.tmp

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/cp ./test.iso ./toDelete.tmp
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./toDelete.tmp

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="cp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test csplit to split a file containing 1K of random data at 3rd line into 2 distinct files

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/csplit ./test.iso 3
end_orig=$(date +%s%N | cut -b1-13)

sudo rm xx00 xx01

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/csplit ./test.iso 3
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm xx00 xx01

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/csplit ./test.iso 3
end_null=$(date +%s%N | cut -b1-13)

sudo rm xx00 xx01

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="csplit"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH

# Test cut utility to extract only first 10 bytes of each line of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cut -b1-10 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/cut -b1-10 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/cut -b1-10 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="cut"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test date utility to print current date to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/date
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/date
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/date
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="date"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test df utility to print to stdout information about file system disk space usage

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/df -h
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/df -h
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/df -h
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="df"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test dir utility to print the list of current directory content to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dir
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/dir
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/dir
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="dir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test dircolors to print the database of set colors to file extensions

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dircolors --print-database
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/dircolors --print-database
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/dircolors --print-database
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="dircolors"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test dirname utility to print the current path except the last directory to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dirname $path
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/dirname $path
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/dirname $path
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="dirname"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test du utility to print information about disk usage of the current directory

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/du -h $path
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/du -h $path
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/du -h $path
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="du"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test echo utility to print 100 B of random data to stdout

random_data=$(cat /dev/urandom | head -c 100)

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/echo $random_data
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/echo $random_data
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/echo $random_data
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="echo"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test env utility to print all set environment variables ti stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/env
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/env
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/env
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="env"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test expand to convert hypothetical tabs to spaces in a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/expand ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/expand ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/expand ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="expand"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH






# Test expr utility to compute a multiplication

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/expr 118462910 \* 123456
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/expr 118462910 \* 123456
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/expr 118462910 \* 123456
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="expr"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test factor utility to factorize a number with 15 digits

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/factor 123456789123459
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/factor 123456789123459
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/factor 123456789123459
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="factor"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test fmt utility to format a file containing 1K of random data such that long lines are splitted and lines contain at most 15 chars

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/fmt -s -w 15 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/fmt -s -w 15 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/fmt -s -w 15 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="fmt"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test fold utility to wrap lines of a file containing 1K of random data into lines of 15 bytes

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/fold -s -b -w 15 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/fold -s -b -w 15 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/fold -s -b -w 15 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="fold"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test getlimits utility to print information about shell limits to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/getlimits
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/getlimits
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/getlimits
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="getlimits"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test groups utility to print the list of available groups in the system to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/groups
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/groups
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/groups
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="groups"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test head to print first 512 bytes of a file containing 1K of random data to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/head -c 512 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/head -c 512 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/head -c 512 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="head"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test hostid to print the numeric identifier of the current host to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/hostid
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/hostid
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/hostid
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="hostid"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test id utility to print user and groupd ids to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/id
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/id
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/id
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="id"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test join utility to join 2 files of 1K of random data each

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/join ./test.sorted ./test2.sorted
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/join ./test.sorted ./test2.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/join ./test.sorted ./test2.sorted
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="join"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test link utility to create a link to a file of 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/link ./test.iso ./test.link
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/link ./test.iso ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/link ./test.iso ./test.link
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="link"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test ln utility to create a symbolic link to a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ln -s ./test.iso ./test.link
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/ln -s ./test.iso ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/ln -s ./test.iso ./test.link
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="ln"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test ls utility to print a list of the current directory content to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ls -al ./Tests/coreutils/src
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/ls -al ./Tests/coreutils/src
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/ls -al ./Tests/coreutils/src
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="ls"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test md5sum utility to compute the md5 hash digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/md5sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/md5sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/md5sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="md5sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test mkdir utility to create a new directory

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mkdir abcdefghijklmnopqrstuvwxyz
end_orig=$(date +%s%N | cut -b1-13)

sudo rm -r abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/mkdir abcdefghijklmnopqrstuvwxyz
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm -r abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/mkdir abcdefghijklmnopqrstuvwxyz
end_null=$(date +%s%N | cut -b1-13)

sudo rm -r abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="mkdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test mkfifo utility to create a named PIPE

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mkfifo abcdefghijklmnopqrstuvwxyz
end_orig=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/mkfifo abcdefghijklmnopqrstuvwxyz
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/mkfifo abcdefghijklmnopqrstuvwxyz
end_null=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="mkfifo"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test mknod utility to create a PIPE

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mknod abcdefghijklmnopqrstuvwxyz p
end_orig=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/mknod abcdefghijklmnopqrstuvwxyz p
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/mknod abcdefghijklmnopqrstuvwxyz p
end_null=$(date +%s%N | cut -b1-13)

sudo rm abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="mknod"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test mktemp utility to create e new temporary file

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mktemp
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/mktemp
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/mktemp
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="mktemp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test mv utility to move a file into another directory

mkdir test_dir

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mv ./test.iso ./test_dir
end_orig=$(date +%s%N | cut -b1-13)

$EXE_PATH/mv ./test_dir/test.iso .

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/mv ./test.iso ./test_dir
end_instrumented=$(date +%s%N | cut -b1-13)

$EXE_PATH/mv ./test_dir/test.iso .

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/mv ./test.iso ./test_dir
end_null=$(date +%s%N | cut -b1-13)

$EXE_PATH/mv ./test_dir/test.iso .

sudo rm -r ./test_dir

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="mv"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test nice utility to run ls command with lowest possible niceness

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nice -n 19 ls -al
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/nice -n 19 ls -al
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/nice -n 19 ls -al
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="nice"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test nl utility to number lines of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nl ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/nl ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/nl ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="nl"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test nproc utility to print the number of available processing units (CPUs) to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nproc
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/nproc
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/nproc
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="nproc"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test numfmt utility to convert numbers from human-readable strings

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/numfmt 123456789 1253 9182746583
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/numfmt 123456789 1253 9182746583
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/numfmt 123456789 1253 9182746583
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="numfmt"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test od to dump a file containing 1K of random data into hexadecimal format

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/od -x ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/od -x ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/od -x ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="od"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# test paste utility to merge lines of 2 files containing 1K of random data each

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/paste ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/paste ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/paste ./test.iso ./test2.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="paste"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test pinky utility to print user information to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/pinky
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/pinky
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/pinky
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="pinky"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test printenv utility to print environment variables to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/printenv
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/printenv
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/printenv
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="printenv"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test printf utility to print a string containing an integer to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/printf "%d is a number, %s is not\n" 1234502839576 ciaone
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/printf $"%d is a number, %s is not\n$" 1234502839576 ciaone
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/printf $"%d is a number, %s is not\n$" 1234502839576 ciaone
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="printf"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


# Test ptx utility to produce a permuted index of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ptx ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/ptx ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/ptx ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="ptx"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test pwd utility to print the current directory to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/pwd
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/pwd
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/pwd
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="pwd"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test readlink utility to print the resolved symbolic link of a file containing 1K of random data

ln -s ./test.iso ./test.link

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/readlink ./test.link
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/readlink ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/readlink ./test.link
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="readlink"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test realpath utility to print the full path of a file in the directory

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/realpath ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/realpath ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/realpath ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="realpath"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH






# Test sudo rm utility to remove a file containing 1K of random data

cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sudo rm ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sudo rm ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sudo rm ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="rm"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test rmdir to remove an empty directory

mkdir ./test_dir

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/rmdir ./test_dir
end_orig=$(date +%s%N | cut -b1-13)

mkdir ./test_dir

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/rmdir ./test_dir
end_instrumented=$(date +%s%N | cut -b1-13)

mkdir ./test_dir

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/rmdir ./test_dir
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="rmdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test seq utility to print the sequence of numbers between 1 and 1000000 to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/seq 1 1000000
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/seq 1 1000000
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/seq 1 1000000
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="seq"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test sha1sum utility to compute SHA1 digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha1sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sha1sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sha1sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sha1sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test sha224sum utility to compute SHA224 digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha224sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sha224sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sha224sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sha224sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test sha256sum utility to compute SHA256 digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha256sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sha256sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sha256sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sha256sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test sha384sum utility to compute SHA384 digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha384sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sha384sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sha384sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sha384sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test sha512sum utility to compute SHA512 digest of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha512sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sha512sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sha512sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sha512sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test shred utility to overwrite a file containing 1K of random data with random content

cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/shred ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/shred ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/shred ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="shred"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test shuf utility to compute a random permutation of lines of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/shuf ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/shuf ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/shuf ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="shuf"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test sleep utility to delay for 1 second

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sleep 1
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sleep 1
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sleep 1
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sleep"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH






# Test split utility to split a file containing 1K of random data into files containing 512 bytes

mkdir test_dir
cd test_dir

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/split -b 512 ../test.iso
end_orig=$(date +%s%N | cut -b1-13)

cd ..
sudo rm -r test_dir
mkdir test_dir
cd test_dir

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/split -b 512 ../test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

cd ..
sudo rm -r test_dir
mkdir test_dir
cd test_dir

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/split -b 512 ../test.iso
end_null=$(date +%s%N | cut -b1-13)

cd ..
sudo rm -r test_dir

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="split"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test stat utility to print information about a file containing 1K of random data to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/stat ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/stat ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/stat ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="stat"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test stty utility to print information about current terminal device characteristics to stdout

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/stty -a
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/stty -a
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/stty -a
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="stty"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test sum utility to compute checksum of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/sum ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test tac utility to concatenate and print 2 files containing 1K of random data each to stdout with reversed order of lines

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tac ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/tac ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/tac ./test.iso ./test2.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="tac"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test tail utility to extract last 512 bytes of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tail -c 512 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/tail -c 512 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/tail -c 512 ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="tail"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test touch utility to create a new file

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/touch ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/touch ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/touch ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="touch"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test truncate utility to shrink the size of a file containing 1K of random data to 512K

cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/truncate -s 512K ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/truncate -s 512K ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy
cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/truncate -s 512K ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

sudo rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="truncate"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test utility tty to print the file name of the device connected to stdin

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tty
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/tty
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/tty
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="tty"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test uname utility to print system information

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uname -a
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/uname -a
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/uname -a
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="uname"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test unexpand utility to convert spaces to tabs in a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/unexpand ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/unexpand ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/unexpand ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="unexpand"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test uniq utility to print only unique lines (not duplicated) in a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uniq -u ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/uniq -u ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/uniq -u ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="uniq"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test unlink utility to remove a file containing 1 M of random data

cp ./test.iso ./test.cpy

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/unlink ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/unlink ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/unlink ./test.cpy
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="unlink"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test uptime to print information about how much time the system is running

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uptime
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/uptime
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/uptime
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="uptime"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test users utility to print users currently logged in

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/users
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/users
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/users
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="users"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH





# Test vdir utility to list the content of the current directory

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/vdir
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/vdir
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/vdir
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="vdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test wc utility to count new lines, words and bytes of a file containing 1K of random data

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/wc ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/wc ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/wc ./test.iso
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="wc"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH



# Test who utility to print who is currently logged in the system

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/who
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/who
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/who
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="who"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH




# Test whoami utility to print the effective user id

echo ""
echo "Starting original"
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/whoami
end_orig=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting memtrace"
start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/MemTrace.so -- $EXE_PATH/whoami
end_instrumented=$(date +%s%N | cut -b1-13)

echo ""
echo "Starting nulltool"
start_null=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t $TOOL_PATH/NullTool.so -- $EXE_PATH/whoami
end_null=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
null_duration=$(expr $end_null - $start_null)
utility="whoami"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo "Null instrumented program execution time: $null_duration ms"
echo

inst_analysis_overhead=$(echo "scale=2; ($instrumented_duration / $orig_duration)" | bc | sed 's/\./,/g')
instrumentation_overhead=$(echo "scale=2; ($null_duration / $orig_duration)" | bc | sed 's/\./,/g')
analysis_overhead=$(echo "scale=2; ($instrumented_duration / $null_duration)" | bc | sed 's/\./,/g')
echo "Total overhead: $inst_analysis_overhead X"
echo

echo "$utility;$orig_duration;$null_duration;$instrumented_duration;$instrumentation_overhead;$analysis_overhead;$inst_analysis_overhead" >> $RESULT_PATH


sudo rm ./test2.iso
sudo rm ./test.sorted
sudo rm ./test2.sorted
sudo rm -r /tmp/*