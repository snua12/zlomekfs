#!/usr/bin/env python


import os
import sys
traceback

if 'DJANGO_SETTINGS_MODULE' not in os.environ:
    os.environ['DJANGO_SETTINGS_MODULE'] = 'TestResultStorage.settings'

from insecticide.report import generateLocalBatch, finalizeBatch, branchEnvOpt

try:
    import pysvn
    entry = pysvn.Client().info('.')
    branch = str(entry.url)[len(entry.repos) + 1:len(entry.url) - len('tests/nose-tests') - 1]
    os.environ[branchEnvOpt] = branch
except:
    print traceback.format_exc()

batch = generateLocalBatch('zfs')

from nose import main

res = main(exit=False)

if batch and batch.id:
    finalizeBatch(batch.id)

sys.exit(not res.success)
