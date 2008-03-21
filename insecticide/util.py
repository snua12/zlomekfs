""" General helper functions used in insecticide """

import os
import sys
from resource import RLIMIT_CORE, RLIMIT_FSIZE, setrlimit, getrlimit


def noseWrapper(project = None, stripPath = None):
    """ Wrapper function which executes nose within TestResultStorage BatchRun.
        
        :Parameters:
            project: project name for which tests are executed
            stripPath: last part of repository path, 
                which is local to branch and should be striped from branch name
                
        :Return:
            exits with return code from nose
    """
    if 'DJANGO_SETTINGS_MODULE' not in os.environ:
        os.environ['DJANGO_SETTINGS_MODULE'] = 'TestResultStorage.settings'

    from insecticide.report import generateLocalBatch, finalizeBatch, \
        branchEnvOpt, reportSystemError


    try:
        import pysvn
        entry = pysvn.Client().info('.')
        if stripPath:
            branch = str(entry.url)[len(entry.repos) + 1:
                len(entry.url) - len(stripPath) - 1]
        else:
            branch = str(entry.url)[len(entry.repos) + 1:]
        os.environ[branchEnvOpt] = branch
    except KeyboardInterrupt:
        raise
    except:
        info = sys.exc_info()
        print info
    else:
        info = None
    
    # we can't do anything about batch creation exceptions. 
    # When batch can't be created, we have no way how to report potential failure.
    batch = generateLocalBatch(project = project)

    if batch and info:
        reportSystemError(batch, name = info[0].__name__, 
            description = "Pysvn error in test.py", errInfo = info)

    from nose import main
    res = None
    try:
        res = main(exit=False)
    except KeyboardInterrupt:
        print sys.exc_info()
    except:
        if batch:
            info = sys.exc_info()
            reportSystemError(batch, name = info[0].__name__,
                description = "Unhandled exception in main execution loop",
                errInfo = info)

    if batch and batch.id:
        finalizeBatch(batch.id)

    if res:
        sys.exit(not res.success)
    else:
        sys.exit(1)
    

def getMatchedTypes(obj,  types):
    """ Returns attributes of object that are of one of defined types.
        
        :Parameters:
            obj: object from which attributes should be fetched
            types: list of types that should be returned
            
        :Return:
            list of attributes of given object that is of defined type
            (for example for types = [MethodType] 
            returns all methods of given object)
    """
    ret = []
    for name in dir(obj):
        item = getattr(obj,  name,  None)
        if type(item)  in types:
            ret.append(name)
        
        return ret

class CoreDumpSettings(object):
    """ Wrapper object for rlimit settngs
        (defines system core dumping settings)
    """
    rLimitCore = None
    """ Limit of core dump size for one process. """
    
    rLimitFsize = None
    """ Limit of file size for current process. """
    
def allowCoreDumps():
    """ Allow core dump generating by defining infinite core dump size.
        
        :Return:
            previous settings used by operating system
    """
    settings = CoreDumpSettings()
    settings.oldRlimitCore = getrlimit(RLIMIT_CORE)
    settings.oldRlimitFsize = getrlimit(RLIMIT_FSIZE)
    #infinite limits
    setrlimit(RLIMIT_CORE, (-1, -1))
    setrlimit(RLIMIT_FSIZE, (-1, -1))
    
    return settings
    
def setCoreDumpSettings(settings):
    """ Set core dump settings to given values
        
        :Parameters:
            settings: CoreDumpSettings instance defining 
                wanted behavior
    """
    
    if settings.rLimitCore is not None:
        setrlimit(RLIMIT_CORE, settings.oldRlimitCore)
    if settings.oldRlimitFsize is not None:
        setrlimit(RLIMIT_FSIZE, settings.oldRlimitFsize)
        
    
