#!/bin/sh
# Packs the built Flick into flick-<version>-1-<arch>.hpkg.
# Run on Haiku after "make" and "make bindcatalogs":
#
#   sh packaging/make-hpkg.sh 0.1.0

set -e

version="$1"
if [ -z "$version" ]; then
	echo "usage: $0 <version>" >&2
	exit 1
fi

arch=$(getarch)
program=$(ls objects.*/Flick | head -n 1)
staging=objects.package
output="flick-$version-1-$arch.hpkg"

rm -rf "$staging" "$output"
mkdir -p "$staging/apps" "$staging/data/deskbar/menu/Applications"
cp "$program" "$staging/apps/Flick"
ln -s ../../../../apps/Flick "$staging/data/deskbar/menu/Applications/Flick"
sed -e "s/@VERSION@/$version/g" -e "s/@ARCH@/$arch/g" \
	packaging/PackageInfo.in > "$staging/.PackageInfo"

package create -C "$staging" "$output"
package list "$output"
