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
    name='Nose plugin tester',
    version='0.1001',
    author='Jiri Zouhar',
    author_email = 'zouhar.jiri@gmail.com',
    description = 'Plugin test',
    license = 'GNU LGPL',
    py_modules = ['PluginTest'],
    entry_points = {
        'nose.plugins.0.10': [
            'pluginTest = PluginTest:PluginTest'
            ]
        }

    )
