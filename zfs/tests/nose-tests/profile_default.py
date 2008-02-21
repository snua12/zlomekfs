
import os

env = {
    'PROFILE_NAME' : __name__,
    'NOSE_TESTMATCH' : '((?:^|[\\b_\\./-])[Mm]eta)|((?:^|[\\b_\\./-])[Tt]est)',
    'NOSE_EVAL_ATTR' : 'not disabled',
    'NOSE_WITH_ZFSCONFIG' : 'yes',
    'ZFS_CONFIG_FILE' : 'zfs_test_config',
    'NOSE_WITH_ZFSSTRESSGENERATOR' : 'yes',
    'STRESS_TEST_LENGTH' : '10',
    'STRESS_TESTS_BY_CLASS' : '1',
    'STRESS_RETRIES_AFTER_FAILURE' : '0',
    'DJANGO_SETTINGS_MODULE' : 'TestResultStorage.settings',
    'NOSE_WITH_SNAPSHOTPLUGIN' : 'yes',
    'NOSE_WITH_ZFSREPORTPLUGIN' : 'yes',
      }

try:
    path = os.environ['PATH']
except KeyError:
    path = '/bin:/usr/bin'

path += ':/sbin:/usr/sbin'

env['PATH'] = path
