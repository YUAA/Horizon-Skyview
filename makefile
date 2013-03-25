missionControl: missionControl.cpp devices/*.cpp devices/nmeaParse/*.cpp akp/cAkpParser/*.c
	g++ -std=c++0x -pedantic -g $^ -o $@
clean:
	rm missionControl