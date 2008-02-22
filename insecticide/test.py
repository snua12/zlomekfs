#!/usr/bin/env python


import os
import sys
import traceback

if 'DJANGO_SETTINGS_MODULE' not in os.environ:
    os.environ['DJANGO_SETTINGS_MODULE'] = 'TestResultStorage.settings'

from insecticide.report import generateLocalBatch, finalizeBatch, branchEnvOpt, reportSystemError


try:
    import pysvn
    entry = pysvn.Client().info('.')
    branch = str(entry.url)[len(entry.repos) + 1:]
    os.environ[branchEnvOpt] = branch
except:
    info = sys.exc_info()
    print e
else:
    info = None

batch = generateLocalBatch('insecticide')

if batch and info:
    reportSystemError(batch, name = info[0].__name__, 
        description = "Pysvn error in test.py", exception = info[1],
        backtrace = info[2])

from nose import main

try:
    res = main(exit=False)
except:
    res = None
    if batch:
        info = sys.exc_info()
        reportSystemError(batch, name = info[0].__name__,
            description = "Unhandled exception in main execution loop",
            exception = info[1], backtrace = info[2])

if batch and batch.id:
    finalizeBatch(batch.id)

sys.exit(not res.success)
