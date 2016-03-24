CPP=g++
CPPFLAGS=-I. -Ilinux
DEPS =  hellomake.h
OBJ = linux/linuxLog.o linux/linuxSPI.o Command.o StatusCommand.o afLib.o linux/gpiolib.o 

# - this isn't working for some reason
%.o: %.cpp $(DEPS)
	$(CPP) -c -o $@ $< $(CFLAGS)

afBlink: linux/afBlink.o $(OBJ)
	g++ -o afBlink linux/afBlink.o $(OBJ) 

clean:
	rm *.o linux/*.o afBlink
