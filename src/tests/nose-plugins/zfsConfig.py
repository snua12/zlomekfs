import os
import re
import textwrap
import logging

from optparse import OptionConflictError
from types import MethodType,  TypeType,  ClassType
from warnings import warn
from ConfigParser import SafeConfigParser

from nose.plugins import Plugin
from nose.util import tolist
from generator import StressGenerator

log = logging.getLogger ("nose.plugins.zfsPlugin")

fixtureMethods = [
                               'setup', 'setUp',  'setup_method', 
                               'teardown', 'tearDown', 'teardonw_method',
                               'setup_class',  'setup_all', 'setupClass', 
                               'setupAll', 'setUpClass',  'setUpAll', 
                               'teardown_class', 'teardown_all', 'teardownClass',
                               'teardownAll', 'tearDownClass', 'tearDownAll' 
                             ]

class MetaTestCollector(object):
    map = {}
    metaClasses = []
    
    def isMetaClass (self,  cls):
        return cls in self.metaClasses
    
    def registerMetaClass(self,  cls):
        self.metaClasses.append(cls)
    
    def add(self,  method):
        classRow = self.map.get(method.im_class,  [])
        classRow.append(method)
        self.map[method.im_class] = classRow
    
    def getClassMethods(self,  cls):
        return self.map.get(cls,  None)

class ZfsPlugin(Plugin):
    """ ZFS test plugin.
    By now loads file with options for tests and passes object with them to test\'s \'self\'
    
    """
    can_configure = False
    enabled = False
    enableOpt = None
    name = "ZfsPlugin"
    # to be sure to run AFTER attrib plugin
    score = 0
    
    # option string for passing config file name to plugin
    configFileOpt = "--zfsConfig"
    # attribute of context (class, module) of test through 
    # which reference to ConfigParser should be passed
    configAttrName = "zfsConfig"
    # environment variable from which default config file name
    # should be read
    configFileEnvOpt = "ZFS_CONFIG_FILE"
    # file name of config passed to plugins
    configFileNames = None
    # ConfigParser object created from configFile (configFileName)
    zfsConfig = None
    
    # name of attribute which says if test is meta
    metaAttrName = "metaTest"
    metaTestCollector = MetaTestCollector()
    generator = StressGenerator(log = log)

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
        
        # add option for configFileName (name of file containing tests config)
        parser.add_option(self.configFileOpt,
                          dest=self.configFileOpt, metavar="file_name", 
                          action="append",
                          default=env.get(self.configFileEnvOpt),
                          help="File containing configuration passed to tests %s (see %s) [%s]" %
                          (self.__class__.__name__, self.__class__.__name__, self.configFileEnvOpt))
        
    
    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.
        
        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        self.conf = conf
        if hasattr(options, self.enableOpt):
            self.enabled = getattr(options, self.enableOpt)
        
        if self.enabled == False:
            return
        
        # try to get name of file containing tests config
        if hasattr(options,  self.configFileOpt):
            self.configFileNames = tolist(getattr(options,  self.configFileOpt,  None))
        
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    def setOutputStream(self,  stream):
        return None
    
    
    '''   
    # Compatiblity shim
    def tolist(self, val):
        warn("Plugin.tolist is deprecated. Use nose.util.tolist instead",
             DeprecationWarning)
        return tolist(val)
    
    def afterTest(self,  test):
        #TODO: snapshot
        pass
    def beforeTest(self,  test):
        #TODO: snapshot
        pass
    
    def begin(self):
        #TODO: start zfs ;)
        pass
    def finalize(self,  result):
        #final cleanup
        pass
    #handleError handleFailure 
    
    def loadTestsFromModule(self,  module):
      if getattr(module, "zfsModule", None) is None:
        return
      #load classes
      classSuites = [self.loadTestsFromClass(self, cls)
                     for cls in filter(self.wantClass,  (dir(cls)))]
      if classSuites is None:#empty
        return
      #create context
      moduleSetup = None
      moduleTeardown = None
      #wrap classes
      return testSuite()
    
    def loadTestsFromClass(self,  cls):
      if getattr(cls,  "zfsClass",  None) is None:
        return
      #make tests
      tests = [self.makeTest(getattr(cls, method))
        for method in filter(self.wantMethod, dir(cls))]
      if tests is None:
        return
      #sort tests
      tests.sort()
      #make suite
      return    ContextSuite(tests, cls)
    '''
    
#    def makeTest(self,  method):
#      return MethodTestCase(method)
    def wantClass(self,  cls):
        isMeta = getattr(cls,   self.metaAttrName,  False)
        if isMeta:
            self.metaTestCollector.registerMetaClass(cls)
        return
    
    def wantMethod(self,  method):
        if hasattr(method,  self.metaAttrName):
            isMeta = getattr(method,  self.metaAttrName)
        else: #collect all methods from "metaClasses" except fixtures
            isMeta = self.metaTestCollector.isMetaClass(method.im_class) and \
                        method.__name__ not in fixtureMethods
        # prevent meta tests to be collected
        if isMeta:
            self.metaTestCollector.add(method)
            log.debug("collecting meta test %s",  method)
            return False
        return
        
    
    def loadTestsFromTestClass(self,  cls):
        log.debug("generating stress tests from class %s methods", cls)
        allowedMethods = self.metaTestCollector.getClassMethods(cls)
        log.debug("allowedMethods %s?",allowedMethods)
        
        if allowedMethods:
            return self.generator.generateFromClass(cls, allowedMethods)
        else:
            log.debug("no meta tests in class %s",  cls)
        
    
    def begin(self):
        # load tests config from configFileName
        if self.configFileNames:
            self.zfsConfig = SafeConfigParser()
            try:
                read = self.zfsConfig.read(self.configFileNames)
                if read != self.configFileNames:
                    log.error("files %s could not be read",  self.configFileNames)
                    log.error("read files %s",  read)
            except (ParsingError,  MissingSectionHeaderError),  inst:
                log.error("Error occured when parsing zfs file %s", inst)
    
    def startContext(self, context):
        # pass user tests config to test
        if type(context) in [TypeType,  ClassType]:
            log.debug("starting context of class %s",  context)
        if not hasattr(context,  self.configAttrName):
            setattr(context,  self.configAttrName,  self.zfsConfig)
    
