from graph import GraphBuilder

def printStatus(self):
    print "self is %s" % repr(self)
    print "\t instantiated ",   self.instantiated
    print "\t setup ",  self.setUp
    print "\t setupMethod ",  self.setUpMethod
    print "\t teardown ",  self.tearDown
    print "\t teardownMethod ",  self.tearDownMethod
    print "\t execs ",  self.execs
    print "\t test1 ", self.tst1, " test2 ",  self.tst2


class TestInstances():
  definitionType = GraphBuilder.USE_FLAT
  metaTest = True
  instantiated = 0
  setUp = 0
  setUpMethod = 0
  tearDown = 0
  tearDownMethod = 0
  execs = 0
  tst1 = 0
  tst2 = 0
    
  def __init__(self):
    print "in method init:"
    printStatus(self)
    self.instantiated += 1
  
  @classmethod
  def setupClass(self):
    print "in method setupClass:"
    printStatus(self)    
    self.setUp += 1
  
  @classmethod
  def teardownClass(self):
    print "in method teardownClass:"
    printStatus(self)    
    self.tearDown += 1
  
  def setup(self):
    print "in method setup:"
    printStatus(self)    
    self.setUpMethod += 1
  
  def teardown(self):
    print "in method teardown:"
    printStatus(self)    
    self.tearDownMethod += 1
  
  def test1(self):
    print "in method test1:"
    printStatus(self)
    self.tst1 += 1
    self.execs += 1
  
  def test2(self):
    print "in method test2:"
    printStatus(self)
    self.tst2 += 1
    self.execs += 1
