
import logging
from ConfigParser import ConfigParser
from graph import GraphBuilder

log = logging.getLogger("nose.sortTest")

def teardown():
    log.debug("teardown module")

def setup():
    log.debug("setup module")

class MetaSort(object):
  definitionType = GraphBuilder.USE_LOCAL
  startingPoint = "metaStart"
  metaTest = True

  def teardown(self):
    log.debug("teardown method")

  def setup(self):
    log.debug("setup method")

  @classmethod
  def setupClass(self):  
    log.debug("setup class")
  
  @classmethod
  def teardownClass(self):
    log.debug("teardown class")
  
  def metaStart(self):
    log.debug("metaStart")
  metaStart.successors = [
                                     ('meta1', 100), 
                                     ('meta2', 10), 
                                     ('meta3', 10), 
                                     ('meta4', 10), 
                                     ('meta5', 10)
                                        ]
  
  def meta1(self):
    log.debug("test1")
  meta1.successors = [
                     ('meta1', 5), 
                     ('meta2', 0), 
                     ('meta3',  20), 
                     #not necessary
                     #('meta4',0), 
                     ('meta5',  30) , 
                     ('failMeta', 100)
                     ]
  
  def meta2(self):
    log.debug( "test2")
    
  def meta5(self):
    log.debug( "test5")
    
  def meta4(self):
    log.debug("test4")
  
  def meta3(self):
    log.debug("test3")
  
  def failMeta(self):
    log.debug("failing at your command, sir")
    assert 0
  failMeta.successors = [
                     ('meta5', 100)
                     ]
  
  def testDisabled(self):
    log.debug("disabled test shouldn't run")
    assert 0
  testDisabled.disabled = True
  testDisabled.metaTest = False
  
  def testWithLevel(self):
    log.debug("test on leve 6 run")
  testWithLevel.level = 6
  
  def testZfsConfig(self):
    if hasattr(self, "zfsConfig"):
        log.debug("has zfsConfig (%s)",  self.zfsConfig)
        try:
            readonly = self.zfsConfig.get("global",  "readOnly")
            log.debug("zfs::readOnly = %s", readonly)
        except Exception,  info:
            log.debug("zfs::readOnly not found (%s)",  info)
    else:
        log.debug("zfsConfig not found")
        
  testZfsConfig.metaTest = False
  
  def testWriteZfsConfig(self):
    if hasattr(self, "zfsConfig"):
        try:
            readonly = self.zfsConfig.set("global",  "readOnly", "reseted")
            log.debug("zfs::readOnly = %s", self.zfsConfig.get("global", "readOnly"))
        except Exception,  info:
            log.debug("zfs::readOnly not found (%s)",  info)
    else:
        log.debug("zfsConfig not found")
        
  testWriteZfsConfig.metaTest = False
  testWriteZfsConfig.disabled = True
  
  graph = {'meta1': [('meta2', 15), ('meta3', 10), ('meta5', 50)],
                'meta2': [('meta5', 100)],
                'meta4': [('meta1', 50), ('meta5', 10)],
                'meta5': [('meta5', 10)],
#                'testDisabled': [], 
                'testWithLevel': []
             }
