#!/usr/bin/env python3
#
# Purple is the legal property of its developers, whose names are too numerous
# to list here.  Please refer to the COPYRIGHT file distributed with this
# source distribution.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA

"""
Produce meson-config.h in a build directory.

This should not really be run manually. It is used by Meson as a
post-configuration script to create meson-config.h which for now simply
contains information about the configuration used to create the build
directory.
"""

import html
import json
import os
import shlex
import subprocess
import sys


try:
    introspect = os.environ['MESONINTROSPECT']
except KeyError:
    print('Meson is too old; '
          'it does not set MESONINTROSPECT for postconf scripts.')
    sys.exit(1)
else:
    introspect = shlex.split(introspect)

try:
    build_root = os.environ['MESON_BUILD_ROOT']
except KeyError:
    print('Meson is too old; '
          'it does not set MESON_BUILD_ROOT for postconf scripts.')
    sys.exit(1)


def tostr(obj):
    if isinstance(obj, str):
        return html.escape(repr(obj))
    else:
        return repr(obj)


conf = subprocess.check_output(introspect + ['--buildoptions', build_root],
                               universal_newlines=True)
conf = json.loads(conf)

settings = ' '.join('{}={}'.format(option['name'], tostr(option['value']))
                    for option in sorted(conf, key=lambda x: x['name']))

with open(os.path.join(build_root, 'meson-config.h'), 'w') as f:
    f.write('#define MESON_ARGS "{}"'.format(settings))
