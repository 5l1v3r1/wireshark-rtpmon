#!/bin/bash
#
# $Id$

# Fuzz-testing script for Tethereal
#
# This script uses Editcap to add random errors ("fuzz") to a set of
# capture files specified on the command line.  It runs Tethereal on
# each fuzzed file and checks for errors.  The files are processed
# repeatedly until an error is found.

# Tweak the following to your liking.  Editcap must support "-E".
TETHEREAL=./tethereal
EDITCAP=./editcap
CAPINFOS=./capinfos

# This needs to point to a 'date' that supports %s.
DATE=/bin/date

# Where our temp files are saved (editcap.out and stderr.out)
TMP_DIR=/tmp

TETHEREAL_ARGS="-nVxr"

# These may be set to your liking
MAX_CPU_TIME=900
MAX_VMEM=500000
ERR_PROB=0.02

ulimit -S -t $MAX_CPU_TIME -v $MAX_VMEM

# Make sure we have a valid test set
FOUND=0
for CF in "$@" ; do
    $CAPINFOS "$CF" > /dev/null 2>&1 && FOUND=1
    if [ $FOUND -eq 1 ] ; then break ; fi
done

if [ $FOUND -eq 0 ] ; then
    cat <<FIN
Error: No valid capture files found.

Usage: `basename $0` capture file 1 [capture file 2]...
FIN
    exit 1
fi

echo "Running $TETHEREAL with args: $TETHEREAL_ARGS"
echo ""

# Iterate over our capture files.
PASS=0
while [ 1 ] ; do
    PASS=`expr $PASS + 1`
    echo "Pass $PASS:"

    for CF in "$@" ; do
	echo -n "    $CF: "

	$CAPINFOS "$CF" > /dev/null 2>&1
	if [ $? -ne 0 ] ; then
	    echo "Not a valid capture file"
	    continue
	fi

	DISSECTOR_BUG=0

	$EDITCAP -E $ERR_PROB "$CF" $TMP_DIR/editcap.out > /dev/null 2>&1
	if [ $? -ne 0 ] ; then
	    $EDITCAP -E $ERR_PROB -T ether "$CF" $TMP_DIR/editcap.out > /dev/null 2>&1
	    if [ $? -ne 0 ] ; then
		echo "Invalid format for editcap"
		continue
	    fi
	fi

	$TETHEREAL $TETHEREAL_ARGS $TMP_DIR/editcap.out \
		> /dev/null 2> $TMP_DIR/stderr.out
	RETVAL=$?
	grep -i "dissector bug" $TMP_DIR/stderr.out \
	    > /dev/null 2>&1 && DISSECTOR_BUG=1
	if [ $RETVAL -ne 0 -o $DISSECTOR_BUG -ne 0 ] ; then
	    SUF=`$DATE +%s`
	    echo " ERROR"
	    echo -e "Processing failed.  Capture info follows:\n"
	    mv $TMP_DIR/editcap.out $TMP_DIR/editcap.out.$SUF
	    echo "  Output file: $TMP_DIR/editcap.out.$SUF"
	    if [ $DISSECTOR_BUG -ne 0 ] ; then
		echo -e "stderr follows:\n"
		cat $TMP_DIR/stderr.out
	    fi
	    exit 1
	fi
	echo " OK"
    done
done

