#!/usr/bin/env python
# encoding: utf-8
#
# This file is part of BeRTOS.
#
# Bertos is free software; you can redistribute it and/or modify
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As a special exception, you may use this file as part of a free software
# library without restriction.  Specifically, if other files instantiate
# templates or use macros or inline functions from this file, or you compile
# this file and link it with other files to produce an executable, this
# file does not by itself cause the resulting executable to be covered by
# the GNU General Public License.  This exception does not however
# invalidate any other reasons why the executable file might be covered by
# the GNU General Public License.
#
# Copyright 2008 Develer S.r.l. (http://www.develer.com/)
#
#
# Author: Jack Bradach <jack@bradach.net
#

from PyQt4.QtCore import PYQT_VERSION_STR
import re

# Extract our Major/Minor/Release version values from the version string.
m = re.search('(?P<ver_major>\d+)\.(?P<ver_minor>\d+)(.(?P<ver_release>\d+))?', PYQT_VERSION_STR)
ver_major   = int(m.group('ver_major'))
ver_minor   = int(m.group('ver_minor'))

# PYQT_VERSION_STR doesn't append a .0 to initial releases,
# so we'll assume that if there were only two numbers in
# the version string that this is X.Y.0.
if (m.group('ver_release')):
    ver_release = int(m.group('ver_release'))
else:
    ver_release = 0

if ((ver_major <= 4) and (ver_minor <= 3) and (ver_release <= 3)):
    from qvariant_converter_old import *
elif ((ver_major <= 4) and (ver_minor > 3) and (ver_minor < 5)):
    from qvariant_converter_4_4 import *
elif ((ver_major <= 4) and (ver_minor >= 5) and (ver_minor < 6)):
    from qvariant_converter_4_5 import *
else:
    from qvariant_converter_4_6 import *
