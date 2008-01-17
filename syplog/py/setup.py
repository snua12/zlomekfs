#!/usr/bin/python
from distutils.core import setup, Extension

setup(name="pysyplog",
      version="0.3",
      description="Python interface for syplog",
      author="Jiri Zouhar",
      author_email="zouhar.jiri@gmail.com",
      url="http://www.loki.name/syplog",
      py_modules = ['pysyplog'],
      ext_modules=[Extension("_pysyplog", ["pysyplog_wrap.c"],
                             library_dirs=['../syplog'],
                             libraries=['syplog', 'dbus-1'],
                             include_dirs=['../syplog',
                             '../syplog/media', '../syplog/formatters',
                             '../syplog/control',
                             '/usr/include/dbus-1.0',
                             '/usr/lib/dbus-1.0/include'])
                  ]
)
