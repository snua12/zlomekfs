""" Module with functions and fields needed for generating html code about test results 
    Uses `views` module for basic operation, `models` module for database handling."""

import pickle
from django.utils.translation import ugettext as _
from django.utils.html import escape
from django.shortcuts import render_to_response, get_object_or_404
from views import globalMenu, generateLink, DEFAULT_PAGING, formatDuration
from views import generatePagination, generateAttrReleasementLinks
from views import testDetailDir, projectListDir, dataDir
from models import TestRun, TestRunData


testListAttrs = ['page', 'paging', 'result', 'batch', 'testName', 'orderBy']
""" attributes recognized by testList page """

def generateHeaderForAttrs(attrs):
    """ Generate header line for test list (include used attribtes)
    
    :Parameters:
        attrs: dictionary containing attributes used for filtering
    :Return:
        plain text describing list (for ex. TestList for myTest in batch firstBatch)
    """
    html = 'Test list'
    if 'testName' in attrs:
        html += ' for ' + attrs['testName']
    if 'batch' in attrs:
        html += ' in batch ' + attrs['batch']
    if 'result' in attrs:
        html += ' with result ' + attrs['result']
    return html

def filterByResult(objects, resultValue):
    """ filter objects by result
    
    :Parameters:
        objects: objects to filter (django Model.objects instance)
        resultValue: `TEST_RESULT_CHOICES` int value
    
    :Return:
        new QueryObject containing subset of objects
    """
    return objects.filter(result = resultValue)
    
def filterByBatch(objects, batch):
    """ filter objects by batch
    
    :Parameters:
        objects: objects to filter (django Model.objects instance)
        batch: batchId (int)
    
    :Return:
        new QueryObject containing subset of objects
    """
    return objects.filter(batchId = batch)
    
def filterByTestName(objects, name):
    """ filter objects by testName
    
    :Parameters:
        objects: objects to filter (django Model.objects instance)
        name: testName field value
    
    :Return:
        new QueryObject containing subset of objects
    """
    return objects.filter(testName = name)

def filterByAttrs(objects, attrs):
    """ filter objects by attributes values
    
    :Parameters:
        objects: objects to filter (django Model.objects instance)
        attrs: dictionary with pairs {attr:value}. 
            currently recognized are `result`, `batch` and `testName` attributes
    
    :Return:
        new QueryObject containing subset of objects
    """
    for attr in ('result', 'batch', 'testName'):
        try:
            objects = {
                'result': filterByResult,
                'batch':     filterByBatch,
                'testName':  filterByTestName,
            }[attr](objects, attrs[attr])
        except KeyError:
            pass
    return objects
    
def generateTestDescription(test):
    """Generate html code describing test object
    
    :Parameters:
        test: `TestResultStorage.resultRepository.models.TestRun` instance
        
    :Return:
        html code (table) describing given object.
    """
    html = "<table>"
    html += "<tr><td>Name</td>"
    html += "<td>" + test.testName + "</td></tr>"
    
    html += "<tr valign=\"top\"><td>Description</td>"
    html += "<td>" + escape(str(test.description)) + "</td></tr>"
    
    html += "<tr><td>Start time</td>"
    html += "<td>" + str(test.startTime) + "</td></tr>"
    
    html += "<tr><td>Duration</td>"
    html += "<td>" + formatDuration(test.duration) + "</td></tr>"
    
    html += "<tr><td>Batch</td>"
    html += "<td>" + str(test.batchId) + "</td></tr>"
    
    html += "<tr><td>Result</td>"
    html += "<td>" + test.get_result_display() + "</td></tr>"
    html += "<tr><td>Repository path</td>"
    html += "<td>" + test.sourceRepositoryPath + "</td></tr>"
    data = TestRunData.objects.filter(runId = test.id)
    for item in data:
        if item.errText:
            html += "<tr valign=\"top\"><td>Error</td>"
            html += "<td>" + escape(item.errText) + "</td></tr>"
        if item.backtrace:
            html += "<tr valign=\"top\"><td>Backtrace</td>"
            html += "<td>"
            try:
                backtrace = pickle.loads(item.backtrace)
                for line in backtrace:
                    html += escape(line) + "<br/>"
            except:
                html += "Can't load - invalid database format"
            html += "</td></tr>"
        if item.dumpFile:
            html += "<tr valign=\"top\"><td>Snapshot</td>"
            html += "<td>" + generateLink(baseUrl = "/" + dataDir + "/" + escape(item.dumpFile),
                                                attrs = None, name = escape(item.dumpFile)) + "</td></tr>"
    html += "</table>"
    return html
    
