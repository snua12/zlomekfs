import logging
from ConfigParser import ConfigParser

from insecticide.graph import GraphBuilder

log = logging.getLogger("nose.pruneTests")


class PruneTest(object):
  definitionType = GraphBuilder.USE_LOCAL
  startingPoint = "metaStart"
  metaTest = True
  shouldFail = False
  
  
  def metaStart(self):
    log.debug("metaStart")
  metaStart.successors = [
                                     ('meta1', 100), 
                                     ('meta2', 1)
                         ]
  
  def meta1(self):
    log.debug("test1")
    self.shouldFail = True
  meta1.successors = [ 
                     ('meta2', 100)
                     ]
  
  def meta2(self):
    log.debug( "test2")
  meta2.successors = [
                     ('failMeta', 100)
                     ]
    
  def failMeta(self):
    if self.shouldFail:
      log.debug("failing at your command, sir")
      assert 0
  
  failMeta.successors = [
                     ('meta5', 100)
                     ]
  
  def meta5(self):
    pass
