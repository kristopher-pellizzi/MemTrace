EXE_PATH="./Tests/coreutils/src"
PIN_PATH="/opt/pin"

user=$(whoami)

if [ "$user" != "root" ]; then
	echo "You must execute as root"
	exit 1
fi

echo "Utility;Original execution time;Instrumented execution time;Slowdown" > ./Tests/execution_times.csv

# Test dd utility to copy 1M of random data from /dev/urandom (to avoid blocking due to reduced entropy) to a file named test.iso

dd if=/dev/urandom of=./test2.iso iflag=count_bytes count=1M

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dd if=/dev/urandom of=./test.iso iflag=count_bytes count=1M
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/dd if=/dev/urandom of=./test.iso iflag=count_bytes count=1M
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="dd"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test b2sum utility to compute checksum of a file of 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/b2sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/b2sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="b2sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test base32 utility to compute base32 encoding of a file with 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/base32 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/base32 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="base32"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test base64 utility to compute the base64 encoding of a file with 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/base64 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/base64 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="base64"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test basename utility to get the basename of the current folder

path=$(pwd)
start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/basename $path
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/basename $path
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="basename"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test cat utility to concatenate 2 files containing 1M of random data each and print them to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cat ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/cat ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="cat"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test chgrp utility to change owner group of a file to root

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/chgrp root ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/chgrp root ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="chgrp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test chmod utility to change file modes to 700 (all permissions to the owner only)

cp ./test.iso ./test.cpy

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/chmod 700 ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.cpy
cp ./test.iso ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/chmod 700 ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="chmod"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test chown utility to change the owner of a file to root

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/chown root ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/chown root ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="chown"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test cksum to compute the checksum of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cksum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/cksum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="cksum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test sort utility to sort a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sort ./test.iso -o ./test.sorted
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.sorted

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sort ./test.iso -o ./test.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

$EXE_PATH/sort ./test2.iso -o ./test2.sorted

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="expand"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test comm to compare 2 files containing 1M of random data each

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/comm ./test.sorted ./test2.sorted
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/comm ./test.sorted ./test2.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="comm"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test cp utility to copy a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cp ./test.iso ./toDelete.tmp
end_orig=$(date +%s%N | cut -b1-13)

rm ./toDelete.tmp

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/cp ./test.iso ./toDelete.tmp
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./toDelete.tmp

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="cp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test csplit to split a file containing 1M of random data at 3rd line into 2 distinct files

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/csplit ./test.iso 3
end_orig=$(date +%s%N | cut -b1-13)

rm xx00 xx01

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/csplit ./test.iso 3
end_instrumented=$(date +%s%N | cut -b1-13)

rm xx00 xx01

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="csplit"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv

# Test cut utility to extract only first 10 bytes of each line of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/cut -b1-10 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/cut -b1-10 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="cut"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test date utility to print current date to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/date
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/date
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="date"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test df utility to print to stdout information about file system disk space usage

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/df -h
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/df -h
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="df"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test dir utility to print the list of current directory content to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dir
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/dir
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="dir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test dircolors to print the database of set colors to file extensions

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dircolors --print-database
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/dircolors --print-database
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="dircolors"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test dirname utility to print the current path except the last directory to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/dirname $path
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/dirname $path
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="dirname"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test du utility to print information about disk usage of the current directory

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/du -h $path
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/du -h $path
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="du"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test echo utility to print 100 B of random data to stdout

random_data=$(cat /dev/urandom | head -c 100)

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/echo $random_data
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/echo $random_data
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="echo"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test env utility to print all set environment variables ti stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/env
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/env
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="env"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test expand to convert hypothetical tabs to spaces in a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/expand ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/expand ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="expand"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv






