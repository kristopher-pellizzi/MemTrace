make_cmd="make PIN_ROOT=/opt/pin"

if [ "$1" = "DEBUG" ]; then
    echo "DEBUG"
    $make_cmd debug/MemTrace.so
    $make_cmd launchDebug
else
    echo "NOT DEBUG"
    $make_cmd obj-intel64/MemTrace.so
    $make_cmd radareTest
fi
