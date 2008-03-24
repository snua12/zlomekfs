""" Module with SnapshotPlugin for nose.
    This plugin provides snapshoting functionality
    (tests can describe what is their state and plugin will
    store the state upon defined circumstances.
"""

import os
import textwrap
import logging
import tempfile
import datetime

snapshotRedirectAttrName = 'snapshotedObject'
""" Name of attribute of test which if specified is snapshoted
    instead of 'inst'
"""

from optparse import OptionConflictError
from warnings import warn
from nose.plugins import Plugin
from logging.handlers import RotatingFileHandler

from insecticide.snapshot import SnapshotDescription
from insecticide.zfsStressGenerator import ChainedTestCase

log = logging.getLogger ("nose.plugins.snapshotPlugin")
""" Logger used in this module. """

class SnapshotPlugin(Plugin):
    """ Snapshot creation plugn.
        Plugin for nose. If enabled, creates snapshots of test state
        in predefined times.
        
        :Configuration Options:
            maxSnapshots: how many old snapshots to preserve (1 = only the last)
            snapshotsRootDir: root dir where snapshot directories 
                should be created (for example /tmp)
            snapshotNoseLog: if set to True, nose log will be appended to every
                snapshot
                
            All options have both environment and parameter configuration option,
            parameter is always stronger.
            
        .. See: nose plugin interface
    """
    
    # enable related variables
    can_configure = False
    """ If configuration options should be used 
        (mainly used for blocking if there is option collision) 
        
        .. See: nose plugin interface
    """
    enabled = False
    """ If this plugin is enabled (should be False by default) 
        
        .. See: nose plugin interface
    """
    
    enableOpt = None
    """ Option name for enabling this plugin, if None, default will be used 
        
        .. See: nose plugin interface
    """
    
    # plugin name
    name = "SnapshotPlugin"
    """ Name used to identify this plugin in nose
        
        .. See: nose plugin interface
    """
    
    # to be sure to run AFTER attrib plugin and before zfsStressGenerator
    score = 8
    """ Plugin ordering field within nose, used in descending order 
        
        .. See: nose plugin interface
    """    

    snapshotNoseLogOpt = "--snapshotNoseLog"
    """ Option to enable snapshoting of nose log """
    
    snapshotNoseLogEnvOpt = "SNAPSHOT_NOSE_LOG"
    """ Environment variable to enable snapshot of nose log """
    
    snapshotNoseLog = False
    """ If nose log should be appended to snapshot """
    
    maxSnapshotsOpt = "--maxSnapshots"
    """ Option string for passing maximum number of snapshots 
        (last N will be preserved) 
    """
    
    maxSnapshotsEnvOpt = "MAX_SNAPSHOTS"
    """ Environment variable from which default snapshot number should be read """
    
    maxSnapshots = 1
    """ Maximum snapshots to preserve. """
    
    snapshotsRootDirOpt = "--snapshotsRootDir"
    """ Option string for passing snapshot temp dir 
        (where snapshots should be stored) 
    """
    
    snapshotsRootDirEnvOpt = "SNAPSHOTS_ROOT_DIR"
    """ Environment variable from which default snapshot number should be read """
    
    snapshotsRootDir = "/tmp"
    """ Directory where snapshotDir for snapshots will be created """
    
    # unconfigurable variables
    snapshotStart = False
    """ If create test snapshot upon test start. """
    
    snapshotSuccess = False
    """ If create test snapshot upon test success. """
    
    snapshotFailure = True
    """ If create test snapshot upon test Failure. """
    
    snapshotError = True
    """ If create test snapshot upon test Error. """
    
    noseLogHandler = None
    """ Log handler that we use to catch output """
    
    #30M
    maxNoseLogSize = 1024 * 1024 * 30
    """ Maximum size of one nose log file. """
    
    noseLogBackupCount = 1
    """ Count of backup logs, for example 1 means, that there 
        can be two files (log and log.1) with maximum size.
        Logging module does rotating.
    """
    
    noseLogFileName = None
    """ Name of file where nose logs are stored """
    
    messageFormat = '%(created)f\t%(name)s\t%(levelname)s\t%(message)s'
    """ Format of log message """
    
    def __init__(self):
        Plugin.__init__(self)
            
    def addOptions(self, parser, env=os.environ):
        """Add command-line options for this plugin.

        The base plugin class adds --with-$name by default, used to enable the
        plugin.
        """
        self.add_options(parser, env)
        
    def add_options(self, parser, env=os.environ):
        """Non-camel-case version of func name for backwards compatibility.
        """
        try:
            self.options(parser, env)
            self.can_configure = True
        except OptionConflictError, exc:
            warn("Plugin %s has conflicting option string: %s and will "
                 "be disabled" % (self, exc), RuntimeWarning)
            self.enabled = False
            self.can_configure = False
            
    def options(self, parser, env=os.environ):
        """ Adds options for this plugin: maxSnapshots, snapshotsRootDir
            
            .. See: nose plugin interface
        """
        Plugin.options(self,  parser,  env)
        
        # add option for maxSnapshots (maximum number of snapshots preserved)
        parser.add_option(self.maxSnapshotsOpt,
            dest=self.maxSnapshotsOpt, metavar="snapshots_num", 
            action="store", type="int", 
            default=env.get(self.maxSnapshotsEnvOpt),
            help="How many last snapshots should be preserved %s (see %s) [%s]" %
            (self.__class__.__name__, self.__class__.__name__, self.maxSnapshotsEnvOpt))
        
        # add option for snapshotsRootDir (where  snapshots should be stored)
        parser.add_option(self.snapshotsRootDirOpt,
            dest=self.snapshotsRootDirOpt, metavar="snapshots_dir", 
            action="store", type="string", 
            default=env.get(self.snapshotsRootDirEnvOpt),
            help="Where snapshots should be stored %s (see %s) [%s]" %
            (self.__class__.__name__, self.__class__.__name__,
            self.snapshotsRootDirEnvOpt))
        
        # add option for enabling log snapshoting
        parser.add_option(self.snapshotNoseLogOpt,
            dest=self.snapshotNoseLogOpt, metavar="yes", 
            action="store_true",
            default=env.get(self.snapshotNoseLogEnvOpt),
            help="If append nose log to snapshot too.."
            "%s (see %s) [%s]" %
            (self.__class__.__name__, self.__class__.__name__,
            self.snapshotNoseLogEnvOpt))
                          

    
    def configure(self, options, conf):
        """ Checks options for this plugin: maxSnapshots, snapshotsRootDir
            and configure plugin according them.
            
            .. See: nose plugin interface
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        
        self.conf = conf
        
        if self.enabled == False:
            return
        
        
        if hasattr(options, self.maxSnapshotsOpt):
            self.maxSnapshots = getattr(options, self.maxSnapshotsOpt)
            
        if hasattr(options, self.snapshotsRootDirOpt):
            self.maxSnapshots = getattr(options, self.snapshotsRootDirOpt)
            
        if hasattr(options,  self.snapshotNoseLogOpt):
            self.snapshotNoseLog = getattr(options,  self.snapshotNoseLogOpt,
                self.snapshotNoseLog)
        
        if self.snapshotNoseLog:
            self.setLogOutput()
        
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def begin(self):
        """ Create snapshotRootDir if doesn't exist. """
        if not os.path.exists(self.snapshotsRootDir):
            os.makedirs(self.snapshotsRootDir)
        if not os.path.isdir(self.snapshotsRootDir):
            raise Exception("Snapshots root dir (%s) can't be created" 
                            % self.snapshotsRootDir)
                            
                            
    def finalize(self, result):
        """ Finalize plugin.
            Close log output, if snapshoting of log were enabled.
            
            ... See: nose plugin interface
        """
        self.stopLogOutput()
        
        
    @classmethod
    def hackNoseLogger(cls):
        """ Hack nose logger to allow second handler """
        noseLogger = logging.getLogger('nose')
        noseLogger.propagate = 1
        
        loggerLevel = noseLogger.getEffectiveLevel()
        for handler in noseLogger.handlers:
            if handler.level < loggerLevel or handler.level == 0:
                handler.setLevel(loggerLevel)
                
        noseLogger.setLevel(logging.NOTSET)
        
    def setLogOutput(self):
        """ Set logger to log everything for our needs.
            Currently logging is done through python logger module handler.
        """
        self.hackNoseLogger()
        
        rootLogger = logging.getLogger()
        rootLogger.setLevel(0)
        
        self.noseLogFileName = tempfile.mkstemp('.log', 'insecticide', 
            self.snapshotsRootDir)[1]
        self.noseLogHandler = RotatingFileHandler(filename = self.noseLogFileName, 
            maxBytes = self.maxNoseLogSize, 
            backupCount = self.noseLogBackupCount)
        self.noseLogHandler.setLevel(0)
        
        outputFormatter = logging.Formatter(fmt = self.messageFormat,
            datefmt = '%(asctime)s')
        self.noseLogHandler.setFormatter(outputFormatter)
        
        rootLogger.addHandler(self.noseLogHandler)
        
    def stopLogOutput(self):
        """ Stop nose logging for snapshots.
            Removes trace file.
        """
        if self.noseLogHandler:
            rootLogger = logging.getLogger()
            rootLogger.removeHandler(self.noseLogHandler)
            self.noseLogHandler.close()
            self.noseLogHandler = None
        if self.noseLogFileName:
            os.unlink(self.noseLogFileName)
            for backLogNumber in range(self.noseLogBackupCount, 
                self.noseLogBackupCount + 1):
                backLogName = self.noseLogFileName + '.' + str(backLogNumber) 
                if os.path.isfile(backLogName):
                    os.unlink(backLogName)
        
        
    def appendNoseLog(self, snapshot):
        """ Appends nose log into snapshot
            
            :Parameters:
                snapshot: snapshot to which log should be added
        """
        self.noseLogHandler.flush()
        if os.path.isfile(self.noseLogFileName):
            snapshot.addFile('nose.log', self.noseLogFileName, 
                SnapshotDescription.TYPE_LOG)
        for backLogNumber in range(self.noseLogBackupCount, self.noseLogBackupCount + 1):
            backLogName = self.noseLogFileName + '.' + str(backLogNumber) 
            if os.path.isfile(backLogName):
                snapshot.addFile('nose.log' + '.' + str(backLogNumber), 
                    backLogName, 
                    SnapshotDescription.TYPE_LOG)
        
    def snapshotIt(self, obj):
        """ Create snapshot of given object and append it to buffer
            
            :Parameters:
                obj: object to call 'snapshot' function on
                    and where snapshotBuffer is located.
                    (for example test class instance)
        """
        # test - we want to snapshot inner test case
        if hasattr(obj, "test"): 
            target = obj.test
            
        # suite - we depend on suite class / instance itself
        else:
            target = obj
            
        if hasattr(target, snapshotRedirectAttrName):
            snapshotedObject = getattr(target, snapshotRedirectAttrName)
        elif  hasattr(target, "inst") and hasattr(target.inst, "snapshot"):
            snapshotedObject = target.inst
        else:
            log.debug("can't snapshot %s. doesn't define snapshot() function",
                str(target))
            return
        log.debug('snapshotedObject is %s', snapshotedObject)
            
        toDir = tempfile.mkdtemp(prefix="noseSnapshot")
        snapshot = SnapshotDescription(toDir, log)
        

        log.debug("snapshotting %s (%s) into dir %s at %s",  str(target), snapshot,
            toDir, str(datetime.datetime.now()))
        snapshotedObject.snapshot(snapshot)
        
        if self.snapshotNoseLog:
            self.appendNoseLog(snapshot)
        
        if hasattr(target, "snapshotBuffer"):
            if target.snapshotBuffer and \
                len(target.snapshotBuffer) >= self.maxSnapshots:
                oldSnapshot = target.snapshotBuffer.pop(0)
                log.debug("removing old snapshot %s", oldSnapshot)
                oldSnapshot.delete()
        else:
            setattr(target, "snapshotBuffer", [])
        
        target.snapshotBuffer.append(snapshot)
            
    def startTest(self, test):
        """ Nose plugin hook, create test snapshot if desired """
        if self.snapshotStart:
            self.snapshotIt(test)
        
    def handleError(self, test, err):
        """ Nose plugin hook, create test snapshot if desired """
        if self.snapshotError:
            log.debug("error %s", str(err))
            self.snapshotIt(test)
        return None
        
    def handleFailure(self, test, err):
        """ Nose plugin hook, create test snapshot if desired """
        if self.snapshotFailure:
            log.debug("failure %s", str(err))
            self.snapshotIt(test)
        return None
        
    @classmethod
    def dropSnapshots(cls, test):
        """ Remove all snapshots from test's snapshotBuffer
            and delete their data fromdisk
        """
        while test.snapshotBuffer:
            test.snapshotBuffer.pop(0).delete()
            
    def addSuccess(self, test):
        """ Nose plugin hook, discards snapshots of
            non-stress tests upon success.
        """
        if not hasattr(test,"test"): #ignore non-tests
            return
        
        #do not drop snapshots of ChainedTestCases
        # before whole chain have finished
        if hasattr(test.test, "snapshotBuffer") \
            and not isinstance(test.test, ChainedTestCase) :
            self.dropSnapshots(test.test)
