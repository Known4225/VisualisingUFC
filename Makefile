all:
	gcc VisualisingUFC.c -L./Linux -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -DDEBUGGING_FLAG -Wall -o VisualisingUFC.o
rel:
	gcc VisualisingUFC.c -L./Linux -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -O3 -o VisualisingUFC.o
win:
	gcc VisualisingUFC.c -L./Windows -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -lwsock32 -lWs2_32 -DOS_WINDOWS -DDEBUGGING_FLAG -Wall -o VisualisingUFC.exe
winrel:
	gcc VisualisingUFC.c -L./Windows -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -lwsock32 -lWs2_32 -DOS_WINDOWS -O3 -o VisualisingUFC.exe