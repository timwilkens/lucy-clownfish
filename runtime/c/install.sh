#!/bin/sh

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

version=0.4.0
major_version=0.4

usage()
{
    echo "Usage: install.sh --prefix path"
}

while [ -n "${1+set}" ]; do
    case "$1" in
        -h|--help|-\?)
            usage
            exit
            ;;
        --prefix)
            if [ -z "${2+set}" ]; then
                echo "--prefix requires an argument."
                exit 1
            fi
            prefix=$2
            shift 2
            ;;
        *)
            echo "Invalid option: '$1'" 1>&2
            usage
            exit 1
            ;;
    esac
done

if [ -z "$prefix" ]; then
    echo "No prefix specified."
    usage
    exit 1
fi

if ! mkdir -p "$prefix"; then
    echo "Can't create directory: $prefix"
    exit 1
fi

prefix=`cd "$prefix" && pwd`

# Install libraries.
case `uname` in
    Darwin*)
        lib_file=libcfish.$version.dylib
        if [ ! -f $lib_file ]; then
            echo "$lib_file not found. Did you run make?"
            exit 1
        fi
        mkdir -p "$prefix/lib"
        cp $lib_file "$prefix/lib"
        install_name=$prefix/lib/libcfish.$major_version.dylib
        ln -sf $lib_file "$install_name"
        ln -sf $lib_file "$prefix/lib/libcfish.dylib"
        install_name_tool -id "$install_name" "$prefix/lib/$lib_file"
        ;;
    *)
        lib_file=libcfish.so.$version
        if [ ! -f $lib_file ]; then
            echo "$lib_file not found. Did you run make?"
            exit 1
        fi
        mkdir -p "$prefix/lib"
        cp $lib_file "$prefix/lib"
        soname=libcfish.so.$major_version
        ln -sf $lib_file "$prefix/lib/$soname"
        ln -sf $soname "$prefix/lib/libcfish.so"
        ;;
esac

# Install executables.
mkdir -p "$prefix/bin"
cp ../../compiler/c/cfc "$prefix/bin/cfc"

# Install Clownfish header files.
for src in `find ../core -name '*.cf[hp]'`; do
    file=${src#../core/}
    dest=$prefix/share/clownfish/include/$file
    dir=`dirname "$dest"`
    mkdir -p "$dir"
    cp $src "$dest"
done

# Install man pages.
cp -R autogen/man "$prefix"

# Create pkg-config file.
mkdir -p "$prefix/lib/pkgconfig"
cat <<EOF >"$prefix/lib/pkgconfig/clownfish.pc"
Name: Clownfish
Description: Symbiotic object system
Version: $version
URL: http://lucy.apache.org/
Libs: -L$prefix/lib -lcfish
EOF

