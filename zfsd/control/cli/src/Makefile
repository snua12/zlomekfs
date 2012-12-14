CXXFLAGS=-I../../include/cli -I../../include
LDFLAGS=-lncurses -L /usr/lib
CLI_DIR=../../../schema
ZFSD_XML=zfsd.xml

gozfsd:gozfsd.o
	c++ ${LDFLAGS} -o gozfsd  gozfsd.o ../../../cpp/build/make/Cygwing++/Release/libclicpp.a /usr/lib/libncurses.a

gozfsd.o:gozfsd.cpp zfsd_cli.h
	c++ ${CXXFLAGS} -c -o gozfsd.o gozfsd.cpp


zfsd_cli.h:zfsd.xml
	xsltproc --stringparam STR_CliClassName 'ZfsdCli' ../../xsl/cppclic.xsl $< > zfsd_cli.h || ( rm $@ && false )

validate:
	xmllint --schema ${CLI_DIR}/cli.xsd --noout ${ZFSD_XML}
	xmllint --relaxng ${CLI_DIR}/cli.rng --noout --noout ${ZFSD_XML}

clean:
	rm -f *.o *.exe zfsd_cli.h
