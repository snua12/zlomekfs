

def getMatchedTypes(obj,  types):
    ret = []
    for name in dir(obj):
        item = getattr(obj,  name,  None)
        if type(item)  in types:
            log.debug("append object %s",  name)
            ret.append(name)
        
        return ret
