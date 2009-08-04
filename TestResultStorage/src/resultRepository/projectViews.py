from views import globalMenu, generateLink, batchListDir, generatePagination, DEFAULT_PAGING
from models import Project
from django.shortcuts import render_to_response

def generateProjectList(projects):
    html = "<table> \
        <tr align=\"left\"> \
            <th>Name</th> \
            <th>Repository</th> \
        </tr>"
    for project in projects:
        html += "<tr>"
        html += "<td>" + project.projectName + "</td>"
        html += "<td>" + generateLink(project.sourceRepositoryUrl, None, project.sourceRepositoryUrl) + "</td>"
        html += "<td>" + generateLink("/" + batchListDir, {'project' : project.id}, "Show batches") + "</td>"
        html += "</tr>"
    html += "</table"
    return html


def projectList(request):
    if 'page' in request.REQUEST:
        attrs = {'page' : request.REQUEST['page']}
    else:
        attrs = {}
    projects = Project.objects.all()
    total = "total:"+ str(projects.count())
    header = "Projects"
    (paginationHtml, projects) = generatePagination(request.path, attrs, projects, DEFAULT_PAGING)
    mainTableHtml = generateProjectList(projects)
    
    return render_to_response('resultRepository/project_list.html',
                                                {
                                        'title': header,
                                        'header': header,
                                        'globalMenu' : globalMenu,
                                        'pagination': paginationHtml,
                                        'overview' : total,
                                        'bodyTable' : mainTableHtml,
                                                })
