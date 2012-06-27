MPICXX=mpicxx

J=1

all:
	# compile the application
	$(MPICXX) main.cpp -c -o main.o -I . -I RayPlatform -g
	$(MPICXX) plugins/NetworkTest/NetworkTest.cpp -c -o plugins/NetworkTest/NetworkTest.o -I . -I RayPlatform -g

	# compile the platform
	cd RayPlatform; make -j $(J) CXXFLAGS=" -g "
	
	# link them
	$(MPICXX) main.o plugins/NetworkTest/NetworkTest.o RayPlatform/libRayPlatform.a -o MessageWarden

clean:
	rm *.o MessageWarden -rf
	cd RayPlatform; make clean

