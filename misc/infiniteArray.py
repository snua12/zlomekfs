class InfiniteArray(object):
    
    __overridenAttributes = ['__overridenAttributes', '__extendedAttributes', \
        '__init__', '__expand__', '__getitem__', '__setitem__', 'array', \
        '__class__', 'next', 'step', '__len__', '__str__', '__repr__']
    
    def __getattribute__(self, name):
        """ Overriding getattribute method for object attributes
            redirects ChainedTestCase.__globalAttributes to self.inst
        """
        if name in InfiniteArray.__overridenAttributes:
            return super(InfiniteArray, self).__getattribute__(name)
        elif not hasattr(self, 'array') or self.array is None:
            raise AttributeError()
        else:
            return self.array.__getattribute__(name)
                
    __getattr__ = __getattribute__
        
    def __setattr__(self, name, value):
        """ Overriding access method for object attributes
            redirects ChainedTestCase.__globalAttributes to self.inst
        """
        if name in InfiniteArray.__overridenAttributes:
            return super(InfiniteArray, self).__setattr__(name, value)
        elif not self.array:
            raise AttributeError()
        else:
            return self.array.__setattr__(name, value)
    
    def __init__(self, begin = None, step = 1):
        self.array = []
        if begin:
            self.append(begin)
            self.next = begin + step
        else:
            self.next = 0
        self.step = step
    
    def __expand__(self, key):
        if isinstance(key, int):
            if key < 0:
                return
            toGenerate = key - len(self) + 1
        elif isinstance(key, slice):
            if key.stop and key.stop >= 0:
                toGenerate = key.stop - len(self) + 1
            else:
                return
        else:
            raise TypeError
        last = self.next + (toGenerate) * self.step 
        self.array.extend(range(self.next, last,self.step))
        self.next = last
    
    
    def __getitem__(self, key):
        self.__expand__(key)
        return self.array.__getitem__(key)
        
    def __setitem__(self, key, value):
        self.__expand__(key)
        return self.array.__setitem__(key, value)
        
    def __len__(self):
        return len(self.array)
    
    def __str__(self):
        return self.array.__str__()
        
    __repr__ = __str__
