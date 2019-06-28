PhysWikiScan:main.cpp PhysWikiScan/PhysWikiScan.h
	g++ -O3 --std=c++17 main.cpp -o PhysWikiScan.x

clean:
	rm -f PhysWikiScan.x *.o *.gch
