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
                             libraries=['syplog'],
                             include_dirs=['../syplog',
                             '../syplog/media', '../syplog/formatters',
                             '../syplog/control'])
                  ]
)
