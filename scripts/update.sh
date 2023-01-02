#!/bin/sh

ROOT=../hello_world/exchange/
for file in $(find . -type f | grep -v '\.git'); do 
  SRC=$ROOT/$file
  if [[ -f $SRC ]]; then
    if ! diff $file $SRC >/dev/null; then
      echo "updating $file FROM $SRC"
      cp $SRC $file
    fi
  fi
done
