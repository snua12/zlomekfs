
profile_default =   
            {
            'NOSE_TESTMATCH' : '((?:^|[\\b_\\./-])[Mm]eta)|((?:^|[\\b_\\./-])[Tt]est)',
            'NOSE_EVAL_ATTR' : 'not disabled',
            'NOSE_WITH_ZFSCONFIG' : 'yes',
            'ZFS_CONFIG_FILE' : 'zfs_test_config',
            'NOSE_WITH_ZFSSTRESSGENERATOR' : 'yes',
            'STRESS_TEST_LENGTH' : 10,
            'STRESS_TESTS_BY_CLASS' : 1,
            'STRESS_RETRIES_AFTER_FAILURE' : 0,
            'DJANGO_SETTINGS_MODULE' : 'settings',
            'NOSE_WITH_SNAPSHOTPLUGIN' : 'yes',
            'NOSE_WITH_ZFSREPORTPLUGIN' : 'yes',
            'PATH' : ['/bin', '/usr/bin', '/sbin', '/usr/sbin']
            }