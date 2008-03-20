""" Module with code handling test dependency graphs.
    Load them from test class, create graph and generate random path through graph.
 """

import unittest
import copy

from random import SystemRandom
from util import getMatchedTypes
from copy import copy
from types import MethodType
from unittest import TestCase

class DependencyDeffinitionError(Exception):
    """ Error raised when dependency deffinition is illegal
        or missing
    """
    pass

class DependencyGraph(object):
    """ Representation of dependency graph between test methods in test class.
        Provides abstraction for transistions and graph walk.
    """
    graph = None
    """ Graph of transistions (directory) in format:
        methodName: [(nameOfMethodToWhichEdgeExists, EdgeScore), ... ]
    """
    currentNode = None
    """ State information in graph walk. """
    
    randomGenerator = SystemRandom()
    """ Random generator used to generate random path through graph. """
    
    stopProbability = 0
    """ Probability to stop after test. 0.1 means 10% """
    
    def equals(self,  graph):
        """ Compares this graph with given and returns if they contains equal information.
            
            :Parameters:
                graph: DependencyGraph instance
                
            :Return:
                True if graphs and currentNode are the same.
        """
        return self.graph == graph.graph and self.currentNode == graph.currentNode
    
    def getNodeList(self):
        """ Return list of known nodes
            
            :Return:
                list of nodes in this graph
        """
        return copy(self.graph.keys())
        
    def __init__(self,  graph = None, startNode = None):
        """ Constructor. Takes optional arguments to bootstrap state.
            
            :Parameters:
                graph: graph to start with in directory format
                startNode: method name which should go first
        """
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
        """ Reset graph walk state by set startNode = currentNode to new, random possition in graph. """
        self.restart( self.randomGenerator.choice(self.graph.keys()) )
    
    def setEdge(self,  start,  end,  prob):
        """ Appends edge to graph. NOTE: multiple paths start -> end are allowd.
            
            :Parameters:
                start: method name from which transistion should be set
                end: method name to which transistion should lead
                probability: score of edge
        """
        try:
            edges = self.graph[start]
            edges.append((end, prob))
            
        except KeyError:
            self.graph[start] = [(end, prob)]
        
    
    def setNode(self,  node,  edges):
        """ Set node and it's successors.
            
            :Parameters:
                node: should be string (method name)
                edges: should be list of pairs (nodeName, score)
                    where nodeName is string and score is integer
        """
        self.graph[node] = edges
    
    def setCurrentNode(self,  node):
        """ Set current possition in graph (next item returned).
            
            :Parameters:
                node: node name
        """
        self.currentNode = node
        
    def setStartNode(self, node):
        """ Set start node in graph. Doesn't reset current node.
            
            :Parameters:
                node: node name
        """
        self.startNode = node
    
    def restart(self,  startNode = None):
        """ Restart graph walkthrough. 
            
            :Parameters:
                startNode: name of first node returned (if given)
        """
        if startNode:
            self.startNode = startNode
        self.currentNode = self.startNode
    
    @classmethod
    def getNodeListSum(self, list):
        """ Returns sum of scores of all edges in list.
            
            :Parameters:
                list: list of edges (as stored in graph for one node)
            
            :Return:
                sum(item[1] where item in list)
        """
        sum = 0
        for item in list:
            sum += item[1]
        return sum

    @classmethod
    def getNodeByRangeHit(self, list,  hit):
        """ Return node in edge such as sum of scores of previous edges is less than hit 
            and sum + edge score is greater than hit.
            
            :Parameters:
                list: edge list as stored in graph for one node
                hit: int in range (0, sum(edge[1] where edge in list)
            
            :Return:
                node name or None (if outside of range)
        """
        for item in list:
            hit -= item[1]
            if hit <=0:
                return item[0]
        return None
        
    def setStopProbability(self, probability):
        """ Set default stop probability for this graph.
            
            :Parameters:
                probability: probablility of terminating graph walk early.
                    0.1 means 10% to stop 
        """
        self.stopProbability = probability
        
    def getCurrent(self):
        """ Return current node
            
            :Return:
                self.currentNode
        """
        return self.currentNode
    
    def next(self,  stopProbability = 0):
        """ Returns current node and shift to next.
            
            :Parameter:
                stopProbability: probability (0,1) of walk termination
                    just for this call, if not defined, default from class will be used
                
            :Return:
                node or None
        """
        current = self.currentNode
        if stopProbability == 0:
            stopProbability = self.stopProbability
            
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
        """ Finds shortest path before start and end node, ignoring listed nodes.
            
            :Parameters:
                start: start node (first in path)
                end: end node (last in path)
                ignoredNodes: nodes that should not appear in path
                
            :Return:
                ordered list of nodes [start, ..., end] or None
        """
        if ignoredNodes:
            visited = ignoredNodes
        else:
            visited = []
            
        queue = []
        for (next, prob) in self.graph[start]:
            if prob > 0:
                queue.append((next, [start]))
        
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
                ancestors = self.graph[node]
                for (next, prob) in ancestors:
                    if prob > 0:
                        queue.append((next, newAncestors))
            except KeyError:
                continue
            
        return None
    