# Test expr utility to compute a multiplication

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/expr 118462910 \* 123456
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/expr 118462910 \* 123456
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="expr"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test factor utility to factorize a number with 15 digits

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/factor 123456789123459
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/factor 123456789123459
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="factor"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test fmt utility to format a file containing 1M of random data such that long lines are splitted and lines contain at most 15 chars

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/fmt -s -w 15 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/fmt -s -w 15 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="fmt"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test fold utility to wrap lines of a file containing 1M of random data into lines of 15 bytes

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/fold -s -b -w 15 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/fold -s -b -w 15 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="fold"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test getlimits utility to print information about shell limits to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/getlimits
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/getlimits
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="getlimits"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test groups utility to print the list of available groups in the system to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/groups
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/groups
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="groups"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test head to print first 512 bytes of a file containing 1M of random data to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/head -c 512 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/head -c 512 ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="head"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test hostid to print the numeric identifier of the current host to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/hostid
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/hostid
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="hostid"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test id utility to print user and groupd ids to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/id
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/id
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="id"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test join utility to join 2 files of 1M of random data each

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/join ./test.sorted ./test2.sorted
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/join ./test.sorted ./test2.sorted
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="join"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test link utility to create a link to a file of 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/link ./test.iso ./test.link
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.link

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/link ./test.iso ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="link"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test ln utility to create a symbolic link to a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ln -s ./test.iso ./test.link
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.link

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/ln -s ./test.iso ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="ln"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test ls utility to print a list of the current directory content to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ls -al ./Tests/coreutils/src
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/ls -al ./Tests/coreutils/src
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="ls"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test md5sum utility to compute the md5 hash digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/md5sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/md5sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="md5sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test mkdir utility to create a new directory

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mkdir abcdefghijklmnopqrstuvwxyz
end_orig=$(date +%s%N | cut -b1-13)

rm -r abcdefghijklmnopqrstuvwxyz

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/mkdir abcdefghijklmnopqrstuvwxyz
end_instrumented=$(date +%s%N | cut -b1-13)

rm -r abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="mkdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test mkfifo utility to create a named PIPE

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mkfifo abcdefghijklmnopqrstuvwxyz
end_orig=$(date +%s%N | cut -b1-13)

rm abcdefghijklmnopqrstuvwxyz

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/mkfifo abcdefghijklmnopqrstuvwxyz
end_instrumented=$(date +%s%N | cut -b1-13)

rm abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="mkfifo"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test mknod utility to create a PIPE

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mknod abcdefghijklmnopqrstuvwxyz p
end_orig=$(date +%s%N | cut -b1-13)

rm abcdefghijklmnopqrstuvwxyz

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/mknod abcdefghijklmnopqrstuvwxyz p
end_instrumented=$(date +%s%N | cut -b1-13)

rm abcdefghijklmnopqrstuvwxyz

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="mknod"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test mktemp utility to create e new temporary file

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mktemp
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/mktemp
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="mktemp"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test mv utility to move a file into another directory

mkdir test_dir

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/mv ./test.iso ./test_dir
end_orig=$(date +%s%N | cut -b1-13)

$EXE_PATH/mv ./test_dir/test.iso .

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/mv ./test.iso ./test_dir
end_instrumented=$(date +%s%N | cut -b1-13)

$EXE_PATH/mv ./test_dir/test.iso .

rm -r ./test_dir

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="mv"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test nice utility to run ls command with lowest possible niceness

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nice -n 19 ls -al
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/nice -n 19 ls -al
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="nice"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test nl utility to number lines of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nl ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/nl ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="nl"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test nproc utility to print the number of available processing units (CPUs) to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/nproc
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/nproc
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="nproc"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test numfmt utility to convert numbers from human-readable strings

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/numfmt 123456789 1253 9182746583
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/numfmt 123456789 1253 9182746583
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="numfmt"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test od to dump a file containing 1M of random data into hexadecimal format

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/od -x ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/od -x ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="od"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# test paste utility to merge lines of 2 files containing 1M of random data each

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/paste ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/paste ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="paste"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test pinky utility to print user information to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/pinky
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/pinky
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="pinky"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test printenv utility to print environment variables to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/printenv
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/printenv
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="printenv"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test printf utility to print a string containing an integer to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/printf "%d is a number, %s is not\n" 1234502839576 ciaone
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/printf "%d is a number, %s is not\n" 1234502839576 ciaone
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="printf"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


# Test ptx utility to produce a permuted index of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/ptx ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/ptx ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="ptx"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test pwd utility to print the current directory to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/pwd
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/pwd
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="pwd"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test readlink utility to print the resolved symbolic link of a file containing 1M of random data

ln -s ./test.iso ./test.link

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/readlink ./test.link
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/readlink ./test.link
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.link

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="readlink"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test realpath utility to print the full path of a file in the directory

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/realpath ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/realpath ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="realpath"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv






# Test rm utility to remove a file containing 1M of random data

cp ./test.iso ./test.cpy

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/rm ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/rm ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="rm"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test rmdir to remove an empty directory

mkdir ./test_dir

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/rmdir ./test_dir
end_orig=$(date +%s%N | cut -b1-13)

