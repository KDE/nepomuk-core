#! /usr/bin/env bash
$XGETTEXT `find . -name "*.cpp"` -o $podir/nepomukcleaner.pot
rm -rf rc.cpp