def generateTestTable(baseUrl, testList, attrs):
    """Generate html code with list (table) containing tests overview and filtering links
    
    :Parameters:
        baseUrl: url to current page (without attributes). 
            Will be used as base for filtering links
        testList: iterable object containing `TestResultStorage.resultRepository.models.TestRun` instances
        attrs: attributes given to current page (will be used to generate links)
        
    :Return:
        html code (table) with tests descriptions and filtering links
            links will point to `baseUrl` giving `attrs` and filtering attr
    """
    html = "<table> \
        <tr align=\"left\">  \
          <th>Test name</th> \
          <th>Duration</th> \
          <th>Result</th> \
          <th>Project</th> \
          <th>Branch</th> \
          <th>Batch</th> \
          <th>Details</th> \
        </tr> "
    for test in testList:
        html += "<tr id=\"" + test.get_result_display() + "\">"
        
        html += "<td>" + generateLink(baseUrl, attrs, test.testName,
                        switchAttr = ('testName', test.testName)) + "</td>" #name
        html += "<td>" + formatDuration(test.duration) + "</td>" #duration
        html += "<td>" + generateLink(baseUrl, attrs, _(test.get_result_display()),
                        switchAttr = ('result', test.result)) + "</td>" #result
        
        html += "<td>" + generateLink('/' + projectListDir, {}, str(test.batchId.project)) + "</td>" #project
        html += "<td>" + str(test.batchId.branch) + "</td>" #branch
        
        html += "<td>" + generateLink(baseUrl, attrs, str(test.batchId.id),
                        switchAttr = ('batch', test.batchId.id)) + "</td>" #batch
        html += "<td>" + generateLink('/' + testDetailDir, {'test' : test.id}, _("Details")) + "</td>" #details
        
        html += "</tr>"
    html += "</table>"
    return html

def testList(request):
    """Handle request for list of tests according `request`.
    
    :Parameters:
        request: django html request (`path` and `REQUEST` will be used)
        .. See: testListAttrs for used attributes
        
    :Return:
        html code (full page) with list of tests matching filtering attributes
    """
    attrs = {}
    for attr in testListAttrs:
        try: 
            attrs[attr] = request.REQUEST[attr]
        except KeyError:
            pass
            
    testList = filterByAttrs(TestRun.objects.all(), attrs)
    
    try:
        ordering = attrs['orderBy']
    except KeyError:
        ordering = '-startTime'
    testList = testList.order_by(ordering)
    
    header = generateHeaderForAttrs(attrs)
    (paginationHtml,pageObjects) = generatePagination(request.path, attrs, testList, DEFAULT_PAGING)
    mainTableHtml = generateTestTable(request.path, pageObjects, attrs)
    releasementsHtml = generateAttrReleasementLinks(request.path, attrs, _("Don't filter by "))
    overviewHtml = "total: " + str(testList.count())
    
    return render_to_response('resultRepository/testrun_list.html',
                                                {
                                        'title': header,
                                        'header': header,
                                        'globalMenu' : globalMenu,
                                        'pagination' : paginationHtml,
                                        'attrReleasements' : releasementsHtml,
                                        'overview' : overviewHtml,
                                        'bodyTable' : mainTableHtml,
                                                })
    
    
INVALID_TEST_ID = -1

def testDetail(request):
    """Handle request for test details.
    
    :Parameters:
        request: django html request (`path` and `REQUEST` will be used)
            should contain 'test' attribute - id of `TestRun` object
        
    :Return:
        html code (full page) with test description
    """
    try:
        test = get_object_or_404(TestRun, id = request.REQUEST['test'])
        header = test.testName
        mainTableHtml = generateTestDescription(test)
    #    mainTableHtml += generateBatchDescription(test.batchId)
    except KeyError:
        header = "Test not specified"
        mainTableHtml = ""
    
    return render_to_response('resultRepository/testrun_detail.html',
                                                {
                                        'title': header,
                                        'header': header,
                                        'globalMenu' : globalMenu,
                                        'bodyTable' : mainTableHtml,
                                                })
    
