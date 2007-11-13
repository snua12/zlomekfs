#FIXME: when stopProbability == 0 graph for USE_FLAG should loop for infinity
import unittest

from random import SystemRandom
from util import getMatchedTypes
from types import MethodType
from unittest import TestCase

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
    
    def initRandomStartNode(self):
        self.currentNode = self.randomGenerator.choice(self.graph.keys())
    
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
        self.startNode = node
    
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
            if hit <0:
                return item[0]
        return None
    
    def getCurrent(self):
        return self.currentNode
    
    """
        Returns current and shift to next
    """
    def next(self,  stopProbability = 0):
        current = self.currentNode
        try:
            edges = self.graph[self.currentNode]
            sum = self.getNodeListSum(edges)
            if sum > 1:
                sum = sum + (sum * stopProbability)
                key = self.randomGenerator.randint(1, sum )
            else:
                key = sum
            self.currentNode = self.getNodeByRangeHit(edges,  key)
        except KeyError:
            self.currentNode = None
        return current
    

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
        if not cls:
            raise Exception("class for generation must ge given")
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
            raise Exception("no graph defined while using global graph dependencies")
        
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