class testLinearGraph(TestCase):
    """ TestCase for operations on linear graph (path) """
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
    """ TestCase for test of shortestPath method """
    graph = None
    def setUp(self):
        self.graph = DependencyGraph(
                                     {
                                     '1': [('2', 1), ('3', 1)], 
                                     '2': [('4', 1), ('5', 1)], 
                                     '3': [('4', 1), ('6', 1)], 
                                     '4': [('5', 1)], 
                                     '5': [('6', 1)], 
                                     '6': [],
                                     
                                     '7':[('7', 1)],
                                     
                                     '8':[('9',1)],
                                     '9':[('8',1)],
                                     
                                     }, 
                                     '1')
    
    def testCycle(self):
        assert self.graph.getShortestPath('7', '7') == ['7', '7']
        assert self.graph.getShortestPath('8', '8') == ['8', '9', '8']
        assert self.graph.getShortestPath('9', '9') == ['9', '8', '9']
        assert self.graph.getShortestPath('9', '9', ['8']) == None
        
        
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
        
class testBrokenShortests(unittest.TestCase):
    graph = {
        'first': [('second',1), ('third', 1)],
        'second': [('first', 1)],
        'third': [('second', 1), ('fourth', 1)],
        'fourth': [('second', 1)],
        }
        
    def setUp(self):
        self.dependencyGraph = DependencyGraph(self.graph)
    def tearDown(self):
        self.graph = None
        
    def testFirstToFirst(self):
        assert self.dependencyGraph.getShortestPath('first', 'first', ['second']) == None

