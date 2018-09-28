CC      = $(CROSS_COMPILE)g++
CFLAGS  +=  -c -Wall $(EXTRAFLAGS)
LDFLAGS +=  -lcurl
EV_OBJ  = $(patsubst %.cpp,%.o,$(wildcard evohomeclient/*.cpp)) $(patsubst %.cpp,%.o,$(wildcard evohomeclient/jsoncpp/*.cpp))
DEPS    = $(wildcard evohomeclient/*.h) $(wildcard evohomeclient/jsoncpp/*.h) $(wildcard demo/*.h)


all: evo-demo evo-cmd evo-schedule-backup evo-setmode evo-settemp

evo-demo: demo/evo-demo.o $(EV_OBJ)
	$(CC) demo/evo-demo.o $(EV_OBJ) $(LDFLAGS) -o evo-demo

evo-cmd: demo/evo-cmd.o $(EV_OBJ)
	$(CC) demo/evo-cmd.o $(EV_OBJ) $(LDFLAGS) -o evo-cmd

evo-schedule-backup: demo/evo-schedule-backup.o $(EV_OBJ)
	$(CC) demo/evo-schedule-backup.o $(EV_OBJ) $(LDFLAGS) -o evo-schedule-backup

evo-setmode: demo/evo-setmode.o $(EV_OBJ)
	$(CC) demo/evo-setmode.o $(EV_OBJ) $(LDFLAGS) -o evo-setmode

evo-settemp: demo/evo-settemp.o $(EV_OBJ)
	$(CC) demo/evo-settemp.o $(EV_OBJ) $(LDFLAGS) -o evo-settemp


%.o: %.cpp $(DEP)
	$(CC) $(CFLAGS) $(EXTRAFLAGS) $< -o $@

distclean: clean

clean:
	rm -f $(EV_OBJ) $(wildcard demo/*.o) evo-demo evo-cmd evo-schedule-backup evo-setmode evo-settemp

