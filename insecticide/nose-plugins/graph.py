import unittest

from random import SystemRandom
from util import getMatchedTypes
from types import MethodType
from unittest import TestCase

class DependencyDeffinitionError(Exception):
    """Error raised when dependency deffinition is illegal
        or missing
    """
    pass

class DependencyGraph(object):
    
    graph = None
    currentNode = None
    randomGenerator = SystemRandom()
    
    def equals(self,  graph):
        return self.graph == graph.graph and self.currentNode == graph.currentNode
    
    def __init__(self,  graph = None, startNode = None):
        if graph:
            self.graph = graph
        else:
            self.graph = {}
        self.currentNode = startNode
        if self.currentNode is None:
            try:
                self.initRandomStartNode()
            except IndexError:
                pass
        self.startNode = self.currentNode
        
    
    def initRandomStartNode(self):
        self.restart( self.randomGenerator.choice(self.graph.keys()) )
    
    def setEdge(self,  start,  end,  prob):
        try:
            edges = self.graph[start]
            edges.append((end, prob))
            
        except KeyError:
            graph[start] = [(end, prob)]
        
    
    """
        Set node and it's ancestors
        node should be string
        edges should be list of pairs (nodeName, score)
        where nodeName is string and score is integer
    """
    def setNode(self,  node,  edges):
        self.graph[node] = edges
    
    def setCurrentNode(self,  node):
        self.currentNode = node
        
    def setStartNode(self, node):
        self.startNode = node
    
    def restart(self,  startNode = None):
        if startNode:
            self.startNode = startNode
        self.currentNode = self.startNode
    
    @classmethod
    def getNodeListSum(self, list):
        sum = 0
        for item in list:
            sum += item[1]
        return sum

    @classmethod
    def getNodeByRangeHit(self, list,  hit):
        for item in list:
            hit -= item[1]
            if hit <=0:
                return item[0]
        return None
    
    def getCurrent(self):
        return self.currentNode
    
    """
        Returns current and shift to next
    """
    def next(self,  stopProbability = 0):
        current = self.currentNode
        if stopProbability > 0:
            key = self.randomGenerator.uniform(0, 1)
            if key < stopProbability:
                self.currentNode = None
                return current
        
        try:
            edges = self.graph[self.currentNode]
            sum = self.getNodeListSum(edges)
            if sum > 1:
                key = self.randomGenerator.randint(1, sum )
            else:
                key = sum
            self.currentNode = self.getNodeByRangeHit(edges,  key)
        except KeyError:
            self.currentNode = None
        return current
        
    def getShortestPath (self,  start,  end,  ignoredNodes = None):
        if ignoredNodes:
            visited = ignoredNodes
        else:
            visited = []
        queue = [(start, [])]
        
        while queue:
            (node, ancestors) = queue.pop (0)
            if node == end:
                ancestors.append(end)
                return ancestors
                
            if node in visited:
                continue
                
            visited.append(node)
            
            newAncestors = list(ancestors)
            newAncestors.append(node)
            try:
                successors = self.graph[node]
                for (next, prob) in successors:
                    if prob > 0:
                        queue.append((next, newAncestors))
            except KeyError:
                continue
            
        return None
    

class testLinearGraph(TestCase):
    graph = None
    def setUp(self):
        self.graph = DependencyGraph(
                                     {
                                     'startNode': [('unreachableNode', 0), ('secondNode', 100) ], 
                                     'unreachableNode':[], 
                                     'secondNode': [('lastNode', 100)], 
                                     'lastNode': []
                                     }, 
                                     'startNode')
        
    
    def testPath(self):
        assert self.graph.next() == 'startNode'
        assert self.graph.next() == 'secondNode'
        assert self.graph.next() == 'lastNode'
        assert self.graph.next() == None
        

