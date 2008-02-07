import tarfile
import uuid
import pickle
import os.path
import logging
import unittest
import struct
import shutil

from ConfigParser import SafeConfigParser
from unittest import TestCase

class SnapshotError(Exception):
    pass

#FIXME: name must not be file name
class SnapshotDescription(object):
    """
    Entries are pairs name:parameters
        name should be arbitrary string ~ filename relative to snapshot directory
        parameters should be pair (type,values) . types are enumerated here
    """
    
    # these MUST NOT change between versions
    descriptionVersionFileName = "descriptionVersion"
    usedCompression = ':gz'
    TYPE_FILE = 0
    TYPE_STRING = 1
    TYPE_INT = 2
    TYPE_BOOL = 3
    TYPE_FLOAT = 4
    
    # version dependent variables
    descriptionVersion = 1
    usedPickleProtocol = 2
    
    # generic types (fallback)
    TYPE_TAR_FILE = 201
    TYPE_GCORE = 202
    TYPE_PICKLED_OBJECT = 203
    
    # specific types
    TYPE_ZFS_GCORE = 101
    TYPE_ZFS_CACHE = 102
    TYPE_ZFS_FS = 103
    TYPE_PICKLED_TEST = 104
    TYPE_LOG = 105
    TYPE_TEST_BACKTRACE = 106
    TYPE_TEST_DATA = 107
    
    TYPE_TEST_CONFIG = 108
    defaultConfigSnapshotFileName = "testConfig"
    
    TYPE_ENV = 109
    TYPE_COMPARE_FS = 111
    TYPE_ZFS_LOG = 112
    
    tarTypes = [TYPE_TAR_FILE, TYPE_ZFS_CACHE, TYPE_ZFS_FS, TYPE_COMPARE_FS]
    
    entries = None
    entriesFileName = "entries"
    directory = None
    file = None
    
    log = logging.getLogger("nose." + __name__)
    
    def __init__(self,  directory, parentLog = None):
        self.directory = directory
        self.entries = {}
        self.addEntry('uuid', (SnapshotDescription.TYPE_STRING, str(uuid.uuid4())))
        
        if parentLog:
            self.log = parentLog
            
        self.log.debug("created snapshot with dir %s", self.directory)
        
    def __iter__(self):
        return self.entries.iteritems()
        
    def setEntry(self, name,  params):
        self.entries[name] = params
        return True
        
    def addEntry(self, name, params):
        if not self.entries.get(name, None):
            return self.setEntry(name, params)
        else:
            raise KeyError("duplicit entry")
        
    def removeEntry(self, name):
        self.entries.pop(name, None)
        
    def getEntries(self):
        for key in self.entries.keys():
            yield (key, self.entries[key])
            
    def getEntry(self, name):
        return self.entries[name]
        
    def delete(self):
        self.log.debug("removing snapshot %s with directory %s", self, self.directory)
        shutil.rmtree(self.directory, ignore_errors = True)
        self.directory = None
        return True
        
    def pack(self,  fileName):
        #store snapshot description version
        descVFileName = self.directory + os.sep + self.descriptionVersionFileName
        if os.path.exists(descVFileName):
            self.log.warning("warning, file %s in snapshot will be overriden", 
                            descVFileName)
        descVFile = open(descVFileName , "w")
        #TODO: why do not use pickle?
        descVFile.write(struct.pack('!l', self.descriptionVersion))
        descVFile.close()
        #add info about snapshotDescription file into entries
        self.addEntry(self.descriptionVersionFileName,  (SnapshotDescription.TYPE_FILE, 
                    "version of SnapshotDescription that created this snapshot"))
        
        #write entries
        entriesFullName = self.directory +  os.sep + self.entriesFileName
        if os.path.exists(entriesFullName):
            self.log.warning("warning, file %s in snapshot will be overriden", 
                            entriesFullName)
        entriesFile = open(entriesFullName , "w")
        #add info about entries file into entries
        self.addEntry(self.entriesFileName, (SnapshotDescription.TYPE_PICKLED_OBJECT,  
                    "stored entries info (pickled)"))
        pickle.dump(self.entries, entriesFile, self.usedPickleProtocol)
        entriesFile.close()
        
        # tar all registered in entries
        self.file = tarfile.open(name = fileName, mode = 'w' + self.usedCompression)
        self.file.add(self.directory, '.') # strip directory file name
        self.file.close()
        
        
    def unpack(self,  fileName,  directory = None):
        if directory:
            self.directory = directory
        if not os.path.exists(fileName):
            raise ValueError ("file not found")
        self.file = tarfile.open(name = fileName,  mode = 'r' + self.usedCompression)
        self.file.extractall(self.directory)
        
        # check snapshot version
        descVFileName = self.directory +  os.sep + self.descriptionVersionFileName
        if not os.path.exists(descVFileName):
            raise SnapshotError ("snapshot doesn't contain version information")
            
        versionFile = open(descVFileName)
        (snapshotVersion, ) = struct.unpack('!l', 
                                            versionFile.read(struct.calcsize('!l')))
        versionFile.close()
        if snapshotVersion != self.descriptionVersion:
            raise SnapshotError ("SnapshotDescription version (%s)"
                             " differ from snapshot version (%s)"
                                % (self.descriptionVersion,  snapshotVersion)
                                )
        
        # load entries
        entriesFullName = self.directory +  os.sep + self.entriesFileName
        if not os.path.exists(entriesFullName):
            raise SnapshotError ("no entries in snapshot")
        entriesFile = open(entriesFullName , "r")
        self.entries = pickle.load(entriesFile)
        entriesFile.close()
        
    def addConfig(self, config,  name = defaultConfigSnapshotFileName):
        #TODO: escape name
        fullFileName = self.directory + os.sep + name
        if os.path.exists(fullFileName):
            self.log.warning("overriding file %s by config",  name)
        file = open(fullFileName, 'w')
        config.write(file)
        file.close()
        self.addEntry(name, 
                        (self.TYPE_TEST_CONFIG, 
                        "config.write written test config"))
        
    def getConfig(self,  name = defaultConfigSnapshotFileName):
        (type, desc) = self.getEntry(name)
        if type != SnapshotDescription.TYPE_TEST_CONFIG:
            raise TypeError ("entry %s has not type TYPE_TEST_CONFIG" % type)
        config = SafeConfigParser()
        fullFileName = self.directory +  os.sep + name
        if not os.path.exists(fullFileName):
            raise KeyError("snapshot doesn't contain config")
        read = config.read(fullFileName)
        if fullFileName not in read:
            raise SnapshotError ("snapshot config could not be read")
        
        return config
    
    def addDir(self, name, sourceDirName,  type = TYPE_TAR_FILE):
        self.addEntry(name, (type, sourceDirName))
        
        tarFile = tarfile.open(name = self.directory + os.sep + name,
                               mode = 'w' + self.usedCompression)
        tarFile.add(sourceDirName, '.')
        tarFile.close()
        
        
    def getDir(self, name, targetDirName):
        (type, origin) = self.getEntry(name)
        if type not in SnapshotDescription.tarTypes:
            raise TypeError ("entry has wrong type %s", type)
        if not targetDirName:
            targetDirName = origin
        
        tarFile = tarfile.open(name = self.directory + os.sep + name, 
                               mode = 'r' + self.usedCompression)
        tarFile.extractall(targetDirName)
        tarFile.close()
        
    def addObject(self, name, object, type = TYPE_PICKLED_OBJECT, pickleMethod = None):
        if not pickleMethod:
            pickleMethod = pickle.dump
            
        dumpFile = open(self.directory + os.sep + name , "w")
        #add info about entries file into entries
        self.addEntry(name, (type, "pickled using " + str(pickleMethod)))

        pickleMethod(object, dumpFile, self.usedPickleProtocol)
        dumpFile.close()
    
    def getObject(self, name, type = TYPE_PICKLED_OBJECT, unpickleMethod = None):
        if not unpickleMethod:
            unpickleMethod = pickle.load
        
        (dumpType, memo ) = self.getEntry(name)
        if dumpType != type:
            raise TypeError("dump type (%s) doesn't match load type" % dumpType)
        dumpFile = open(self.directory + os.sep + name , "r")
        obj = unpickleMethod(dumpFile)
        dumpFile.close()
        
        return obj
        
    def addFile(self, name, sourceFileName, type = TYPE_FILE):
      self.addEntry (name,  (type,  "source was " +sourceFileName))
      shutil.copyfile(sourceFileName,  self.directory + os.sep + name)
      
    def getFile(self,  name, targetFileName, type = TYPE_FILE):
      (dumpType,  memo) = self.getEntry(name)
      if dumpType != type:
        raise TypeError("dump type (%s) doesn't match load type"  % dumpType)
        
      shutil.copyfile(self.directory + os.sep + name,  targetFileName)
      
      return targetFileName

    
