#! /usr/bin/env bash
$XGETTEXT `find . -name "*.cpp" | grep -v '/test/' | grep -v '/migrator/' | grep -v '/backup/gui/'` -o $podir/nepomukstorage.pot
