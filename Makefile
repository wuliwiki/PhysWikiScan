goal: main.cpp lib/PhysWikiScan.h
	g++ -O3 --std=c++17 main.cpp -o PhysWikiScan

clean:
	rm -f PhysWikiScan *.o *.gch
