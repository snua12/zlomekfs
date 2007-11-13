"""
Test of nose plugin

"""
try:
    import ez_setup
    ez_setup.use_setuptools()
except ImportError:
    pass

from setuptools import setup

setup(
    name='Zfs config plugin',
    version='0.0',
    author='Jiri Zouhar',
    author_email = 'zouhar.jiri@gmail.com',
    description = 'Example plugin test',
    license = 'GNU LGPL',
    py_modules = ['zfsConfig',  'graph',  'util', 'generator'],
    entry_points = {
        'nose.plugins.0.10': [
            'zfsConfig = zfsConfig:ZfsPlugin'
            ]
        }

    )
