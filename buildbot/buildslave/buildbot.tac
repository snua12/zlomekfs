
from twisted.application import service
from buildbot.slave.bot import BuildSlave

basedir = r'/home/buildslave/testproject'
buildmaster_host = '10.0.0.2'
port = 9989
slavename = 'vboxi386'
passwd = 'h2so42'
keepalive = 600
usepty = 1
umask = None

application = service.Application('buildslave')
s = BuildSlave(buildmaster_host, port, slavename, passwd, basedir,
               keepalive, usepty, umask=umask)
s.setServiceParent(application)

