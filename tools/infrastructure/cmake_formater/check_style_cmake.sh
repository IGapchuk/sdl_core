#!/usr/bin/env bash
# Copyright (c) 2018 Ford Motor Company,
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of Ford Motor Company nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


FORMATER=cmake-format
INSTALL_CMD="pip install -f $FORMATER"
DIFF_FILE="style_difference.txt"

collect_files() {
    local _files_path=""
    if [ -z $1 ]; then
        _files_path="."
    else
        _files_path=$1
    fi
    local _file_names=$(find "$_files_path" -name \*.cmake -type f -print -o -name CMakeLists.txt -type f -print | grep -Ev $DIRS_TO_EXCLUDE_STR)
    echo "$_file_names"
}

if [ "$1" = "--help" ]
then
    echo    ""
    echo    "Script checks CMake code style in all CMakeLists.txt and .cmake files"
    echo    "Uses $FORMATER as base tool. Install it with : $INSTALL_CMD"
    echo    ""
    echo    "USAGE:"
    echo -e "      \e[1m`basename $0`\e[0m [OPTION1] [OPTION2]"
    echo    ""
    echo -e "      \e[3m[OPTION1]\e[0m"
    echo -e "      \e[1m--\e[4mfix\e[0m   Fix files format. Changes will be wrote in same files."
    echo    "                             Without this oprtion, files will be checked and diff"
    echo    "                             "
    echo -e "              \e[3m[OPTION2]\e[0m"
    echo    "            - /path/to/directory/directory_name  - Will be fixed files only in this directory."
    echo    "            - /path/to/directory/file_name       - Will be fixed only this file."
    echo    ""
    echo -e "      \e[1m--\e[4mhelp\e[0m  Display this information"
    echo    ""
    exit 0
fi

command -v $FORMATER >/dev/null 2>&1 || { echo >&2 "$FORMATER is not installed. Use following: $INSTALL_CMD"; exit 1; }

DIRS_TO_EXCLUDE=(build \
  apache-log4cxx-0.10.0 \
  apr-1.5.0 \
  apr-util-1.5.3 \
  expat-2.1.0 \
  gmock-1.7.0)
DIRS_TO_EXCLUDE_STR=$(IFS='|'; echo "${DIRS_TO_EXCLUDE[*]}")

FILE_NAMES=""

if [ -z $2 ]; then
    # If script will be running without 'OPTION2' argument, it will be recursively checking cmake files in current directory.
    FILE_NAMES="$(collect_files)"
elif [ -d "$2" ]; then
    # If 'OPTION2' argumets will be a directory, formater will be searching cmake files only in specified directory.
    FILE_NAMES="$(collect_files $2)"
else
    # If 'OPTION2' argumets will be a file, formater will be checking only specified file.
    FILE_NAMES=$2
fi

check_style() {
  $FORMATER $1 | diff $1 -
}

fix_style() {
  $FORMATER -i $1
}

if [ "$1" = "--fix" ]; then
  for FILE_NAME in $FILE_NAMES; do
    fix_style $FILE_NAME;
  done
else
  if [ -z $1 ]; then
      echo "Will be checked recursively all files in "$PWD""
  elif [ -f $1 ]; then
      FILE_NAMES=$1
  elif [ -d $1 ]; then
      FILE_NAMES="$(collect_files $1)"
  fi
  PASSED=0
  for FILE_NAME in $FILE_NAMES; do
    echo "$FILE_NAME"
    check_style $FILE_NAME >> $DIFF_FILE
    if [ $? != 0 ]; then
      echo "in " $FILE_NAME >> $DIFF_FILE
      PASSED=1
    fi
  done

  if [ $PASSED != 0 ]; then
      echo -e "\nDifferences has written into \e[3m"$PWD"/"$DIFF_FILE"\e[0m file."
  else
      echo "No differences. CMake style is right."  
  fi
  exit $PASSED
fi
