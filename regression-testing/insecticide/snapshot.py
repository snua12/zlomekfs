""" Module with snapshot helper functions and classes.

    Main class is SnapshotDescription, it should be used
    as abstract layer for accessing raw snapshot data.
 """

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
    """ Error generated when incorect data were passed. """
    pass

class SnapshotDescription(object):
    """
        Helper object holding snapshot state.
        Primitive types are stored in memory, big data (files, etc are stored on disk)
        
        Before release of SnapshotDescription instance,
        delete method should be called. This is because
        some temporary data are stored on disk and are not
        deleted upon pack method.
    """
    
    # these MUST NOT change between versions
    descriptionVersionFileName = "descriptionVersion"
    """ 
        In snapshot tar (directory) should be file with this name
        which should contain number (description version).
        From this number format of other data should be derived.
    """
    
    usedCompression = ':gz'
    """ Compression used for tar. Directly passed to tarfile module. """
    
    
    entries = None
    """
        Entries are dictionary pairs name:parameters
        name should be arbitrary string ~ filename relative to snapshot 
            directory or just variable name
        parameters should be pair (type,values) . types are enumerated here
    """
    
    entriesFileName = "entries"
    """
        File under snapshot directory (relative) where entries should be stored into
        in descriptionVersion 1 format of this file is pickled object 
            with pickle protocol 2.
    """
    
    TYPE_FILE = 0
    """ Generic file type. Entry name is name of file,
        description is user readable string. 
    """
    
    TYPE_STRING = 1
    """ String value. Name is variable name, description is variable value (string). """
    
    TYPE_INT = 2
    """ Integer value. Name is variable name,
        description is variable value (integer). 
    """
    
    TYPE_BOOL = 3
    """ Boolean value. Name is variable name,
        description is variable value (boolean).
    """
    
    TYPE_FLOAT = 4
    """ Float value. Name is variable name, description is variable value (float). """
    
    # version dependent variables
    descriptionVersion = 1
    """ Version number of snapshot content format in 
        which data will be by default stored and load. 
    """
    
    usedPickleProtocol = 2
    """ Protocol number of pickle module (format of pickled object files). """
    
    # generic types (fallback)
    TYPE_TAR_FILE = 201
    """ Generic tar file type. Entry name is name of tar file, 
        description is user readable string. 
    """
    
    TYPE_GCORE = 202
    """ Core dump file type. Entry name is name of file, 
        description is user readable string. 
    """
    
    TYPE_PICKLED_OBJECT = 203
    """ Object pickled to file. Entry name is name of file, 
        description is user readable string. 
    """
    
    # specific types
    TYPE_ZFS_GCORE = 101
    """ Subtype of TYPE_GCORE, fields have the same meaning, 
        content is core dump of zfsd. 
    """
    
    TYPE_ZFS_CACHE = 102
    """ Subtype of TYPE_TAR_FILE, fields have the same meaning,
        content is tared zfs cache. 
    """
        
    TYPE_ZFS_FS = 103
    """ Subtype of TYPE_TAR_FILE, fields have the same meaning,
        content is tared zfs file system. 
    """
    
    TYPE_PICKLED_TEST = 104
    """ Subtype of TYPE_PICKLED_OBJECT, fields have the same meaning,
        content is tared nose test object. 
    """
    
    TYPE_LOG = 105
    """ Subtype of TYPE_FILE, fields have the same meaning, file contains logs. """
    
    TYPE_TEST_DATA = 107
    """ Subtype of TYPE_PICKLED_OBJECT, fields have the same meaning, 
        file contains pickled test data. 
    """
    
    TYPE_TEST_CONFIG = 108
    """ Subtype of TYPE_FILE, fields have the same meaning, 
        file is ConfigParser's config file. 
    """
    
    defaultConfigSnapshotFileName = "testConfig"
    """ Default name where to store config of test. """
    
    TYPE_ENV = 109
    """ Subtype of TYPE_PICKLED_OBJECT, fields have the same meaning, 
        file contains logs. 
    """
    
    TYPE_COMPARE_FS = 111
    """ Subtype of TYPE_TAR_FILE, fields have the same meaning, 
        content is tared compare file system. 
    """
    
    TYPE_ZFS_LOG = 112
    """ Subtype of TYPE_LOG, fields have the same meaning,
        file contains zfs (syplog) log. 
    """
    
    TYPE_ZEN_TEST = 113
    """ Subtype of TYPE_FILE, fields have the same meaning, 
        file contains binary with zen tests 
    """
    
    TYPE_PICKLED_OUTPUT = 114
    """ Subtype of TYPE_PICKLED_OBJECT, fields have the same meaning, 
        file contains pickled list of lines generated by binary.
    """
    
    tarTypes = [TYPE_TAR_FILE, TYPE_ZFS_CACHE, TYPE_ZFS_FS, TYPE_COMPARE_FS]
    """ enumeration of types, that are tar files """
    
    directory = None
    """ 
        Directory name (should be empty, dedicated to this snapshot only)
        where snapshot data should be stored
    """
    
    file = None
    """
        Filename of packed snapshot. This is used after pack or before unpack.
    """
    
    log = logging.getLogger("nose." + __name__)
    """
        Default logger.
    """
    
    def __init__(self,  directory, parentLog = None):
        """ Create new instance...
            
            :Parameters:
                directory: name of directory where to store big data from snapshot.
                    Must be empty and dedicated to this snapshot only
                parentLog: alternaative logger where to log. if not specified, nose.SnapshotDescription will be used
        """
        self.directory = directory
        self.entries = {}
        self.addEntry('uuid', (SnapshotDescription.TYPE_STRING, str(uuid.uuid4())))
        
        if parentLog:
            self.log = parentLog
            
        self.log.debug("created snapshot with dir %s", self.directory)
        
    def __iter__(self):
        """ Returns terator for entries. """
        return self.entries.iteritems()
        
    def setEntry(self, name,  params):
        """ Set entry, please don't use this directly.
            
            :Parameters:
                name: name of entry to set
                params: tuple (entryType, description)
            
            :Return:
                True
        """
        self.entries[name] = params
        return True
        
    def addEntry(self, name, params):
        """ Add entry of raise exception if exists.
            Use only for primitive types (string, int, bool, float).
            
            :Parameters:
                name: name of entry to set
                params: tuple (entryType, description)
            
            :Return:
                True
            
            :Raise:
                KeyError: upon duplicit entry
        """
        if not self.entries.get(name, None):
            return self.setEntry(name, params)
        else:
            raise KeyError("duplicit entry")
        
    def removeEntry(self, name):
        """ Remove entry. 
            Use only for primitive types (string, int, bool, float).
            
            :Parameters:
                name: name of entry to set
                params: tuple (entryType, description)
            
            :Raise:
                KeyError: when entry is not present
        """
        self.entries.pop(name, None)
        
    def getEntries(self):
        """ Get entry names list.
            
            :Return:
                generator for key names
        """    
        for key in self.entries.keys():
            yield (key, self.entries[key])
            
    def getEntry(self, name):
        """ Get entry parameters.
            Use only for primitive types (string, int, bool, float).
            
            :Parameters:
                name: name of entry to set
            
            :Return:
                params tuple (entryType, description)
            
            :Raise:
                KeyError: when entry is not present
        """
        return self.entries[name]
        
    def delete(self):
        """ Remove snapshot content from file system.
            
            :Return:
                True
        """
        self.log.debug("removing snapshot %s with directory %s", self, self.directory)
        shutil.rmtree(self.directory, ignore_errors = True)
        self.directory = None
        return True
        
    def pack(self,  fileName):
        """ Pack snapshot content (files, entries, info) into file (tar.gz).
            
            :Parameters:
                fileName: absolute path to file where to store
        """
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
        """ Unpack snapshot content (files, entries, info) 
            from file (tar.gz) to self.directory.
            
            :Parameters:
                fileName: absolute path to file where packed snapshot is
                directory: directory where to unpack (same as in __init__)
            
            :Raise:
                ValueError: if file is not found
                SnapshotError: if snapshot content is invalid
        """
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
        snapshotVersion = struct.unpack('!l', 
                                            versionFile.read(struct.calcsize('!l')))[0]
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
        """ Add ConfigParser configuration into snapshot.
            
            :Parameters:
                name: name of entry in snapshot
                config: ConfigParser instance
                
            :Raise:
                KeyError: if entry of that name exists
        """
        #TODO: escape name
        fullFileName = self.directory + os.sep + name
        if os.path.exists(fullFileName):
            self.log.warning("overriding file %s by config",  name)
        fileObject = open(fullFileName, 'w')
        config.write(fileObject)
        fileObject.close()
        self.addEntry(name, 
                        (self.TYPE_TEST_CONFIG, 
                        "config.write written test config"))
        
    def getConfig(self,  name = defaultConfigSnapshotFileName):
        """ Retrive ConfigParser configuration from snapshot
            
            :Parameters:
                name: name of config entry in snapshot (must have TYPE_TEST_CONFIG)
            
            :Return:
                ConfigParser instance
                
            :Raise:
                TypeError: if entry type is invalid
                KeyError: if entry is not found
                SnapshotError: if config file (in snapshot) is corrupted
        """
        entryType = self.getEntry(name)[0]
        if entryType != SnapshotDescription.TYPE_TEST_CONFIG:
            raise TypeError ("entry %s has not type TYPE_TEST_CONFIG" % entryType)
        config = SafeConfigParser()
        fullFileName = self.directory +  os.sep + name
        if not os.path.exists(fullFileName):
            raise KeyError("snapshot doesn't contain config")
        read = config.read(fullFileName)
        if fullFileName not in read:
            raise SnapshotError ("snapshot config could not be read")
        
        return config
    
    def addDir(self, name, sourceDirName,  entryType = TYPE_TAR_FILE):
        """ Add filesystem directory content into snapshot.
            
            :Parameters:
                name: name of entry in snapshot
                sourceDirName: absolute path to directory
                entryType: override type for entry type (for example TYPE_ZFS_CACHE). use only tar types
                
            :Raise:
                KeyError: if entry of that name exists
        """
        self.addEntry(name, (entryType, sourceDirName))
        
        tarFile = tarfile.open(name = self.directory + os.sep + name,
                               mode = 'w' + self.usedCompression)
        tarFile.add(sourceDirName, '.')
        tarFile.close()
        
        
    def getDir(self, name, targetDirName):
        """ Unpack filesystem directory content from snapshot.
            
            :Parameters:
                name: name of entry in snapshot
                targetDirName: absolute path to directory where to unpack content
                
            :Return:
                None
                
            :Raise:
                TypeError: if entry type is not in tar types
                KeyError: if entry is not found
        """
        (entryType, origin) = self.getEntry(name)
        if entryType not in SnapshotDescription.tarTypes:
            raise TypeError ("entry has wrong type %s", entryType)
        if not targetDirName:
            targetDirName = origin
        
        tarFile = tarfile.open(name = self.directory + os.sep + name, 
                               mode = 'r' + self.usedCompression)
        tarFile.extractall(targetDirName)
        tarFile.close()
        
    def addObject(self, name, obj, entryType = TYPE_PICKLED_OBJECT, pickleMethod = None):
        """ Add python object to snapshot
            
            :Parameters:
                name: name of entry in snapshot
                obj: object to snapshot
                entryType: override type for entry type (for example TYPE_PICKLED_TEST).
                pickleMethod: override pickle protocol
                
            :Raise:
                KeyError: if entry of that name exists
        """
        if not pickleMethod:
            pickleMethod = pickle.dump
            
        dumpFile = open(self.directory + os.sep + name , "w")
        #add info about entries file into entries
        self.addEntry(name, (entryType, "pickled using " + str(pickleMethod)))

        pickleMethod(obj, dumpFile, self.usedPickleProtocol)
        dumpFile.close()
    
    def getObject(self, name, requiredType = TYPE_PICKLED_OBJECT, unpickleMethod = None):
        """ Get python object from snapshot
            
            :Parameters:
                name: name of entry in snapshot
                requiredType: require entry of given type  (for example TYPE_PICKLED_TEST). 
                unpickleMethod: override pickle protocol for load
            
            :Return:
                python object
                
            :Raise:
                TypeError: if type is invalid
                KeyError: if entry is not found
        """
        if not unpickleMethod:
            unpickleMethod = pickle.load
        
        dumpType = self.getEntry(name)[0]
        if dumpType != requiredType:
            raise TypeError("dump type (%s) doesn't match load type" % dumpType)
        dumpFile = open(self.directory + os.sep + name , "r")
        obj = unpickleMethod(dumpFile)
        dumpFile.close()
        
        return obj
        
    def addFile(self, name, sourceFileName, entryType = TYPE_FILE):
        """ Add file content into snapshot.
            
            :Parameters:
                name: name of entry in snapshot
                sourceFileName: absolute path to file to snapshot
                entryType: override type for entry type (for example TYPE_ZFS_LOG).
                
            :Raise:
                KeyError: if entry of that name exists
        """
        self.addEntry (name,  (entryType,  "source was " +sourceFileName))
        shutil.copyfile(sourceFileName,  self.directory + os.sep + name)
      
    def getFile(self,  name, targetFileName, requiredType = TYPE_FILE):
        """ Unpack file content from snapshot.
            
            :Parameters:
                name: name of entry in snapshot
                targetFileName: absolute path to file where to unpack content
                requiredType: check if target entry type is equivalent  (for example TYPE_ZFS_LOG).
                
            :Return:
                None
                
            :Raise:
                TypeError: if entry type is not in tar types
                KeyError: if entry is not found
        """
        dumpType = self.getEntry(name)[0]
        if dumpType != requiredType:
            raise TypeError("dump type (%s) doesn't match load type"  % dumpType)
        shutil.copyfile(self.directory + os.sep + name,  targetFileName)
            
        return targetFileName
    
    

