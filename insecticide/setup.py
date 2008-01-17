#!/usr/bin/python
from distutils.core import setup

setup(name="TestResultStorage",
      version="0.1",
      description="Result repository for insecticide",
      author="Jiri Zouhar",
      author_email="zouhar.jiri@gmail.com",
      url="http://www.loki.name/insecticide",
      py_modules = ['TestResultStorage/__init__', 'TestResultStorage/manage',
              'TestResultStorage/settings', 'TestResultStorage/urls',
              'TestResultStorage/resultRepository/__init__',
              'TestResultStorage/resultRepository/models', 
              'TestResultStorage/resultRepository/views']
)
