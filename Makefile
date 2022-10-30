ALL:test
test: leptjson.o test.o
	g++ leptjson.o test.o -o $@
test.o:test.cpp
	g++ -c test.cpp -o $@
leptjson.o:leptjson.cpp leptjson.h
	g++ -c $< -o $@
clean:
	rm -rf leptjson.o test.o test