class testInfiniteGraph(TestCase):
    """ TestCase for testing infinite graph walkthrough
        NOTE: tests are heuristic, we can't test infinity :)
    """
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
    """ Class that defines helper functions to extract transistions between methods 
        from TestClass and transform them into DependencyGraph object.
    """
    
    defVarName = "definitionType"
    """ TestClass attribute name which specifies used definition type """
    
    USE_FLAT = 0
    """ Constant for definitionType with meaning 'all transistions are possible, scores are even' """
    
    USE_LOCAL = 1
    """ Constant for definitionType with meaning 'get edges from method from method attribute' """
    
    USE_GLOBAL = 2
    """ Constant for definitionType with meaning 'all transistions (whole graph) is defined in class itself' """
    
    USE_PROB = 3
    """ Constant for definitionType with meaning 'all transistions are possible, all edges to given node 
    has score defined in method attribute'
   """
    
    graphVarName = "graph"
    """ TestCase class attribute name which defines whole graph. """
    
    nodeGraphVarName = "successors"
    """ Method attribute name defining edges from this method. """
    
    probVarName = "score"
    """ Method attribute name defining probability of usage of this method. """
    
    startVarName = "startingPoint"
    """ TestCase class attribute name which defines first method called """
    
    
    def __init__(self):
        pass
    
    @classmethod
    def generateDependencyGraph(self,  cls,  methods = None):
        """ Generate dependency graph for given class using given methods.
            
            :Parameters:
                cls: class to generate graph from
                methods: methods allowed to use. 
                    Only these will be used in resulting graph.
                    If None, all methods from class will be used.
            
            :Return:
                DependencyGraph instance
        """
        if not methods:
            methods = getMatchedTypes(cls,   [MethodType])
        
        type = getattr(cls, GraphBuilder.defVarName,  GraphBuilder.USE_FLAT)
        startNode = getattr(cls, GraphBuilder.startVarName,  None)
        if methods and startNode not in methods:
            startNode = None
        
        return {
          GraphBuilder.USE_FLAT: self.generateUsingDefaults,
          GraphBuilder.USE_LOCAL:     self.generateUsingLocal,
          GraphBuilder.USE_GLOBAL:  self.generateUsingGlobal,
          GraphBuilder.USE_PROB:       self.generateUsingProbability,
        }[type](cls = cls,  methods = methods,  startNode = startNode)    
    
    @classmethod    
    def generateUsingDefaults(self,  cls,  methods,  startNode = None):
        """ Generate dependency graph with flat probabilities (all edges are even, 
            from every node goes edges to all other nodes including self)
            
            .. See generateDependencyGraph
        """
        graph = DependencyGraph(graph = {}, startNode = startNode)
        
        edges = []
        for method in methods:
            edges.append((method,  1))
        
        for item in edges:
            graph.setNode(item[0],  edges)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph
    
    @classmethod
    def generateUsingLocal(self,  cls,  methods,  startNode = None):
        """ Generate dependency graph using local transistion deffinitions.
            Edges from method are specified in method attribute.
            
            .. See generateDependencyGraph
        """
        graph = DependencyGraph(graph = {}, startNode = startNode)
        
        
        for method in methods:
            obj = getattr(cls,  method)
            successors = getattr(obj, GraphBuilder.nodeGraphVarName,  None )
            if successors is not None:
                graph.setNode(method, successors)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph
    
    @classmethod
    def generateUsingGlobal(self,  cls,  methods = None,  startNode = None):
        """ Generate dependency graph from graph defined in class.
            Transistions from and to methods not listed will be discarded.
            
            .. See generateDependencyGraph
        """
        graph = getattr(cls, GraphBuilder.graphVarName,  None)
        if graph is None:
            raise DependencyDeffinitionError("no graph defined while using global graph dependencies")
        else:
            graph = copy(graph)
        
        if methods:
            for key in graph.keys():
                if key not in methods:
                    graph.pop(key)
        
        return DependencyGraph(graph = graph,  startNode = startNode)
    
    @classmethod
    def generateUsingProbability(self,  cls,  methods, startNode = None, defaultProbability = 0):
        """ Generate dependency graph with using probabilities.
            From every node goes edges to all other nodes including self,
            score of edge is defined by attribute of target node (method)
            
            .. See generateDependencyGraph
        """
        graph = DependencyGraph(graph = {}, startNode = startNode)
        
        edges = []
        for method in methods:
            obj = getattr(cls,  method)
            score = getattr(obj,  GraphBuilder.probVarName,  defaultProbability)
            if score:
                edges.append((method,  score))
        
        for item in edges:
            graph.setNode(item[0],  edges)
        
        if startNode is None:
            graph.initRandomStartNode()
        
        return graph

class testGraphGenerator(TestCase):
    """ Tests for GraphGenerator """
    class GraphClass(object):
        startingPoint = 'startNode'
        
        def startNode(self):
            pass
        startNode.successors = [('unreachableNode', 0), ('secondNode', 100) ]
        startNode.score = 0
        def unreachableNode(self):
            pass
        unreachableNode.successors = []
        def secondNode(self):
            pass
        secondNode.successors = [('switchNode', 100)]
        def lastNode(self):
            pass
        lastNode.score = 1
        lastNode.successors = []
        def switchNode(self):
            pass
        switchNode.score = 0
        switchNode.successors = [('startNode',50),  ('lastNode', 50)]
        
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
    
    def testBlockedMethodGlobal(self):
        assert self.buildGraphsAndCompare(self.nonUniformGraphShort, GraphBuilder.USE_GLOBAL, self.nonUniformMethodsShort)
    
    def testBlockedMethodLocal(self):
        assert self.buildGraphsAndCompare(self.nonUniformGraphShort, GraphBuilder.USE_LOCAL, self.nonUniformMethodsShort)
    
    uniformMethods = ['startNode',  'lastNode', 'unreachableNode']
    uniformProbGraph = {
                            'lastNode':[('lastNode', 1)]
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


if __name__ == '__main__':
    unittest.main()

