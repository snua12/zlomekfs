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
    name='Zfs plugins',
    version='0.1',
    author='Jiri Zouhar',
    author_email = 'zouhar.jiri@gmail.com',
    description = 'Zfs support plugins for nose',
    license = 'GNU LGPL',
    py_modules = ['zfsConfig',  'graph', 'failure',  'report',  'util', 'zfsStressGenerator'],
    entry_points = {
        'nose.plugins.0.10': [
            'zfsConfig = zfsConfig:ZfsConfig', 
            'zfsStressGeerator = zfsStressGenerator:StressGenerator'
            ]
        }

    )
