import os
import textwrap
import logging
import tempfile

from insecticide.snapshot import SnapshotDescription
from insecticide.zfsStressGenerator import ChainedTestCase
from optparse import OptionConflictError
from warnings import warn

from nose.plugins import Plugin

log = logging.getLogger ("nose.plugins.snapshotPlugin")

class SnapshotPlugin(Plugin):
    """ Automatic test snapshots creation
    """
    can_configure = False
    enabled = False
    enableOpt = None
    name = "SnapshotPlugin"
    # to be sure to run AFTER attrib plugin and before zfsStressGenerator
    score = 8
    
    # option string for passing maximum number of snapshots (last N will be preserved)
    maxSnapshotsOpt = "--maxSnapshots"
    # environment variable from which default snapshot number
    # should be read
    maxSnapshotsEnvOpt = "MAX_SNAPSHOTS"
    # file name of config passed to plugins
    maxSnapshots = 1
    
    # option string for passing snapshot temp dir (where snapshots should be stored)
    snapshotsRootDirOpt = "--snapshotsRootDir"
    # environment variable from which default snapshot number
    # should be read
    snapshotsRootDirEnvOpt = "SNAPSHOTS_ROOT_DIR"
    # file name of config passed to plugins
    snapshotsRootDir = "/tmp"

    snapshotStart = False
    snapshotSuccess = False
    snapshotFailure = True
    snapshotError = True
    
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
        # FIXME raise deprecation warning if wasn't called by wrapper 
        try:
            self.options(parser, env)
            self.can_configure = True
        except OptionConflictError, e:
            warn("Plugin %s has conflicting option string: %s and will "
                 "be disabled" % (self, e), RuntimeWarning)
            self.enabled = False
            self.can_configure = False
            
    def options(self, parser, env=os.environ):
        """New plugin API: override to just set options. Implement
        this method instead of addOptions or add_options for normal
        options behavior with protection from OptionConflictErrors.
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
                          (self.__class__.__name__, self.__class__.__name__, self.snapshotsRootDirEnvOpt))

    
    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.
        
        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
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
        
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def begin(self):
        if not os.path.exists(self.snapshotsRootDir):
            os.makedirs(self.snapshotsRootDir)
        if not os.path.isdir(self.snapshotsRootDir):
            raise Exception("Snapshots root dir (%s) can't be created" 
                            % self.snapshotsRootDir)
    
    def snapshotTest(self, test):
        if not hasattr(test, "test"): #not normal test, possibly suite
            return
        toDir = tempfile.mkdtemp(prefix="noseSnapshot")
        snapshot = SnapshotDescription(toDir, log)
        if hasattr(test.test, "inst") and hasattr(test.test.inst, "snapshot"):
            import datetime
            log.debug("snapshotting %s (%s) into dir %s at %s",  str(test), snapshot, toDir, str(datetime.datetime.now()))
            test.test.inst.snapshot(snapshot)
        else:
            log.debug("can't snapshot %s. doesn't define snapshot() function",  str(test))
            #snapshot.addObject(name = str(test.test), object = test,  type = SnapshotDescription.TYPE_PICKLED_TEST)
        
        if hasattr(test.test, "snapshotBuffer"):
            if test.test.snapshotBuffer and len(test.test.snapshotBuffer) >= self.maxSnapshots:
                oldSnapshot = test.test.snapshotBuffer.pop(0)
                log.debug("removing old snapshot %s", oldSnapshot)
                oldSnapshot.delete()
        else:
            setattr(test.test, "snapshotBuffer", [])
        
        test.test.snapshotBuffer.append(snapshot)
        
    def startTest(self, test):
        if self.snapshotStart:
            self.snapshotTest(test)
        
    def handleError(self, test, err):
        if self.snapshotError:
            log.debug("error %s", str(err))
            self.snapshotTest(test)
        
    def handleFailure(self, test, err):
        if self.snapshotFailure:
            log.debug("failure %s", str(err))
            self.snapshotTest(test)
        
    @classmethod
    def dropSnapshots(self,test):
        while test.test.snapshotBuffer:
            test.test.snapshotBuffer.pop(0).delete()
            
    def addSuccess(self, test):
        if not hasattr(test,"test"): #ignore non-tests
            return
        if hasattr(test.test, "snapshotBuffer") and test.test.__class__ is not ChainedTestCase :
            self.dropSnapshots(test)
