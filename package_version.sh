#!/bin/sh

if [ -d .git ]; then
    #  Retrieve the version from the last git tag.
    VER=`git describe --always | sed -e "s:v::"`
    if [ x"`git diff-index --name-only HEAD`" != x ]; then
        #  If the sources have been changed locally, add -dirty to the version.
        VER="${VER}-dirty"
    fi
elif [ -f .version ]; then
    #  If git is not available (e.g. when building from source package)
    #  we can extract the package version from .version file.
    VER=`< .version`
else
    #  The package version cannot be retrieved.
    VER="Unknown"
fi

printf '%s' "$VER"