mkdir ./test_dir

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/rmdir ./test_dir
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="rmdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test seq utility to print the sequence of numbers between 1 and 1000000 to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/seq 1 1000000
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/seq 1 1000000
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="seq"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test sha1sum utility to compute SHA1 digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha1sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha1sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sha1sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test sha224sum utility to compute SHA224 digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha224sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha224sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sha224sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test sha256sum utility to compute SHA256 digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha256sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha256sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sha256sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test sha384sum utility to compute SHA384 digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha384sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha384sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sha384sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test sha512sum utility to compute SHA512 digest of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha512sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha512sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sha512sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test shred utility to overwrite a file containing 1M of random data with random content

cp ./test.iso ./test.cpy

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sha224sum ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.cpy
cp ./test.iso ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sha224sum ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="shred"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test shuf utility to compute a random permutation of lines of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/shuf ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/shuf ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="shuf"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test sleep utility to delay for 1 second

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sleep 1
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sleep 1
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sleep"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv






# Test split utility to split a file containing 1M of random data into files containing 512 bytes

mkdir test_dir
cd test_dir

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/split -b 512 ../test.iso
end_orig=$(date +%s%N | cut -b1-13)

cd ..
rm -r test_dir
mkdir test_dir
cd test_dir

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/split -b 512 ../test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

cd ..
rm -r test_dir

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="split"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test stat utility to print information about a file containing 1M of random data to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/stat ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/stat ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="stat"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test stty utility to print information about current terminal device characteristics to stdout

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/stty -a
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/stty -a
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="stty"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test sum utility to compute checksum of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/sum ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/sum ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="sum"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test tac utility to concatenate and print 2 files containing 1M of random data each to stdout with reversed order of lines

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tac ./test.iso ./test2.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/tac ./test.iso ./test2.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="tac"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test tail utility to extract last 512 bytes of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tail -c 512 ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/tail -c ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="tail"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test touch utility to create a new file

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/touch ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/touch ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="touch"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test truncate utility to shrink the size of a file containing 1M of random data to 512K

cp ./test.iso ./test.cpy

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/truncate -s 512K ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

rm ./test.cpy
cp ./test.iso ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/truncate -s 512K ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

rm ./test.cpy

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="truncate"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test utility tty to print the file name of the device connected to stdin

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/tty
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/tty
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="tty"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test uname utility to print system information

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uname -a
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/uname -a
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="uname"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test unexpand utility to convert spaces to tabs in a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/unexpand ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/unexpand ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="unexpand"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test uniq utility to print only unique lines (not duplicated) in a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uniq -u ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/uniq -u ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="uniq"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test unlink utility to remove a file containing 1 M of random data

cp ./test.iso ./test.cpy

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/unlink ./test.cpy
end_orig=$(date +%s%N | cut -b1-13)

cp ./test.iso ./test.cpy

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/unlink ./test.cpy
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="unlink"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test uptime to print information about how much time the system is running

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/uptime
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/uptime
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="uptime"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test users utility to print users currently logged in

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/users
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/users
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="users"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv





# Test vdir utility to list the content of the current directory

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/vdir
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/vdir
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="vdir"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test wc utility to count new lines, words and bytes of a file containing 1M of random data

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/wc ./test.iso
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/wc ./test.iso
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="wc"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv



# Test who utility to print who is currently logged in the system

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/who
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/who
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="who"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv




# Test whoami utility to print the effective user id

start_orig=$(date +%s%N | cut -b1-13) # Get start time in milliseconds
$EXE_PATH/whoami
end_orig=$(date +%s%N | cut -b1-13)

start_instrumented=$(date +%s%N | cut -b1-13)
$PIN_PATH/pin -t obj-intel64/MemTrace.so -- $EXE_PATH/whoami
end_instrumented=$(date +%s%N | cut -b1-13)

orig_duration=$(expr $end_orig - $start_orig)
instrumented_duration=$(expr $end_instrumented - $start_instrumented)
utility="whoami"

echo
echo "Original program execution time: $orig_duration ms"
echo "Instrumented program execution time: $instrumented_duration ms"
echo

slowdown=$(echo "scale=2; (($instrumented_duration - $orig_duration) / $orig_duration) * 100" | bc)
echo "Slowdown: $slowdown %"
echo

echo "$utility;$orig_duration;$instrumented_duration;$slowdown" >> ./Tests/execution_times.csv


rm ./test.iso
rm ./test2.iso
rm ./test.sorted
rm ./test2.sorted
rm -r /tmp/*