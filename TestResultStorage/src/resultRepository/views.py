
from django.utils.translation import ugettext as _

# internationalize string -- _(string)

DEFAULT_PAGING = 20

testDetailDir = 'testDetail'
testListDir = 'tests'
batchDetailDir = 'batchDetail'
batchListDir = 'batches'
projectListDir = 'projects'
projectDetailDir = 'projectDetail'


globalMenu = "".join([
        '<span id = "small-header">Goto:</span><br>',
        '<a href="/' + batchListDir + '/">Batches</a><br/>',
        '<a href="/' + projectListDir + '/">Projects</a><br/>',
        '<a href="/' + testListDir + '/">Tests</a><br/>'])
    
from django.core.paginator import ObjectPaginator, InvalidPage

def generatePagination(baseUrl, attrs, objects, pageSize):
    '''
    page got in positive, internally using as zero based
    return (htmlCode, objectSubset)
    '''
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

noReleasementAttrs = ['page']    
def generateAttrReleasementLinks(baseUrl, attrs, text):
    html = ""
    for attr in attrs.keys():
        if attr not in noReleasementAttrs:
            html += generateLink(baseUrl, attrs, text + " " + str(attr), attr) + "<br/>"
            
    return html

    
def generateLink(baseUrl, attrs, name, excludeTag = None, switchAttr = None):
    return "<a href=\"" + generateAddress(baseUrl, attrs, excludeTag, switchAttr) + \
                "\">" + name + "</a>"
    
def switchAttrValue(dictionary, attr, value):
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
    
def generateAddress(baseUrl, attrs, excludeAttr = None, switchAttr = None):
    if switchAttr:
        back = switchAttrValue(attrs, switchAttr[0], switchAttr[1])
    if not attrs:
        return baseUrl + "?"
    addr = baseUrl + "?"
    for key in attrs.keys():
        if key != excludeAttr:
            addr += key + "=" + str(attrs[key]) + "&"
    if switchAttr:
        switchAttrValue(attrs, switchAttr[0], back)
    return addr

def index(request):
    from batchViews import batchList
    return batchList(request)
