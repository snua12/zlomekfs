#!/usr/bin/python

"""
Zfs test accessible plugin,
zfs stress generator plugin,

"""
try:
    import ez_setup
    ez_setup.use_setuptools()
except ImportError:
    pass

from setuptools import setup

setup(
    name='insecticide',
    version='0.2',
    author='Jiri Zouhar',
    author_email = 'zouhar.jiri@gmail.com',
    description = 'Zfs support plugins for nose',
    license = 'GNU LGPL',
    package_dir = {'insecticide': '.'},
    packages = ['insecticide'],
#    py_modules = [
#                    'zfsConfig',
#                    'snapshotPlugin',
#                    'zfsStressGenerator', 
#                    'zfsReportPlugin', 
#                    'graph',
#                    'failure', 
#                    'report',
#                    'snapshot',
#                    'util'],
    entry_points = {
        'nose.plugins.0.10': [
            'zfsConfig = insecticide.zfsConfig:ZfsConfig', 
            'zfsStressGeerator = insecticide.zfsStressGenerator:StressGenerator', 
            'snapshotPlugin = insecticide.snapshotPlugin:SnapshotPlugin', 
            'zfsReportPlugin = insecticide.zfsReportPlugin:ZfsReportPlugin'
            ]
        }

    )
