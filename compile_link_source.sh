#!/bin/bash
if [ "$#" -ne 1 ]
then
    echo Please provide a source file path
else
    SOURCE_NAME=$1
    EXECUTABLE_NAME=$(basename $SOURCE_NAME .c)
    echo Compiling $SOURCE_NAME into $EXECUTABLE_NAME
    gcc $SOURCE_NAME -lssh -lutil -o $EXECUTABLE_NAME
fi