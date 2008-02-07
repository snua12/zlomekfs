""" Module with generic functions and fields for generating resultRepository html """


from django.utils.translation import ugettext as _
from django.core.paginator import ObjectPaginator, InvalidPage

# internationalize string -- _(string)

DEFAULT_PAGING = 20


testDetailDir = 'testDetail' 
""" relative html path where details of tests should be queried """

testListDir = 'tests'
""" relative html path where test list should be queried """

batchDetailDir = 'batchDetail'
""" relative html path where details of batches should be queried """

batchListDir = 'batches'
""" relative html path where batch list should be queried """

projectListDir = 'projects'
""" relative html path where project list should be queried """

globalMenu = "".join([
        '<span id = "small-header">View:</span><br>',
        '<a href="/' + batchListDir + '/">Batches</a><br/>',
        '<a href="/' + projectListDir + '/">Projects</a><br/>',
        '<a href="/' + testListDir + '/">Tests</a><br/>'])
""" menu with links to all site parts """    

def formatDuration(duration):
    """format time duration (milisec int) into user readable string
    .. Note:: we ignore days and above
    
    :Parameters:
        duration: integer - duration in miliseconds
    :Return: 
        string - user readable representation of duration
    """
    if not isinstance(duration, int):
        return "Unknown"
    milis = duration % 1000
    duration /= 1000
    sec = duration % 60
    duration /= 60
    min = duration % 60
    ''' ignore long lasting tests
    duration /= 60
    hour = duration % 24
    duration /= 24
    day = duration
    '''
    string = str(milis)
    while len(string) < 3:
        string = '0' + string
        
    string = str(sec) + "." + string + "s"
    if min:
        string = str(min) + " min " + string
    
    return string


def generatePagination(baseUrl, attrs, objects, pageSize):
    """generates division to pages of objects
    
    :Parameters:
        baseUrl: base url where pages should point
        attrs: attributes to put to other page links (should contain current page number)
        objects: all objects to split to pages
        pageSize: number of objects to display on one page
            
    :Return:
        tuple (htmlCode, objectSubset): htmlCode contains links to other pages and current page num
            objectSubset is subset of objects for current page
    """
    html = ""
    try:
        page = int(attrs['page']) - 1
    except KeyError:
        page = 0
    baseAddr = generateAddress(baseUrl, attrs, 'page')
    
    pagination = ObjectPaginator(objects, pageSize)

    if page < 0 or page >= pagination.pages:
        page = 0
    
    html += "page " + str(page + 1) + " of " + str(pagination.pages) + "<br/>"
    html += "pages: "
    for pgnum in pagination.page_range:
        html += "&nbsp; "  
        if pgnum -1 == page:
            html += str(pgnum)
        else:
            html += "<a href=\""+ baseAddr + "page=" + str(pgnum) + "&\">" + \
                        str(pgnum) + "</a>"
        html += "&nbsp;"
    
    return (html, pagination.get_page(page))

## list of attributes that should not be available for releasing with releasement links
noReleasementAttrs = ['page']    

def generateAttrReleasementLinks(baseUrl, attrs, text):
    """generate links to the same page without one of current attributes
    
    :Parameters:
        baseUrl: base page url
        attrs: attributes that should be passed to links 
        text: text to prepend to attr name in links
    :Return:
        html code (len(attrs) links (lines) each containing all attrs except one)
    """
    html = ""
    for attr in attrs.keys():
        if attr not in noReleasementAttrs:
            html += generateLink(baseUrl, attrs, text + " " + str(attr), attr) + "<br/>"
            
    return html

def generateLink(baseUrl, attrs, name, excludeAttr = None, switchAttr = None):
    """generate link html code
    
    :Parameters:
        baseUrl: base url to which attributes should be appended
        name: link text
    :Return:
        html code
    .. See: `generateAddress` for undocumented params (should be the same)
    """
    return "<a href=\"" + generateAddress(baseUrl, attrs, excludeAttr, switchAttr) + \
                "\">" + name + "</a>"
    
def switchAttrValue(dictionary, attr, value):
    """switch attribute in dictionary returning old value
    
    :Parameters:
        dictionary: dictionary to change
        attr: attribute name to change
        value: value to set to attribute
    :Return:
        old value of attribute attr
    """
    try:
        back = dictionary[attr]
    except KeyError:
        back = None
    if value is not None:
        dictionary[attr] = value
    else:
        try:
            dictionary.pop(attr)
        except KeyError:
            pass
    return back
    
def generateAddress(baseUrl, attrs = None, excludeAttr = None, switchAttr = None):
    """generate url with given attributes
    
    :Parameters:
        baseUrl base: url to use. Should not contain attributes
        attrs: attributes to append. Null value will return baseUrl
            otherwise ? and attributes will be appended (thus address will end with ? or &)
        excludeAttr: attribute to ignore when generating address
        switchAttr: tuple (attrName, attrValue)
            attribute attrName will be used with attrValue insead of the one listed in attrs
    :Return:
        plain text address
    """
    if switchAttr:
        back = switchAttrValue(attrs, switchAttr[0], switchAttr[1])
    if attrs is None:
        return baseUrl
    addr = baseUrl + "?"
    for key in attrs.keys():
        if key != excludeAttr:
            addr += key + "=" + str(attrs[key]) + "&"
    if switchAttr:
        switchAttrValue(attrs, switchAttr[0], back)
    return addr

def index(request):
    """Global repository index
        
    :Parameters:
        request: django request object
    :Return:
        html code to view
    """
    from batchViews import batchList
    return batchList(request)
