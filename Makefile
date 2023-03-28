goal: main.cpp lib/PhysWikiScan.h
	g++ -fmax-errors=1 -O3 --std=c++11 main.cpp -l sqlite3 -o PhysWikiScan

clean:
	rm -f PhysWikiScan *.o *.gch