class SnapshotTest(TestCase):
    """ unit test tests for SnapshotDescription """
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
        for fileName in self.files:
            shutil.rmtree(fileName, True)
        
    @classmethod
    def configIsSubset(cls, config1, config2):
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
    def configEqual(cls, config1, config2):
        return cls.configIsSubset(config1, config2) and \
            cls.configIsSubset(config2, config1)
    
    def testEmptySnapshot(self):
        writeSnapshot = SnapshotDescription(self.writeDir)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        
        # there should be no other info
        assert readSnapshot.entries == writeSnapshot.entries
        
    def entrySnapshot(self, obj, entryType):
        writeSnapshot = SnapshotDescription(self.writeDir)
        writeSnapshot.addEntry("_testType_", (entryType, obj))
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        (readType, readObject) = readSnapshot.getEntry("_testType_")
        if readType != entryType:
            raise TypeError("got other type (%s)than inserted (%s)", 
                            readType, entryType)
        
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
        
    def objectSnapshot(self, obj, objectType = None):
        writeSnapshot = SnapshotDescription(self.writeDir)
        if objectType:
            writeSnapshot.addObject("testObject", obj, objectType)
        else:
            writeSnapshot.addObject("testObject", obj)
        writeSnapshot.pack(self.targetDir + os.sep + "snapshot")
        
        readSnapshot = SnapshotDescription(self.readDir)
        readSnapshot.unpack(self.targetDir + os.sep + "snapshot")
        
        if objectType:
            readObject = readSnapshot.getObject("testObject", objectType)
        else:
            readObject = readSnapshot.getObject("testObject")
        
        return readObject
        
    def testTypedObjectSnapshot(self):
        # test object pickling on config
        writeConfig = SafeConfigParser()
        writeConfig.add_section("section")
        writeConfig.set("section", "option", "value")
        
        readConfig = self.objectSnapshot(writeConfig,
            SnapshotDescription.TYPE_PICKLED_TEST)
        
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
        writeConfig.read([self.targetDir + os.sep + "config1",
            self.targetDir + os.sep + "config2"])
        
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
                              entryType = SnapshotDescription.TYPE_ZFS_GCORE)
        
        writeSnapshot.pack(fileName = self.targetDir +  os.sep + "snapshot")
        
        # read snapshot
        readSnapshot = SnapshotDescription(directory = self.readDir)
        readSnapshot.unpack(fileName = self.targetDir +  os.sep + "snapshot")
        readSnapshot.getFile(name="bin.sh", 
                             targetFileName = self.targetDir + os.sep + "sh", 
                             requiredType = SnapshotDescription.TYPE_ZFS_GCORE)
        
        targetFile = open(self.targetDir + os.sep + 'sh', 'r')
        targetContent = targetFile.read()
        targetFile.close()
        
        assert originalContent == targetContent
        

if __name__ == '__main__':
    unittest.main()

