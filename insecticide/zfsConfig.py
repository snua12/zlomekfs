""" This module provides zfsConfig plugin, which 
    loads ConfigParser files and serves to tests.
"""

import os
import textwrap
import logging

from optparse import OptionConflictError
from types import TypeType,  ClassType
from warnings import warn
from ConfigParser import SafeConfigParser, ParsingError, MissingSectionHeaderError

from nose.plugins import Plugin
from nose.util import tolist

log = logging.getLogger ("nose.plugins.zfsConfig")
""" Logger used in this module"""

class ZfsConfig(Plugin):
    """ ZFS test accessible config loader and inserter plugin.
        If enabled, configs loaded will be provided to test instances
        as zfsConfig attirbute.
        :Configuration Options:
            zfsConfig: list of filenames which will be considered
                as config files (and loaded, if possible)
        
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
    name = "ZfsConfig"
    """ Name used to identify this plugin in nose
        
        .. See: nose plugin interface
    """
    
    # to be sure to run AFTER attrib plugin
    score = 10
    """ Plugin ordering field within nose, used in descending order 
        
        .. See: nose plugin interface
    """
    
    configAttrName = "zfsConfig"
    """ Attribute of context (class, module) of test through 
        which reference to ConfigParser should be passed.
        
        For example if test is loaded from class TestTest.testFunction(self),
        then self.zfsConfig will be the ConfigParser object
    """
    
    
    configFileOpt = "--zfsConfig"
    """ option string for passing config file names to plugin """
    
    configFileEnvOpt = "ZFS_CONFIG_FILE"
    """ Environment variable from which default config file name
        should be read.
    """
    
    configFileNames = None
    """ File names of configs passed to plugins """
    
    zfsConfig = None
    """ ConfigParser object created from configFiles (configFileNames). """

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
        """ Adds options for this plugin: zfsConfig
            
            .. See: nose plugin interface
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
        """ Checks options for this plugin: zfsConfig.
            and configure plugin according them.
            
            .. See: nose plugin interface
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
        
        .. See: nose plugin interface
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def begin(self):
        """ Before anything starts, load config.
            Load tests config from configFileNames.
            
        .. See: nose plugin interface
        """
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
        """ Alter ConfigParser instance methods to log warning
            upon try to write into config (and block the write).
            
            :Parameters:
                config: ConfigParser instance
                
        """
        def warnWrite(self, *arg,  **kwarg):
            """ Function to stub write methods on ConfigParser with """
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
        """ Upon start of context of test, put config to it's context.
        
            .. See: nose plugin interface
        """
        # pass user tests config to test
        if type(context) in [TypeType,  ClassType]:
            log.debug("starting context of class %s",  context)
        if not hasattr(context,  self.configAttrName):
            setattr(context,  self.configAttrName,  self.zfsConfig)
        
    
