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
        

class RotatingFile(object):
    """ File object wrapper that watch for file size and
        rotate it.
    """
    __overridenAttributes = ['__overridenAttributes', '__init__', 'rotate',
        'realFile', 'maxBytes', 'backupCount', 'bufsize']
    """ Attributes that are monkey patched and doesn't go directly to File object """
    
    def __getattribute__(self, name):
        """ Overriding getattribute method for object attributes
            redirects all except RotatingFile.__overridenAttributes to
            self.array (array of used items)
        """
        if name in RotatingFile.__overridenAttributes:
            return super(RotatingFile, self).__getattribute__(name)
        elif not hasattr(self, 'realFile') or self.realFile is None:
            raise AttributeError()
        else:
            self.rotate()
            return self.realFile.__getattribute__(name)
                
    __getattr__ = __getattribute__
        
    def __setattr__(self, name, value):
        """ Overriding access method for object attributes
            redirects all except RotatingFile.__overridenAttributes to
            self.array (array of used items).
        """
        if name in RotatingFile.__overridenAttributes:
            return super(RotatingFile, self).__setattr__(name, value)
        elif not hasattr(self, 'realFile') or self.realFile is None:
            raise AttributeError()
        else:
            return self.realFile.__setattr__(name, value)
    
    def __hasattr__(self, name):
        """ Overriding getattribute method for object attributes
            redirects all except RotatingFile.__overridenAttributes to
            self.array (array of used items).
            
            It creates at most backupCount backups named filename.[backupNumber]
            for example there would be file, file.1, file.2, file.3 
        """
        if name in RotatingFile.__overridenAttributes:
            return super(RotatingFile, self).__hasattr__(name)
        elif not hasattr(self, 'realFile') or self.realFile is None:
            raise AttributeError()
        else:
            return self.realFile.__hasattr__(name)
                
    __getattr__ = __getattribute__
    
    def __init__(self, filename, mode = 'a', bufsize = -1, maxBytes = 0,
        backupCount = 0):
        """ Constructor of rotating file. 
            
            :Parameters:
                filename: goes directly to buildin open() function
                mode: goes directly to buildin open() function
                bufsize: goes directly to buildin open() function
                maxBytes: maximum size of file in bytes.
                backupCount: how many backups should be there.
        """
        self.maxBytes = maxBytes
        self.backupCount = backupCount
        self.bufsize = bufsize
        self.realFile = open(filename, mode, bufsize)
    
    def rotate(self, size = None):
        if not self.realFile or self.realFile.closed:
            return
        if not size:
            size = self.realFile.tell()
        if size < self.maxBytes:
            return
            
        oldName = self.realFile.name
        
        if os.path.isfile(oldName + '.' + str(self.backupCount)):
            os.unlink(oldName + '.' + str(self.backupCount))
        backupNumber = self.backupCount - 1
        while backupNumber > 0:
            if os.path.isfile(oldName + '.' + str(backupNumber)):
                os.rename(oldName + '.' + str(backupNumber),
                    oldName + '.' + str(backupNumber + 1))
            backupNumber -= 1
        
        if not self.realFile.closed:
            self.realFile.close()
            
        if self.backupCount > 0:
            os.rename(oldName, oldName + '.' + '1')
        else:
            os.unlink(oldName)
        
        newFile = open(oldName, self.realFile.mode, self.bufsize)
        self.realFile = newFile
        