class SnapshotBuffer(object):
    snapshots = []
    
    def __init__(self, temp = "/tmp",  maxSnapshots = 3):
        self.temp = temp
        self.maxSnapshots = maxSnapshots
        
    def addSnapshot(self, snapshot):
        if self.maxSnapshots <= len(self.snapshots):
            old = self.snapshots.pop(0)
            old.delete()
            
        self.snapshots.append(snapshot)
        return True
        
    def getSnapshots(self):
        return self.snapshots
    
    
    
    

class SnapshotTest(TestCase):
    
    def setUp(self):
        self.files = []
        import tempfile
        
        # create snapshot dir
        self.writeDir = tempfile.mkdtemp()
        self.files.append(self.writeDir)
        
        # export snapshot dir
        self.targetDir = tempfile.mkdtemp()
        self.files.append(self.targetDir)
        
        # read snapshot dir
        self.readDir = tempfile.mkdtemp()  
        self.files.append(self.readDir)
        
    def tearDown(self):
        import shutil
        for file in self.files:
            shutil.rmtree(file, True)
        
    @classmethod
    def configIsSubset(self, config1, config2):
        for section in config1.sections():
            if not config2.has_section(section):
                return False
            
            for item in config1.items(section):
                if not config2.has_option(section, item[0]):
                    return False
                if item[1] != config2.get(section, item[0]):
                    return False
        
        return True
    
    @classmethod
    def configEqual(self, config1, config2):
        return self.configIsSubset(config1, config2) and self.configIsSubset(config2, config1)
    
    def testEmptySnapshot(self):
        writeSnapshot = SnapshotDescription(self.writeDir)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        
        # there should be no other info
        assert readSnapshot.entries == writeSnapshot.entries
        
    def entrySnapshot(self, object, type):
        writeSnapshot = SnapshotDescription(self.writeDir)
        writeSnapshot.addEntry("_testType_", (type, object))
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        (readType, readObject) = readSnapshot.getEntry("_testType_")
        if readType != type:
            raise TypeError("got other type (%s)than inserted (%s)", 
                            readType, type)
        
        return readObject
        
    def testIntSnapshot(self):
        writeInt = 8
        readInt = self.entrySnapshot(writeInt, SnapshotDescription.TYPE_INT)
        
        assert writeInt == readInt
        
    def testStringSnapshot(self):
        writeString = "testing, testing...^&*\'\"\\#~"
        readString = self.entrySnapshot(writeString, SnapshotDescription.TYPE_STRING)
        
        assert writeString == readString
        
    def testBoolSnapshot(self):
        writeBool = False
        readBool = self.entrySnapshot(writeBool, SnapshotDescription.TYPE_BOOL)
        
        assert writeBool == readBool
        
    def objectSnapshot(self, object, type = None):
        writeSnapshot = SnapshotDescription(self.writeDir)
        if type:
            writeSnapshot.addObject("testObject", object, type)
        else:
            writeSnapshot.addObject("testObject", object)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        
        if type:
            readObject = readSnapshot.getObject("testObject", type)
        else:
            readObject = readSnapshot.getObject("testObject")
        
        return readObject
        
    def testTypedObjectSnapshot(self):
        # test object pickling on config
        writeConfig = SafeConfigParser()
        writeConfig.add_section("section")
        writeConfig.set("section", "option", "value")
        
        readConfig = self.objectSnapshot(writeConfig, SnapshotDescription.TYPE_PICKLED_TEST)
        
        assert self.configEqual(writeConfig, readConfig)
        
    def testObjectSnapshot(self):
        # test object pickling on config
        writeConfig = SafeConfigParser()
        writeConfig.add_section("section")
        writeConfig.set("section", "option", "value")
        
        readConfig = self.objectSnapshot(writeConfig)
        
        assert self.configEqual(writeConfig, readConfig)
        
    def testFileReadConfigSnapshot(self):
        configFile1 = open(self.targetDir + os.sep + "config1",  'w')
        configFile1.write('[global]\n'
                          'readOnly = True\n'
                          '\n'
                          '[zfs]\n'
                          'zfsRoot:/mnt/zfs\n')
        configFile1.close()
        configFile2 = open(self.targetDir + os.sep + "config2",  'w')
        configFile2.write('[zfs]\n'
                          'testRoot:/tmp/zfs\n'
                          'zfsCache=/var/cache/zfs\n')
        configFile2.close()
        
        writeConfig = SafeConfigParser()
        writeConfig.read([self.targetDir + os.sep + "config1", self.targetDir + os.sep + "config2"])
        
        writeSnapshot = SnapshotDescription(self.writeDir)
        writeSnapshot.addConfig(writeConfig)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        
        readConfig = readSnapshot.getConfig()
        
        assert self.configEqual(readConfig, writeConfig)
        
    def testCreatedConfigSnapshot(self):
        #create config
        writeConfig = SafeConfigParser()
        writeConfig.add_section("section")
        writeConfig.set("section", "option", "value")
        
        # create snapshot
        writeSnapshot = SnapshotDescription(directory = self.writeDir)
        writeSnapshot.addConfig(writeConfig)
        
        # export snapshot
        writeSnapshot.pack(fileName = self.targetDir +  os.sep + "snapshot")
        
        # read snapshot
        readSnapshot = SnapshotDescription(directory = self.readDir)
        readSnapshot.unpack(fileName = self.targetDir +  os.sep + "snapshot")
        
        #export and compare config
        readConfig = readSnapshot.getConfig()
        assert self.configEqual(writeConfig, readConfig)
        
    def testDirSnapshot(self):
        writeSnapshot = SnapshotDescription(self.writeDir)
        import tempfile
        fromDir = tempfile.mkdtemp()
        
        #generate some data to dir
        file2 = open(fromDir + os.sep + "config2",  'w')
        file2.write('[zfs]\n'
                          'testRoot:/tmp/zfs\n'
                          'zfsCache=/var/cache/zfs\n')
        file2.close()
        os.mkdir(fromDir + os.sep + "dir")
        
        writeSnapshot.addDir("dir", fromDir)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        toDir = tempfile.mkdtemp()
        readSnapshot.getDir('dir', toDir)
        
        assert True #TODO: compare fromDir and toDir
        
    def testFileSnapshot(self):
        writeSnapshot = SnapshotDescription(self.writeDir)
        
        originalFile = open('/bin/bash',  'r')
        originalContent = originalFile.read()
        originalFile.close()
        
        writeSnapshot.addFile(name = "bin.sh", 
                              sourceFileName = "/bin/sh",
                              type = SnapshotDescription.TYPE_ZFS_GCORE)
        
        writeSnapshot.pack(fileName = self.targetDir +  os.sep + "snapshot")
        
        # read snapshot
        readSnapshot = SnapshotDescription(directory = self.readDir)
        readSnapshot.unpack(fileName = self.targetDir +  os.sep + "snapshot")
        readSnapshot.getFile(name="bin.sh", 
                             targetFileName = self.targetDir + os.sep + "sh", 
                             type = SnapshotDescription.TYPE_ZFS_GCORE)
        
        targetFile = open(self.targetDir + os.sep + 'sh', 'r')
        targetContent = targetFile.read()
        targetFile.close()
        
        assert originalContent == targetContent
        

if __name__ == '__main__':
    unittest.main()

