#!/bin/bash

# Copyright (c) 2006-2011, Alexis Royer, http://alexis.royer.free.fr/CLI
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
#     * Neither the name of the CLI library project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# First of all, remove the write perm on for groups and others.
chmod -R go-w .

# Then, for each type of extension, set the appropriate permissions.
rw=644
rwx=755
# The '-ro' option permits to set the archive in the read-only mode.
if [ "$1" = "-ro" ] ; then
    rw=444
    rwx=555
fi

checkperm() {
    files=$1
    perms=$2
    echo "Checking permissions of $files files..."
    find . -name "$files" -exec chmod $perms {} \;
}

checkperm "*.check"         $rw
checkperm "*.cpp"           $rw
checkperm "*.css"           $rw
checkperm "*.deps"          $rw
checkperm "*.dot"           $rw
checkperm "*.doxygen"       $rw
checkperm "*.gif"           $rw
checkperm "*.h"             $rw
checkperm "*.html"          $rw
checkperm "*.java"          $rw
checkperm "*.jpg"           $rw
checkperm "*.mak"           $rw
checkperm "*.map"           $rw
checkperm "*.md5"           $rw
checkperm "*.png"           $rw
checkperm "*.rng"           $rw
checkperm "*.sh"            $rwx
checkperm "*.test"          $rw
checkperm "*.txt"           $rw
checkperm "*.xml"           $rw
checkperm "*.xsd"           $rw
checkperm "*.xsl"           $rw
checkperm "Makefile"        $rw
checkperm "package-list"    $rw

# Eventually check every kind of end file is managed
echo "Checking no other kind of files..."
unknown_exts=$(find . -type f -print | sed -e "s/^.*\././" | sed -e "s/^.*\///" | sort | uniq \
    | grep -v "\.check" \
    | grep -v "\.cpp" \
    | grep -v "\.css" \
    | grep -v "\.deps" \
    | grep -v "\.dot" \
    | grep -v "\.doxygen" \
    | grep -v "\.gif" \
    | grep -v "\.h" \
    | grep -v "\.html" \
    | grep -v "\.java" \
    | grep -v "\.jpg" \
    | grep -v "\.mak" \
    | grep -v "\.map" \
    | grep -v "\.md5" \
    | grep -v "\.png" \
    | grep -v "\.rng" \
    | grep -v "\.sh" \
    | grep -v "\.test" \
    | grep -v "\.txt" \
    | grep -v "\.xml" \
    | grep -v "\.xsd" \
    | grep -v "\.xsl" \
    | grep -v "Makefile" \
    | grep -v "package-list")
for unknown_ext in $unknown_exts ; do
    echo "Unknown extension: $unknown_ext"
done

