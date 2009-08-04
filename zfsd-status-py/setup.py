#!/usr/bin/python
from  setuptools import setup, Extension

setup(name="zfsd_status",
      version="0.1",
      description="Python export of zfsd dbus status iface descriptors",
      author="Jiri Zouhar",
      author_email="zouhar.jiri@gmail.com",
      url="http://www.shiva.ms.mff.cuni.cz/zfs",
      py_modules = ['zfsd_status'],
      ext_modules=[Extension("_zfsd_status", ["zfsd_status_wrap.c", "zfsd_status.c"],
                             libraries=['dbus-1'],
                             include_dirs=['/usr/include',
                             '/usr/include/dbus-1.0',
                             '/usr/lib/dbus-1.0/include'])
                  ]
)
