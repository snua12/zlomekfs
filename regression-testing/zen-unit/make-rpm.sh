# make-rpm.sh
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of Zen-unit
#
# Zen-unit is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zen-unit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Zen-unit; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

if [ -z "$VERSION" ]; then
	VERSION=`cat VERSION`
fi


if [ -z "$RELEASE" ]; then
	RELEASE=`if [ -f "RELEASE" ]; then cat RELEASE; else echo 0; fi`
fi

if [ -z "$REVISION" ]; then
        REVISION=`svnversion | sed -e 's/\([0-9]*\).*/\1/'`
	if expr "$REVISION" - "$REVISION" >/dev/null 2>&1; then #non-numeric revision
		REVISION=0
	fi
fi

mkdir -p build/RPMS build/BUILD build/SRPMS build/SPECS
expr ${RELEASE} + 1 > RELEASE
rpmbuild -ta zen-unit-${VERSION}.tar.gz --clean --define "_topdir `pwd`/build" --define "REVISION ${REVISION}" --define "VERSION ${VERSION}" --define "RELEASE ${RELEASE}"