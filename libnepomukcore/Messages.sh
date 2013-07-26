#!bin/sh
# fetch all strings in source code files
$XGETTEXT `find . -name \*.cc -o -name \*.cpp -o -name \*.h` -o $podir/libnepomukcore.pot