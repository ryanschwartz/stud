all: stud stud-http stud-http-jem

stud: stud.c Makefile
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -L/sw/lib -I/sw/include -I. -o stud ringbuffer.c stud.c -D_GNU_SOURCE -lssl -lcrypto -lev

stud-http: stud.c Makefile http-parser/http_parser.o proto_http.c proto_http.h
	gcc -DPROTO_HTTP -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -L/sw/lib -I/sw/include -I. -o stud-http ringbuffer.c stud.c proto_http.c http-parser/http_parser.o -D_GNU_SOURCE -lssl -lcrypto -lev

stud-http-jem: stud.c Makefile http-parser/http_parser.o proto_http.c jemalloc/jemalloc/lib/libjemalloc.a
	gcc -DPROTO_HTTP -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -L/sw/lib -I/sw/include -I. -o stud-http-jem ringbuffer.c stud.c proto_http.c http-parser/http_parser.o -D_GNU_SOURCE -lssl -lcrypto -lev jemalloc/jemalloc/lib/libjemalloc.a -ldl -lpthread

http-parser/http_parser.o: http-parser/README.md
	make -C http-parser http_parser.o

http-parser/README.md:
	git clone http://github.com/ry/http-parser
	cd http-parser && git checkout -b jul6_stable 1786fdae36d3d40d59463dacab1cfb4165cf9f1d

jemalloc/jemalloc/lib/libjemalloc.a: jemalloc/jemalloc/autogen.sh

jemalloc/jemalloc/autogen.sh:
	git clone git://canonware.com/jemalloc.git
	cd jemalloc/jemalloc; ./autogen.sh; make -j4

install: stud
	cp stud /usr/local/bin

clean:
	rm -f stud stud-http stud-http-jem *.o tags
	rm -rf jemalloc/ http-parser/
