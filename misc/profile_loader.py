#!/usr/bin/python

from profile_default import profile_default
import os
from subprocess import Popen

os.environ.update(profile_default)

make = Popen (args=('make', 'test'), env = os.environ)
make.wait ()