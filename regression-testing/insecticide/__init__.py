""" Package with nose plugins used for testing zlomekFS. 

    :Nose plugins api extensions:
        Plugin configuration is stored within plugin class instance. But data connected
        to distinct test instances may not be stored this way. They are stored in test
        instance special attributes. In most cases, there is no need to share them 
        between plugins, but sometimes (as for snapshots) they are used widely.

        First two attributes with specific meaning are snapshot and restore methods, 
        that should be defined for test class. They are used for snapshoting of context 
        of test instance. These methods should be defined by test class writter.

        Attribute snapshotBuffer on test class instance is used to preserve snapshots 
        of test state, snapshots are inserted by SnapshotPlugin, if test fails, they are 
        reported by ZfsReportPlugin to TestResultStorage. 

        Attribute snapshotedObject of TestCase and ContextSuite instance can override 
        what object should be snapshoted for it. Default behavior is to snapshot test 
        class instance.

        ZfsStressGenerator plugin usess attribute failureBuffer on test class instance 
        to store failure reports between runs of the same stress chain with diferrent 
        pruning. This approach is used to report only the shortests version of failed 
        chain, while the last version can success. For control of number of reruns
        there is retryIteration attribute (zero on begining, 1 on first retry, ...).

        ZfsStressGenerator distinguishes metaTests by metaTest attribute. It can be 
        defined on test class, or test method. Test method attribute has precedence, 
        but if it is not specified, the class attribute is used. Set it to True to mark 
        test as meta, set it to False on test to override class attribute.

        If ContextSuite should stop after test and skip further tests, stopContext 
        attribute on TestCase instance should be set to True.

        ZfsReportPlugin stores duration information of test into startTime and endTime 
        attributes on test class instance.
        
    :Usage:
        Simple tests should be written as usual for Nose. Tests for stress testing
        are handled specially, but in general, their format is the same as normal tests,
        just they run on the same instance of their class. 
        
        If zfsReportPlugin is used, preffered way to create batch is beforehand
        by report.generateLocalBatch. It will load profile information too.
"""


__author__ = 'Jiri Zouhar'
