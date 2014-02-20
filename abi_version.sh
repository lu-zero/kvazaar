#!/bin/sh

file="src/version.h"
name="KVAZAAR"
if [ ! -f $file ]; then
    echo "abi_version.sh: error: $file does not exist" 1>&2
    exit 1
fi

eval $(awk "/#define ${name}_VERSION_M/ { print \$2 \"=\" \$3 }" "$file")

case $1 in
    -libtool)
        eval echo -n \$${name}_VERSION_MAJOR:\$${name}_VERSION_MINOR:0
    ;;
    *)
        eval echo -n \$${name}_VERSION_MAJOR.\$${name}_VERSION_MINOR.\$${name}_VERSION_MICRO
    ;;
esac
