from django.utils.translation import ugettext as _
from django.http import HttpResponse
from django.shortcuts import render_to_response, get_object_or_404
from models import TestRun, BatchRun, TEST_RESULT_CHOICES 
from django.core.paginator import ObjectPaginator, InvalidPage
from views import globalMenu, generateLink, DEFAULT_PAGING, formatDuration
from views import generatePagination, generateAttrReleasementLinks
from views import testDetailDir, testListDir, batchDetailDir, batchListDir, projectListDir
from batchViews import generateBatchDescription


testListAttrs = ['page', 'paging', 'result', 'batch', 'testName', 'orderBy']

def generateHeaderForAttrs(attrs):
    html = 'Test list'
    if 'testName' in attrs:
        html += ' for ' + attrs['testName']
    if 'batch' in attrs:
        html += ' in batch ' + attrs['batch']
    if 'result' in attrs:
        html += ' with result ' + attrs['result']
    return html

def filterByResult(objects, resultValue):
    return objects.filter(result = resultValue)
    
def filterByBatch(objects, batch):
    return objects.filter(batchId = batch)
    
def filterByTestName(objects, name):
    return objects.filter(testName = name)

def filterByAttrs(objects, attrs):
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
    html = "<table>"
    html += "<tr><td>Name</td>"
    html += "<td>" + test.testName + "</td></tr>"
    
    html += "<tr><td>Description</td>"
    html += "<td>" + test.description + "</td></tr>"
    
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
    html += "</table>"
    return html
    
def generateTestTable(baseUrl, testList, attrs):
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
    
