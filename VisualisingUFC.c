#include "include/turtleTools.h"
#include "include/osTools.h"
#include <time.h>

typedef struct {
    list_t *data;
    list_t *packetDefinitions;
    list_t *packets;
} visualising_ufc_t;

typedef enum {
    UFC_PACKET_FIELD_DELIMETER_UINT8 = 0,
    UFC_PACKET_FIELD_DELIMETER_UINT16 = 1,
    UFC_PACKET_FIELD_DELIMETER_UINT32 = 2,
    UFC_PACKET_FIELD_DELIMETER_UINT64 = 3,
    UFC_PACKET_FIELD_UINT8 = 4,
    UFC_PACKET_FIELD_INT8 = 5,
    UFC_PACKET_FIELD_UINT16 = 6,
    UFC_PACKET_FIELD_INT16 = 7,
    UFC_PACKET_FIELD_UINT32 = 8,
    UFC_PACKET_FIELD_INT32 = 9,
    UFC_PACKET_FIELD_UINT64 = 10,
    UFC_PACKET_FIELD_INT64 = 11,
    UFC_PACKET_FIELD_FLOAT = 12,
    UFC_PACKET_FIELD_DOUBLE = 13,
} ufc_packet_field_types_t;

visualising_ufc_t self;

int32_t matchPacket(int32_t packetIndex, uint8_t *file, uint32_t maxLength) {
    list_t *packetType = self.packetDefinitions -> data[packetIndex].r;
    if (maxLength <= packetType -> data[1].i) {
        return 0;
    }
    for (uint32_t i = 0; i < packetType -> data[1].i; i++) {
        /* we make use of the assumption that big endian machines don't exist - and it's 2025 so they really don't */
        switch (packetType -> data[i * 2 + 2].i) {
            case UFC_PACKET_FIELD_DELIMETER_UINT64:
            if (packetType -> data[i * 2 + 3].l != *((uint64_t *) (file + i))) {
                return 0;
            }
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT32:
            // printf("%X ", *((uint32_t *) (file + i)));
            if (packetType -> data[i * 2 + 3].u != *((uint32_t *) (file + i))) {
                return 0;
            }
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT16:
            if (packetType -> data[i * 2 + 3].h != *((uint16_t *) (file + i))) {
                return 0;
            }
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT8:
            if (packetType -> data[i * 2 + 3].b != file[i]) {
                return 0;
            }
            break;
            default:
            break;
        }
    }
    printf("bno packet spotted\n");
    return 1;
}

void import(char *filename) {
    /* memory map file */
    uint32_t fileSize = 0;
    uint8_t *fileData = mapFile(filename, &fileSize);
    // printf("size of %s: %d\n", filename, fileSize);
    if (fileData == NULL) {
        return;
    }
    for (uint32_t i = 0; i < fileSize; i++) {
        matchPacket(0, fileData + i, fileSize - i);
    }
    // for (uint32_t i = 0; i < 100; i++) {
    //     printf("%X ", fileData[i]);
    // }
    unmapFile(fileData);
}

