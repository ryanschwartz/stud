all: stud stud-jem stud-http stud-http-jem

BASIC_DEPS=stud.c ringbuffer.c
HTTP_DEPS=http-parser/http_parser.o proto_http.c proto_http.h
JEM_DEPS=jemalloc/jemalloc/lib/libjemalloc.a
LIBEV_DEPS=libev/lib/libev.a

HTTP_JEM_DEPS=$(HTTP_DEPS) $(JEM_DEPS)

stud: stud.c Makefile $(LIBEV_DEPS)
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -Ilibev/include -I. -o $@ ringbuffer.c stud.c -D_GNU_SOURCE -lssl libev/lib/libev.a -lcrypto -lm

stud-jem: stud.c Makefile $(JEM_DEPS) $(LIBEV_DEPS)
	gcc -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -Ilibev/include -I. -o $@ ringbuffer.c stud.c -D_GNU_SOURCE -lssl -lcrypto libev/lib/libev.a -lm jemalloc/jemalloc/lib/libjemalloc.a -ldl -lpthread

stud-http: stud.c Makefile $(HTTP_DEPS) $(LIBEV_DEPS)
	gcc -DPROTO_HTTP -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -Ilibev/include -I. -o $@ ringbuffer.c stud.c proto_http.c http-parser/http_parser.o -D_GNU_SOURCE -lssl -lcrypto libev/lib/libev.a -lm

stud-http-jem: stud.c Makefile $(HTTP_DEPS) $(JEM_DEPS) $(LIBEV_DEPS)
	gcc -DPROTO_HTTP -O2 -g -std=c99 -fno-strict-aliasing -Wall -W -I/usr/local/include -L/usr/local/lib -Ilibev/include -I. -o $@ ringbuffer.c stud.c proto_http.c http-parser/http_parser.o -D_GNU_SOURCE -lssl -lcrypto libev/lib/libev.a -lm jemalloc/jemalloc/lib/libjemalloc.a -ldl -lpthread

http-parser/http_parser.o: http-parser/README.md
	make -C http-parser http_parser.o

http-parser/README.md:
	git clone http://github.com/ry/http-parser
	cd http-parser && git checkout -b jul6_stable 1786fdae36d3d40d59463dacab1cfb4165cf9f1d

jemalloc/jemalloc/lib/libjemalloc.a: jemalloc/jemalloc/autogen.sh

jemalloc/jemalloc/autogen.sh:
	git clone git://canonware.com/jemalloc.git
	cd jemalloc/jemalloc; ./autogen.sh; make -j4

libev/lib/libev.a:
	curl -O http://dist.schmorp.de/libev/libev-4.04.tar.gz
	tar xfvzp libev*.gz
	cd libev-*; ./configure --prefix=`pwd`/../libev/; make install

install: stud
	cp stud /usr/local/bin

clean:
	rm -f stud stud-jem stud-http stud-http-jem *.o tags
	rm -rf jemalloc/ http-parser/ libev*
