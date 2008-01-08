import os
import textwrap
import logging
from optparse import OptionConflictError
from types import TypeType,  ClassType
from warnings import warn
from ConfigParser import SafeConfigParser

from nose.plugins import Plugin
from nose.util import tolist
from failure import ZfsTestFailure
from report import ReportProxy

class ZfsReportPlugin(Plugin):
    """ Report test results to repository
    """
    can_configure = False
    enabled = False
    enableOpt = None
    name = "ZfsReportPlugin"
    # to be sure to run LAST
    score = 2
    
    def __init__(self):
        Plugin.__init__(self)
        self.reporter = ReportProxy()
        
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
        
    
    def configure(self, options, conf):
        """Configure the plugin and system, based on selected options.
        
        The base plugin class sets the plugin to enabled if the enable option
        for the plugin (self.enableOpt) is true.
        """
        Plugin.configure(self,  options,  conf)
        if not self.can_configure:
            return
        
        if self.enabled == False:
            return
        
    
    def help(self):
        """Return help for this plugin. This will be output as the help
        section of the --with-$name option that enables the plugin.
        """
        if self.__class__.__doc__:
            # doc sections are often indented; compress the spaces
            return textwrap.dedent(self.__class__.__doc__)
        return "(no help available)"
    
    def addFailure(self, test, err):
	if hasattr(test, "test"): #ignore context suites
            self.reporter.reportFailure(ZfsTestFailure(test, err))
    
    addError = addFailure
    
    def addSuccess(self, test):
        self.reporter.reportSuccess(test)
    
