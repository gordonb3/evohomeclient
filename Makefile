.PHONY : demo


DEMOS = evo-demo evo-cmd evo-settemp evo-setmode evo-schedule-backup


demo: demo/CMakeCache.txt
	make -C demo $(DEMOS)

demo/CMakeCache.txt:
	cmake -B demo -Sdemo

clean:
	rm -rf demo/CMakeFiles
	rm -f demo/CMakeCache.txt
	rm -f demo/*.cmake
	rm -f demo/Makefile
	rm -f $(DEMOS)
	rm -f libevohomeclient.a

