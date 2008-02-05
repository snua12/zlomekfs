
from django.utils.translation import ugettext as _
from django.http import HttpResponse
from django.shortcuts import render_to_response, get_object_or_404
from models import BatchRun, Project, TestRun
from django.core.paginator import ObjectPaginator, InvalidPage
from views import globalMenu, generateLink, DEFAULT_PAGING
from views import generatePagination, generateAttrReleasementLinks
from views import testDetailDir, testListDir, batchDetailDir, batchListDir, projectListDir

batchListAttrs = ['page', 'paging', 'result', 'branch', 'project', 'orderBy', 'revision', 'hasFinished', 'host']

def filterByResult(objects, resultValue):
    return objects.filter( result = resultValue)
def filterByBranch(objects, branchValue):
    return objects.filter( branch = branchValue)
def filterByProject(objects, projectValue):
    return objects.filter( project = projectValue)
def filterByRevision(objects, revisionValue):
    return objects.filter( repositoryRevision = revisionValue)
def filterByHasFinished(objects, hasFinishedValue):
    return objects.filter( hasFinished = hasFinishedValue)
def filterByHost(objects, hostValue):
    return objects.filter( machineName = hostValue)

def filterByAttrs(objects, attrs):
    for attr in ('result', 'branch', 'project', 'revision', 'hasFinished', 'host'):
        try:
            objects = {
                'result': filterByResult,
                'branch':     filterByBranch,
                'project':  filterByProject,
                'revision':  filterByRevision,
                'hasFinished':  filterByHasFinished,
                'host': filterByHost,
            }[attr](objects, attrs[attr])
        except KeyError:
            pass
    return objects

def generateBatchDescription(batch):
    html = "<table> "
    '''
        <tr> \
        <th colspan=\"2\">Batch info</th> \
        </tr> \
        </tr>"
    '''
    html += '<tr><td>Start time</td>'
    html += '<td>' +  str(batch.startTime) + '</td></tr>'
    
    html += '<tr><td>Duration</td>'
    html += '<td>' + str(batch.duration) + '</td></tr>'
    
    html += '<tr><td>Result</td>'
    html += '<td>' +  batch.get_result_display() + '</td></tr>'
    
    html += '<tr><td>Finished</td>'
    html += '<td>' +  str(batch.hasFinished == 1) + '</td></tr>'
    
    html += '<tr><td>Test count</t>'
    if batch.hasFinished:
        testCount = batch.testCount
    else:
        testCount = TestRun.objects.filter(batchId = batch.id)
    html += '<td>' +  str(testCount) + '</td></tr>'
    
    html += '<tr><td>Project</td>'
    html += '<td>' +  batch.project.projectName + '</td></tr>'
    
    html += '<tr><td>Repository</td>'
    html += '<td>' +  batch.project.sourceRepositoryUrl + '</td></tr>'
    
    html += '<tr><td>Branch</td>'
    html += '<td>' + batch.branch + '</td></tr>'
    
    html += '<tr><td>Revision</td>'
    html += '<td>' +  str(batch.repositoryRevision) + '</td></tr>'
    
    #html += '<tr><td>Description</td>'
    #html += '<td>' +  batch.description + '</td></tr>'
    
    html += '<tr><td>Profile</td>'
    html += '<td>' +  batch.profileName + '</td></tr>'
    
    html += '<tr><td>Environment</td>'
    html += '<td>' + "" + '</td></tr>'
    
    html += '<tr><td>Host</td>'
    html += '<td>' + batch.machineName  + '</td></tr>'
    
    html += "</table>"
    return html
    
def generateBatchTable(baseUrl, batchList, attrs):
    html = ' \
    <table> \
        <tr> \
          <th>Project</th> \
          <th>Branch</th> \
          <th>Rev</th> \
          <th>Host</th> \
          <th>Start time</th> \
          <th>Duration</th> \
          <th>Batch</th> \
          <th>Finished</th> \
          <th>Result</th> \
          <th>Tests</th> \
        </tr> '
    for batch in batchList:
        html += "<tr id=\"" + batch.get_result_display() + "\">"
        html += "<td>" + generateLink(baseUrl, attrs, batch.project.projectName,
                        switchAttr = ('project', batch.project.id)) + "</td>" #project
        html += "<td>" + generateLink(baseUrl, attrs, batch.branch,
                        switchAttr = ('branch', batch.branch)) + "</td>" #branch
        if batch.repositoryRevision:
            html += "<td>" + generateLink(baseUrl, attrs, str(batch.repositoryRevision),
                        switchAttr = ('revision', batch.repositoryRevision)) + "</td>" #revision
        else:
            html += "<td>" + str(batch.repositoryRevision) + "</td>" #revision
        html += "<td>" + generateLink(baseUrl, attrs, batch.machineName,
                        switchAttr = ('host', batch.machineName)) + "</td>" #host
        html += "<td>" + str(batch.startTime) + "</td>" #startTime
        if batch.duration:
            html += "<td>" + str(batch.duration) + "</td>" #duration
        else:
            html += "<td>Unknown</td>" #duration
        html += "<td>" + generateLink('/' + batchDetailDir, {'batch' : batch.id},
                        str(batch.id)) + "</td>" #batch
        html += "<td>" + generateLink(baseUrl, attrs, str(batch.hasFinished == 1),
                        switchAttr = ('hasFinished', batch.hasFinished)) + "</td>" #hasFinished
        html += "<td>" + generateLink(baseUrl, attrs, batch.get_result_display(),
                        switchAttr = ('result', batch.result)) + "</td>" #result
        if batch.hasFinished:
            testCount = batch.testCount
        else:
            testCount = TestRun.objects.filter( batchId = batch.id).count()
        if testCount:
            html += "<td>" + generateLink('/' + testListDir, {'batch' : batch.id },
                        str(testCount)) + "</td>" #testCount
        else:
            html += "<td>0</td>"

        html += "</tr>"
    html += "</table>"
    return html

def batchList(request):
    attrs = {}
    for attr in batchListAttrs:
        try: 
            attrs[attr] = request.REQUEST[attr]
        except KeyError:
            pass
            
    batchList = filterByAttrs(BatchRun.objects.all(), attrs)
    
    try:
        ordering = attrs['orderBy']
    except KeyError:
        ordering = '-startTime'
    batchList = batchList.order_by(ordering)
    
    header = 'Batch list'
    (paginationHtml,pageObjects) = generatePagination(request.path, attrs, batchList, DEFAULT_PAGING)
    mainTableHtml = generateBatchTable(request.path, pageObjects, attrs)
    releasementsHtml = generateAttrReleasementLinks(request.path, attrs, _("Don't filter by "))
    overviewHtml = "total: " + str(batchList.count())
            
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
    
def batchDetail(request):
    try:
        batch = get_object_or_404(BatchRun, id = request.REQUEST['batch'])
        header = batch.description
        mainTableHtml = generateBatchDescription(batch)
    except KeyError:
        header = "Batch not specified"
        mainTableHtml = ""
    
    return render_to_response('resultRepository/batchrun_detail.html',
                                                {
                                        'title': header,
                                        'header': header,
                                        'globalMenu' : globalMenu,
                                        'bodyTable' : mainTableHtml,
                                                })
