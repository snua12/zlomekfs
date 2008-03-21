##
# Tests for FS operations, should be used as metatests


import logging

import os
import time
import random

from traceback import format_exc

from insecticide.graph import GraphBuilder

from zfs import ZfsStressTest, abortDeadlock, forceDeleteFile
from testFSOp import TestFSOp, tryRead, tryWrite, tryTouch, tryUnlink, tryRename
from testFSOp import  trySeek, tryGetSize, tryGetPos


from insecticide.timeoutPlugin import timed

log = logging.getLogger ("nose.tests.testStressFSOp")

class TestGlobalState(object):
    """ Global state holder for TestStressFSOp
        Holds file handles (to allow close from within
        class context.
        """
    def __init__(self):
        self.safeFile = None
        self.testFile = None
        
            
    def clean(self):
        if self.safeFile:
            forceDeleteFile(self.safeFile)
            self.safeFile = None
            
        if self.testFile:
            forceDeleteFile(self.testFile)
            self.testFile = None

class TestStressFSOp(ZfsStressTest, TestFSOp):
    disabled = False
    definitionType = GraphBuilder.USE_GLOBAL
    
    globalState = None
    """ Holder of external state that SHOULD be cleaned
        in teardownClass (file handles, etc)
    """

    
  
    noFileSuccessors = [('testTouch', 3), ('testGenerateName', 1), 
        ('testOpen', 5)]
    fileExistSuccessors = [('testRename', 1), ('testUnlink', 1),
        ('testOpen', 1), ('testTouch', 1), ('testGenerateName', 1)]
    openedFileSuccessors = [('testClose', 1), ('testRead', 1), ('testWrite', 1),
        ('testSeek', 1), ('testGetPos', 1), ('testGetSize', 1)]
  
    graph = {
                'testGenerateName' : noFileSuccessors,
                'testTouch' : fileExistSuccessors,
                'testRename' : fileExistSuccessors,
                'testUnlink' : noFileSuccessors,
                'testOpen' : openedFileSuccessors,
                'testClose' : fileExistSuccessors,
                'testRead' : openedFileSuccessors,
                'testWrite' : openedFileSuccessors,
                'testSeek': openedFileSuccessors,
                'testGetSize': openedFileSuccessors,
                'testGetPos' : openedFileSuccessors,
                }
    startingPoint = "testGenerateName"
  
    def __init__(self):
        ZfsStressTest.__init__(self)
  
    ##
    # setup before every test method
    @classmethod
    def setupClass(cls):
        cls.globalState = TestGlobalState()
        super(TestStressFSOp, cls).setupClass()
  
    ##
    # cleanup after every test method
    @classmethod
    def teardownClass(cls):
        cls.cleanFiles()
        cls.globalState = None
        super(TestStressFSOp, cls).teardownClass()
    
    def setup(self):
        ZfsStressTest.setup(self)
    
    def teardown(self):
        ZfsStressTest.teardown(self)
        
    ##
    # remove files and clean handles
    @classmethod
    def cleanFiles(cls):
        log.debug('cleaning')
        cls.globalState.clean()
        
        super(TestStressFSOp, cls).cleanFiles()
        
    @timed(2, abortDeadlock)
    def testGenerateName(self):
        name = self.generateRandomFileName()
        self.safeFileName = os.path.join(self.safeRoot, name)
        self.testFileName = os.path.join(self.zfsRoot, self.zfsVolumeDir, name)
    
    @timed(5, abortDeadlock)  
    def testTouch(self):
        
        safe = tryTouch(self.safeFileName)
        test =  tryTouch(self.testFileName)
        self.raiseExceptionIfDied()
        assert safe == test
        
    @timed(1, abortDeadlock)
    def testSkip(self):
        time.sleep(0.1)
    
    @timed(10, abortDeadlock)  
    def testUnlink(self):
        safe = tryUnlink(self.safeFileName) 
        test = tryUnlink(self.testFileName)
        
        self.raiseExceptionIfDied()
        assert safe == test
    
    @timed(10, abortDeadlock)  
    def testRename(self):
        safeResult = False
        testResult = False
        
        newName = self.generateRandomFileName()
        newSafeFileName = os.path.join(self.safeRoot, newName)
        newTestFileName = os.path.join(self.zfsRoot, self.zfsVolumeDir, newName)
        
        if tryRename(self.safeFileName,  newSafeFileName):
            self.safeFileName = newSafeFileName
            safeResult = True
        
        if tryRename(self.testFileName,  newTestFileName):
            self.testFileName = newTestFileName
            testResult = True
        
        self.raiseExceptionIfDied()
        assert safeResult == testResult
    
    @timed(5, abortDeadlock)  
    def testOpen(self):
        safeResult = False
        testResult = False
        
        try:
            self.globalState.safeFile = open(self.safeFileName, 
                self.fileAccessMode)
            safeResult = True
        except KeyboardInterrupt:
            raise
        except Exception:
            log.debug(format_exc())
        
        try:
            self.globalState.testFile = open(self.testFileName, 
                self.fileAccessMode)
            testResult = True
        except KeyboardInterrupt:
            raise
        except Exception:
            log.debug(format_exc())
            pass
        
        self.raiseExceptionIfDied()
        assert safeResult == testResult
    
    @timed(5, abortDeadlock)  
    def testClose(self):
        safeResult = False
        testResult = False
        
        try:
            if self.globalState.safeFile:
                self.globalState.safeFile.close()
            self.globalState.safeFile = None
            safeResult = True
        except KeyboardInterrupt:
            raise
        except Exception:
            log.debug(format_exc())
        
        try:
            if self.globalState.testFile:
                self.globalState.testFile.close()
            self.globalState.testFile = None
            testResult = True
        except KeyboardInterrupt:
            raise
        except Exception:
            log.debug(format_exc())
        
        self.raiseExceptionIfDied()
        assert testResult == safeResult
    
    @timed(10, abortDeadlock)  
    def testRead(self):
        position = tryGetPos(self.globalState.safeFile)
        length = tryGetSize(self.globalState.safeFile)
        
        safeResult = tryRead(self.globalState.safeFile, length - position)
        testResult = tryRead(self.globalState.testFile, length - position)
        
        self.raiseExceptionIfDied()
        assert safeResult == testResult
  
    @timed(15, abortDeadlock)  
    def testWrite(self):
        data = str(self.dataVector)
        log.debug('writing %d bytes into safe', len(data))
        safe = tryWrite(self.globalState.safeFile,  data)
        log.debug('writing %d bytes into test', len(data))
        test = tryWrite(self.globalState.testFile,  data)
        
        self.raiseExceptionIfDied()
        assert safe == test
    
    @timed(15, abortDeadlock)
    def testGetSize(self):
        safeSize = tryGetSize(self.globalState.safeFile)
        testSize = tryGetSize(self.globalState.testFile)
        self.raiseExceptionIfDied()
        assert safeSize == testSize
        
    @timed(10, abortDeadlock)
    def testGetPos(self):
        safePos = tryGetPos(self.globalState.safeFile)
        testPos = tryGetPos(self.globalState.testFile)
        
        self.raiseExceptionIfDied()
        assert safePos == testPos
  
    @timed(10, abortDeadlock)
    def testSeek(self):    
        # we assume right size of tested file - it is tested by testGetSize
        safeSize = tryGetSize(self.globalState.safeFile)
        newPos = random.randint(0, safeSize)
        
        safe = trySeek(self.globalState.safeFile, safeSize) 
        test = trySeek(self.globalState.testFile, safeSize)
        
        self.raiseExceptionIfDied()
        assert safe == test
    
    '''
    TODO: implement
    def testFlush(self):
        return
    
    def testMknod(self):
        return
    
    def testGetpos(self):
        return
    
    def testTruncate(self):
        return
    
    def testFeof(self):
        return
    
    def testGetAtime(self):
        return
    
    def testGetCtime(self):
        return
    
    def testGetMtime(self):
        return
    
    def testGetattr(self):
        return
    
    def testSetattr(self):
        return
    
    def testFlock(self):
        return
    
    def testFunlock(self):
        return
    
    def testSymlink(self):
        return
    
    def testReadlink(self):
        return
    
    def testMkdir(self):
        return
    
    def testRmdir(self):
        return
    
    def testReaddir(self):
        return
    
    def testLink(self):
        return
    
    def testChmod(self):
        return
    
    def testChown(self):
        return
    
    def testSetxattr(self):
        return
    
    def testGetxattr(self):
        return
    
    def testListxattr(self):
        return
    
    def testRemovexattr(self):
        return
    '''


class TestStressFSOpRandomly (TestStressFSOp):
    disabled = True
    definitionType = GraphBuilder.USE_FLAT