class testShortestPath(TestCase):
    graph = None
    def setUp(self):
        self.graph = DependencyGraph(
                                     {
                                     '1': [('2', 1), ('3', 1)], 
                                     '2': [('4', 1), ('5', 1)], 
                                     '3': [('4', 1), ('6', 1)], 
                                     '4': [('5', 1)], 
                                     '5': [('6', 1)], 
                                     '6': []
                                     }, 
                                     '1')
    
    def testTrivialPath(self):
        assert self.graph.getShortestPath('4', '5') == ['4', '5']
    
    def testFullPath(self):
        assert self.graph.getShortestPath('1', '6') == ['1', '3', '6']
    
    def testNonExisting(self):
        assert self.graph.getShortestPath('2', '3') == None
    
    def testPartialPath(self):
        assert self.graph.getShortestPath('2', '6') == ['2', '5', '6']
    
    def testPathWithBlockedNodes(self):
        assert self.graph.getShortestPath('1', '6', ['3']) == ['1', '2', '5', '6']

class testInfiniteGraph(TestCase):
    graph = None
    def setUp(self):
        self.graph = DependencyGraph(
                                     {
                                        'firstNode':[('secondNode', 2)], 
                                        'secondNode':[('firstNode', 1)]
                                     }
                                )
        self.graph.restart('firstNode')
        
    
    def testInfinity(self):
        for i in range(0, 100):
            next = self.graph.next()
            if i % 2:
                assert next == 'secondNode'
            else:
                assert next == 'firstNode'
        
    def testTerminatingPercentage(self):
        next = self.graph.next(1)
        next = self.graph.next()
        assert next == None
        

class GraphBuilder(object):
    defVarName = "definitionType"
    USE_FLAT = 0
    USE_LOCAL = 1
    USE_GLOBAL = 2
    USE_PROB = 3
    
    graphVarName = "graph"
    ancestorsVarName = "ancestors"
    probVarName = "score"
    startVarName = "startingPoint"
    
    
    
    def __init__(self):
        pass
    
    @classmethod
    def generateDependencyGraph(self,  cls,  methods = None):
        type = getattr(cls, GraphBuilder.defVarName,  GraphBuilder.USE_FLAT)
        startNode = getattr(cls, GraphBuilder.startVarName,  None)
        if methods and startNode not in methods:
            startNode = None
        
        return {
          GraphBuilder.USE_FLAT: self.generateUsingDefaults,
          GraphBuilder.USE_LOCAL:     self.generateUsingLocal,
          GraphBuilder.USE_GLOBAL:  self.generateUsingGlobal,
          GraphBuilder.USE_PROB:       self.generateUsingProbability,
        }[type](cls,  methods,  startNode)    
    
    @classmethod    
    def generateUsingDefaults(self,  cls,  methods = None,  startNode = None):
        graph = DependencyGraph(graph = {}, startNode = startNode)
        if methods is None:
            methods = getMatchedTypes(cls,   [MethodType])
        
        edges = []
        for method in methods:
            edges.append((method,  1))
        
        for item in edges:
            graph.setNode(item[0],  edges)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph
    
    @classmethod
    def generateUsingLocal(self,  cls,  methods = None,  startNode = None):
        graph = DependencyGraph(graph = {}, startNode = startNode)
        
        if methods is None:
            methods = getMatchedTypes(cls,   [MethodType])
        
        for method in methods:
            obj = getattr(cls,  method)
            ancestors = getattr(obj, GraphBuilder.ancestorsVarName,  None )
            if ancestors is not None:
                graph.setNode(method, ancestors)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph
    
    @classmethod
    def generateUsingGlobal(self,  cls,  methods = None,  startNode = None):
        graph = getattr(cls, GraphBuilder.graphVarName,  None)
        if graph is None:
            raise DependencyDeffinitionError("no graph defined while using global graph dependencies")
        
        if methods is not None:
            for key in graph.keys():
                if key not in methods:
                    graph.pop(key)
        
        return DependencyGraph(graph = graph,  startNode = startNode)
    
    @classmethod
    def generateUsingProbability(self,  cls,  methods = None,  defaultProbability = 0,  startNode = None):
        graph = DependencyGraph(graph = {}, startNode = startNode)
        if methods is None:
            methods = getMatchedTypes(cls,   [MethodType])
        
        edges = []
        for method in methods:
            obj = getattr(cls,  method)
            score = getattr(method,  probVarName,  defaultProbability)
            if score:
                edges.append((method,  score))
        
        for item in edges:
            graph.setNode(item[0],  edges)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph

