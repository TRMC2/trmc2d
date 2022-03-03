#!/bin/bash

# Provide a version string to be used by trmc2d when answering the
# "*idn?" command. Ideally, the string has the form
#
#     02468ac (2018-02-28 11:22:33 +0100)
#
# where the first part is the abbreviated commit hash, and in
# parentheses is the commit author date. Alternative forms are used if
# this information is not available or relevant.

# If the commit date is not appropriate, we use instead the date of the
# most recently changed source file.
source_date() {
    local latest_file=$(ls -t Makefile {.,plugins}/*.[ch] | head -1)
    date +'%F %T %z' -r $latest_file
}

# If this working directory comes from an exported archive, the version
# string below has already been filled with the correct version by "git
# archive". We just have to output it as is.
unset Format
version="$Format:%h (%ai)$"
if [[ "$version" != :* ]]; then
    echo $version
    exit
fi

# Otherwise, if this is not a git working tree, or if the git command is
# not available, then the commit is "unknown" and the only dates we have
# are those from the file system.
if ! git status > /dev/null 2>&1; then
    echo "unknown ($(source_date))"
    exit
fi

# If we get here, we know this is a git working tree. Get the hash of
# the last commit.
git_commit=$(git log -n 1 --pretty='%h')

# If the working tree is dirty, append "-dirty" to the commit hash and
# use the date of the most recent source file.
if [ -n "$(git status -uno --porcelain)" ]; then
    echo "$git_commit-dirty ($(source_date))"
    exit
fi

# Otherwise use the commit date.
echo "$git_commit ($(git log -n 1 --pretty='%ai'))"
