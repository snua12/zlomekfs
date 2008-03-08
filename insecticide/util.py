import os
import sys
from resource import RLIMIT_CORE, RLIMIT_FSIZE, setrlimit, getrlimit


def noseWrapper(project = None, stripPath = None):
    if 'DJANGO_SETTINGS_MODULE' not in os.environ:
        os.environ['DJANGO_SETTINGS_MODULE'] = 'TestResultStorage.settings'

    from insecticide.report import generateLocalBatch, finalizeBatch, branchEnvOpt, reportSystemError


    try:
        import pysvn
        entry = pysvn.Client().info('.')
        if stripPath:
            branch = str(entry.url)[len(entry.repos) + 1:len(entry.url) - len(stripPath) - 1]
        else:
            branch = str(entry.url)[len(entry.repos) + 1:]
        os.environ[branchEnvOpt] = branch
    except:
        info = sys.exc_info()
        print info
    else:
        info = None
    
    # we can't do anything about batch creation exceptions. When batch can't be created, we have no way how to report potential failure.
    batch = generateLocalBatch(project = project)

    if batch and info:
        reportSystemError(batch, name = info[0].__name__, 
            description = "Pysvn error in test.py", errInfo = info)

    from nose import main

    try:
        res = main(exit=False)
    except:
        res = None
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
    ret = []
    for name in dir(obj):
        item = getattr(obj,  name,  None)
        if type(item)  in types:
            ret.append(name)
        
        return ret

class CoreDumpSettings(object):
    rLimitCore = None
    rLimitFsize = None
    
def allowCoreDumps():
    settings = CoreDumpSettings()
    settings.oldRlimitCore = getrlimit(RLIMIT_CORE)
    settings.oldRlimitFsize = getrlimit(RLIMIT_FSIZE)
    #infinite limits
    setrlimit(RLIMIT_CORE, (-1, -1))
    setrlimit(RLIMIT_FSIZE, (-1, -1))
    
    return settings
    
def setCoreDumpSettings(settings):
    if settings.rLimitCore is not None:
        setrlimit(RLIMIT_CORE, settings.oldRlimitCore)
    if settings.oldRlimitFsize is not None:
        setrlimit(RLIMIT_FSIZE, settings.oldRlimitFsize)
        
    
