#!/bin/bash
# do some transformations on files according to SED_expressions
# first arg input directory
# second arg output directory
# [third arg] mask

PWD=`pwd`
DEFAULT_MASK='*.[ch]'

SED_MESSAGE_FILE='s/\(.*\)message[^\\(]*(\([0-9]\)[^,]*,[^,]*,\(.*\)/\1message\ (\2,\ NULL,\3/'

# absolutize
if expr match "$1" "[^/].*" > /dev/null; then
	INDIR="$PWD/$1"
else
	INDIR="$1"
fi
if expr match "$2" "[^/].*" > /dev/null; then
	OUTDIR="$PWD/$2"
else
	OUTDIR="$2"
fi

function print_help(){
	echo 'do some transformations on files ;)'
	echo usage:
	echo $0 '<input dir> <output dir> [mask]	'
}

if [ "$1" = "-h" ]; then
	print_help
	exit 0
fi

if [ ! -z "$3" ]; then
	MASK="$3"
else
	MASK="$DEFAULT_MASK"
fi


cd "${INDIR}" && ls -1 $MASK | \
while read filename; do
	sed -e "$SED_MESSAGE_FILE" \
	< "${INDIR}/$filename" \
	> "${OUTDIR}/$filename"
done