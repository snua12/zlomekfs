import os
import textwrap
import logging

from optparse import OptionConflictError
from types import TypeType,  ClassType
from warnings import warn
from ConfigParser import SafeConfigParser

from nose.plugins import Plugin
from nose.util import tolist

log = logging.getLogger ("nose.plugins.zfsConfig")

class ZfsConfig(Plugin):
    """ ZFS test accessible config loader and inserter plugin.
    """
    can_configure = False
    enabled = False
    enableOpt = None
    name = "ZfsConfig"
    # to be sure to run AFTER attrib plugin
    score = 10
    
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
                          action="append", type="string", 
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
            
            #TODO: allow writes by option
            self.blockConfigWrites(self.zfsConfig)
            
            
    def blockConfigWrites(self, config):
        def warnWrite(self, *arg,  **kwarg):
            import zfsConfig
            import traceback
            trace = traceback.extract_stack(limit=5)
            trace.pop()
            zfsConfig.log.warning("try to write into readOnly config\n%s", trace)
        
        setattr(config, "set", warnWrite)
        setattr(config, "add_section", warnWrite)
        setattr(config, "remove_option", warnWrite)
        setattr(config, "remove_section", warnWrite)
    
    def startContext(self, context):
        # pass user tests config to test
        if type(context) in [TypeType,  ClassType]:
            log.debug("starting context of class %s",  context)
        if not hasattr(context,  self.configAttrName):
            setattr(context,  self.configAttrName,  self.zfsConfig)
        
    