void init() {
    self.data = list_init();
    self.packetDefinitions = list_init();
    self.packets = list_init();
    list_t *bnoPacket = list_init();
    list_append(bnoPacket, (unitype) "bnoPacket", 's');
    list_append(bnoPacket, (unitype) 52, 'i'); // bno packet is 52 bytes big
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(bnoPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(bnoPacket, (unitype) "timestamp", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 4 for bno packet
    list_append(bnoPacket, (unitype) 4, 'u');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(bnoPacket, (unitype) "state", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(bnoPacket, (unitype) "pkt_len", 's');

    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "accel_x", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "accel_y", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "accel_z", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "gyro_x", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "gyro_y", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "gyro_z", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "eul_heading", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "eul_roll", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(bnoPacket, (unitype) "eul_pitch", 's');

    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(bnoPacket, (unitype) 0xCA11AB1E, 'u');

    list_append(self.packetDefinitions, (unitype) bnoPacket, 'r');

    list_print(self.packetDefinitions);

    import("C:\\Information\\Random\\BackplaneCAN\\UFCData\\FlashBackup_Card4_222617.txt");
}

void parseRibbonOutput() {
    if (ribbonRender.output[0] == 1) {
        ribbonRender.output[0] = 0;
        if (ribbonRender.output[1] == 0) { // File
            if (ribbonRender.output[2] == 1) { // New
                printf("New\n");
            }
            if (ribbonRender.output[2] == 2) { // Save
                if (strcmp(osToolsFileDialog.selectedFilename, "null") == 0) {
                    if (osToolsFileDialogPrompt(1, "") != -1) {
                        printf("Saved to: %s\n", osToolsFileDialog.selectedFilename);
                    }
                } else {
                    printf("Saved to: %s\n", osToolsFileDialog.selectedFilename);
                }
            }
            if (ribbonRender.output[2] == 3) { // Save As...
                if (osToolsFileDialogPrompt(1, "") != -1) {
                    printf("Saved to: %s\n", osToolsFileDialog.selectedFilename);
                }
            }
            if (ribbonRender.output[2] == 4) { // Open
                if (osToolsFileDialogPrompt(0, "") != -1) {
                    printf("Loaded data from: %s\n", osToolsFileDialog.selectedFilename);
                }
            }
        }
        if (ribbonRender.output[1] == 1) { // Edit
            if (ribbonRender.output[2] == 1) { // Undo
                printf("Undo\n");
            }
            if (ribbonRender.output[2] == 2) { // Redo
                printf("Redo\n");
            }
            if (ribbonRender.output[2] == 3) { // Cut
                osToolsClipboardSetText("test123");
                printf("Cut \"test123\" to clipboard!\n");
            }
            if (ribbonRender.output[2] == 4) { // Copy
                osToolsClipboardSetText("test345");
                printf("Copied \"test345\" to clipboard!\n");
            }
            if (ribbonRender.output[2] == 5) { // Paste
                osToolsClipboardGetText();
                printf("Pasted \"%s\" from clipboard!\n", osToolsClipboard.text);
            }
        }
        if (ribbonRender.output[1] == 2) { // View
            if (ribbonRender.output[2] == 1) { // Change theme
                printf("Change theme\n");
                if (tt_theme == TT_THEME_DARK) {
                    turtleBgColor(36, 30, 32);
                    turtleToolsSetTheme(TT_THEME_COLT);
                } else if (tt_theme == TT_THEME_COLT) {
                    turtleBgColor(212, 201, 190);
                    turtleToolsSetTheme(TT_THEME_NAVY);
                } else if (tt_theme == TT_THEME_NAVY) {
                    turtleBgColor(255, 255, 255);
                    turtleToolsSetTheme(TT_THEME_LIGHT);
                } else if (tt_theme == TT_THEME_LIGHT) {
                    turtleBgColor(30, 30, 30);
                    turtleToolsSetTheme(TT_THEME_DARK);
                }
            } 
            if (ribbonRender.output[2] == 2) { // GLFW
                printf("GLFW settings\n");
            } 
        }
    }
}

int main(int argc, char *argv[]) {
    /* Initialize glfw */
    if (!glfwInit()) {
        return -1;
    }
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA (Anti-Aliasing) with 4 samples (must be done before window is created (?))

    /* Create a windowed mode window and its OpenGL context */
    const GLFWvidmode *monitorSize = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int32_t windowHeight = monitorSize -> height * 0.85;
    GLFWwindow *window = glfwCreateWindow(windowHeight * 16 / 9, windowHeight, "turtle demo", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetWindowSizeLimits(window, windowHeight * 16 / 9, windowHeight, windowHeight * 16 / 9, windowHeight);

    /* initialize turtle */
    turtleInit(window, -320, -180, 320, 180);
    /* initialise turtleText */
    turtleTextInit("include/roberto.tgl");
    /* initialise turtleTools ribbon */
    turtleToolsSetTheme(TT_THEME_DARK); // dark theme preset
    ribbonInit("include/ribbonConfig.txt");
    /* initialise osTools */
    osToolsInit(argv[0], window); // must include argv[0] to get executableFilepath, must include GLFW window
    osToolsFileDialogAddExtension("txt"); // add txt to extension restrictions

    uint32_t tps = 120; // ticks per second (locked to fps in this case)
    uint64_t tick = 0; // count number of ticks since application started
    clock_t start, end;

    init();

    turtleBgColor(30, 30, 30);
    int32_t buttonVar, switchVar = 0, dropdownVar = 0;
    double dialVar = 0.0, sliderVar = 0.0, scrollbarVarX = 0.0, scrollbarVarY = 0.0;
    list_t *dropdownOptions = list_init();
    list_append(dropdownOptions, (unitype) "a", 's');
    list_append(dropdownOptions, (unitype) "long item", 's');
    list_append(dropdownOptions, (unitype) "very long item name", 's');
    list_append(dropdownOptions, (unitype) "b", 's');
    list_append(dropdownOptions, (unitype) "c", 's');
    list_append(dropdownOptions, (unitype) "d", 's');
    list_append(dropdownOptions, (unitype) "e", 's');
    buttonInit("button", &buttonVar, TT_BUTTON_SHAPE_RECTANGLE, 150, 20, 10);
    switchInit("switch", &switchVar, 150, -20, 10);
    dialInit("dial", &dialVar, TT_DIAL_EXP, -150, 20, 10, 0, 1000, 1);
    dialInit("dial", &dialVar, TT_DIAL_LINEAR, -150, -20, 10, 0, 1000, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_LEFT, -100, 35, 10, 50, 0, 255, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_CENTER, 0, 35, 10, 50, 0, 255, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_RIGHT, 100, 35, 10, 50, 0, 255, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_VERTICAL, TT_SLIDER_ALIGN_LEFT, -100, -35, 10, 50, 0, 255, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_VERTICAL, TT_SLIDER_ALIGN_CENTER, 0, -35, 10, 50, 0, 255, 1);
    sliderInit("slider", &sliderVar, TT_SLIDER_VERTICAL, TT_SLIDER_ALIGN_RIGHT, 100, -35, 10, 50, 0, 255, 1);
    scrollbarInit(&scrollbarVarX, TT_SCROLLBAR_HORIZONTAL, 20, -170, 10, 550, 50);
    scrollbarInit(&scrollbarVarY, TT_SCROLLBAR_VERTICAL, 310, 0, 10, 320, 33);
    dropdownInit("dropdown", dropdownOptions, &dropdownVar, TT_DROPDOWN_ALIGN_CENTER, 0, 70, 10);

    double power = 0.0, speed = 0.0, exposure = 0.0, x = 103, y = 95, z = 215;
    int32_t xEnabled, yEnabled, zEnabled;
    list_t *sources = list_init();
    int sourceIndex = 0;
    list_append(sources, (unitype) "None", 's');
    list_append(sources, (unitype) "SP932", 's');
    list_append(sources, (unitype) "SP932U", 's');
    list_append(sources, (unitype) "SP928", 's');
    list_append(sources, (unitype) "SP1203", 's');
    list_append(sources, (unitype) "SP-1550M", 's');
    dialInit("Power", &power, TT_DIAL_LINEAR, -150, -210, 10, 0, 100, 1);
    dialInit("Speed", &speed, TT_DIAL_LINEAR, -100, -210, 10, 0, 1000, 1);
    dialInit("Exposure", &exposure, TT_DIAL_EXP, -50, -210, 10, 0, 1000, 1);
    dropdownInit("Source", sources, &sourceIndex, TT_DROPDOWN_ALIGN_LEFT, -10, -207, 10);
    tt_slider_t *xSlider = sliderInit("", &x, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_CENTER, -100, -240, 10, 100, -300, 300, 0);
    tt_slider_t *ySlider = sliderInit("", &y, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_CENTER, -100, -260, 10, 100, -300, 300, 0);
    tt_slider_t *zSlider = sliderInit("", &z, TT_SLIDER_HORIZONTAL, TT_SLIDER_ALIGN_CENTER, -100, -280, 10, 100, -300, 300, 0);
    switchInit("", &xEnabled, -10, -240, 10);
    switchInit("", &yEnabled, -10, -260, 10);
    switchInit("", &zEnabled, -10, -280, 10);

    list_t *xPositions = list_init();
    list_t *yPositions = list_init();
    for (uint32_t i = 0; i < tt_elements.all -> length; i++) {
        list_append(xPositions, (unitype) ((tt_button_t *) tt_elements.all -> data[i].p) -> x, 'd');
        list_append(yPositions, (unitype) ((tt_button_t *) tt_elements.all -> data[i].p) -> y, 'd');
    }

    double scroll = 0.0;
    double scrollFactor = 15;
    while (turtle.shouldClose == 0) {
        start = clock();
        turtleGetMouseCoords();
        turtleClear();
        tt_setColor(TT_COLOR_TEXT);
        turtleTextWriteStringf(-310, -170, 5, 0, "%.2lf, %.2lf", turtle.mouseX, turtle.mouseY);
        turtleTextWriteString("X", xSlider -> x - xSlider -> length / 2 - xSlider -> size, xSlider -> y, xSlider -> size - 1, 100);
        turtleTextWriteStringf(ySlider -> x + xSlider -> length / 2 + xSlider -> size, xSlider -> y, 4, 0, "%.01lf", round(x) / 10);
        turtleTextWriteString("Y", xSlider -> x - ySlider -> length / 2 - xSlider -> size, ySlider -> y, xSlider -> size - 1, 100);
        turtleTextWriteStringf(ySlider -> x + ySlider -> length / 2 + xSlider -> size, ySlider -> y, 4, 0, "%.01lf", round(y) / 10);
        turtleTextWriteString("Z", zSlider -> x - zSlider -> length / 2 - xSlider -> size, zSlider -> y, xSlider -> size - 1, 100);
        turtleTextWriteStringf(zSlider -> x + zSlider -> length / 2 + xSlider -> size, zSlider -> y, 4, 0, "%.01lf", round(z) / 10);

        for (uint32_t i = 0; i < tt_elements.all -> length; i++) {
            if (((tt_button_t *) tt_elements.all -> data[i].p) -> element != TT_ELEMENT_SCROLLBAR) {
                ((tt_button_t *) tt_elements.all -> data[i].p) -> x = xPositions -> data[i].d - scrollbarVarX * 5;
                ((tt_button_t *) tt_elements.all -> data[i].p) -> y = yPositions -> data[i].d + scrollbarVarY * 3.3;
            }
        }
        scroll = turtleMouseWheel();
        if (scroll != 0) {
            if (turtleKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
                scrollbarVarX -= scroll * scrollFactor;
                if (scrollbarVarX < 0) {
                    scrollbarVarX = 0;
                }
                if (scrollbarVarX > 100) {
                    scrollbarVarX = 100;
                }
            } else {
                scrollbarVarY -= scroll * scrollFactor;
                if (scrollbarVarY < 0) {
                    scrollbarVarY = 0;
                }
                if (scrollbarVarY > 100) {
                    scrollbarVarY = 100;
                }
            }
        }
        turtleToolsUpdate(); // update turtleTools
        parseRibbonOutput(); // user defined function to use ribbon
        if (turtle.close == 1) {
            turtle.shouldClose = 1;
        }
        turtleUpdate(); // update the screen
        end = clock();
        while ((double) (end - start) / CLOCKS_PER_SEC < (1.0 / tps)) {
            end = clock();
        }
        tick++;
    }
    turtleFree();
    glfwTerminate();
    return 0;
}