class testGraphGenerator(TestCase):
    
    class GraphClass(object):
        startingPoint = 'startNode'
        
        def startNode(self):
            pass
        startNode.ancestors = [('unreachableNode', 0), ('secondNode', 100) ]
        startNode.score = 0
        def unreachableNode(self):
            pass
        unreachableNode.ancestors = []
        def secondNode(self):
            pass
        secondNode.ancestors = [('switchNode', 100)]
        def lastNode(self):
            pass
        lastNode.score = 1
        lastNode.ancestors = []
        def switchNode(self):
            pass
        switchNode.score = 0
        switchNode.ancestors = [('startNode',50),  ('lastNode', 50)]
        
        graph =    {
                         'startNode': [('unreachableNode', 0), ('secondNode', 100) ], 
                         'unreachableNode':[], 
                         'secondNode': [('switchNode', 100)], 
                         'switchNode': [('startNode',50),  ('lastNode', 50)], 
                         'lastNode': []
                        }
        
    
    def buildGraphsAndCompare(self,  reference,  buildMethod,  methods):
        referenceGraph = DependencyGraph(graph = reference,  startNode = self.GraphClass.startingPoint)
        self.GraphClass.definitionType = buildMethod
        generatedGraph = GraphBuilder.generateDependencyGraph(self.GraphClass,  methods)
        return generatedGraph.equals(referenceGraph)
    
    nonUniformGraph ={
                         'startNode': [('unreachableNode', 0), ('secondNode', 100) ], 
                         'unreachableNode':[], 
                         'secondNode': [('switchNode', 100)], 
                         'switchNode': [('startNode',50),  ('lastNode', 50)], 
                         'lastNode': []
                        }
    nonUniformMethods = ['startNode', 'secondNode', 'switchNode', 'lastNode', 'unreachableNode']
    
    def testLocal(self):
        assert self.buildGraphsAndCompare(reference = self.nonUniformGraph, buildMethod = GraphBuilder.USE_LOCAL, methods = self.nonUniformMethods)
    
    def testGlobal(self):
        assert self.buildGraphsAndCompare(reference = self.nonUniformGraph, buildMethod = GraphBuilder.USE_GLOBAL, methods = self.nonUniformMethods)
    
    nonUniformGraphShort ={
                         'startNode': [('unreachableNode', 0), ('secondNode', 100) ], 
                         'unreachableNode':[], 
                         'secondNode': [('switchNode', 100)], 
                         'switchNode': [('startNode',50),  ('lastNode', 50)],
                        }
    nonUniformMethodsShort = ['startNode', 'secondNode', 'switchNode', 'unreachableNode']
    """
    def testBlockedMethodGlobal(self):
        assert self.buildGraphsAndCompare(self.nonUniformGraphShort, GraphBuilder.USE_GLOBAL, self.nonUniformMethodsShort)
    
    def testBlockedMethodLocal(self):
        assert self.buildGraphsAndCompare(self.nonUniformGraphShort, GraphBuilder.USE_LOCAL, self.nonUniformMethodsShort)
    
    uniformMethods = ['startNode',  'lastNode', 'unreachableNode']
    uniformProbGraph = {
                            'startNode':[('lastNode', 1)], 
                            'lastNode':[('lastNode', 1)], 
                            'unreachableNode':[('lastNode', 1)], 
                         }
    
    def testProbability(self):
        assert self.buildGraphsAndCompare(self.uniformProbGraph, GraphBuilder.USE_PROB, self.uniformMethods)
    
    uniformFlatGraph =  {
                            'startNode':[('startNode', 1), ('lastNode', 1), ('unreachableNode', 1)], 
                            'lastNode':[('startNode', 1), ('lastNode', 1), ('unreachableNode', 1)], 
                            'unreachableNode':[('startNode', 1), ('lastNode', 1), ('unreachableNode', 1)], 
                            }
    
    def testFlat(self):
        assert self.buildGraphsAndCompare(self.uniformFlatGraph, GraphBuilder.USE_FLAT, self.uniformMethods)
"""

if __name__ == '__main__':
    unittest.main()

