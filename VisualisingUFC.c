#include "include/turtleTools.h"
#include "include/osTools.h"
#include <time.h>

/* up to 8 sources graphed at once */
#define NUMBER_OF_GRAPH_SOURCES 8

typedef struct {
    int32_t index;
    int32_t field;
    int8_t editable;
    double screenX;
    double screenY;
    double scaleX;
    double scaleY;
    double anchorX;
    double anchorY;
    double anchorScreenX;
    double anchorScreenY;
} graph_source_t;

typedef struct {
    uint32_t packetType; // index in packet list
    uint32_t index; // index in list of packet list - packets[packetType][index]
} selected_t;

typedef union {
    uint64_t selectedPacked;
    selected_t selected;
} selected_union_t;

typedef struct {
    char alreadyExported[4096]; // export file name
    list_t *dataFiles; // list of filenames
    list_t *packetDefinitions;
    list_t *packets; // list of packets (packets -> data[packetType].r -> data[index].r -> data[field]) - hijacked start delimeter for other uses
    list_t *selected; // list of selected packets
    int8_t **directory; // array of rendered fields
    graph_source_t graph[NUMBER_OF_GRAPH_SOURCES];

    /* mouse */
    double mx; // mouseX
    double my; // mouseY
    double mw; // mouseWheel
    uint8_t keys[12];
    uint8_t dragging;
    uint8_t selecting;
    double anchorSelectX;
    double anchorSelectY;
    double selectCoords[4]; // X min, Y min, X max, Y max
    double scrollFactor;
    double scrollFactorCtrl;
    double scrollFactorArrow;

    /* UI */
    double bottomBoxHeight;
    double dataColors[90];
    double selectedColor[3];
    double unselectedColor[3];
    int32_t hoverIndex;
    int32_t hoverField;
    int32_t hoverSource;
    int32_t uiPacketSelected;
    list_t *fieldSwitches;
    list_t *sourceSwitches;
    tt_button_t *matchScaleAndOffsetButton;
    tt_button_t *zeroTimestampButton;
    tt_button_t *syncTimestampButton;
    int8_t matchScaleAndOffset;
    int8_t zeroTimestamp;
    int8_t syncTimestamp;
    int8_t renderByTimestamp; // renderByTimestamp or renderByIndex
} visualising_ufc_t;

visualising_ufc_t self;

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

int32_t matchPacket(int32_t packetIndex, uint8_t *file, uint32_t maxLength) {
    list_t *packetType = self.packetDefinitions -> data[packetIndex].r;
    if (maxLength <= packetType -> data[1].i) {
        return 0;
    }
    uint32_t packetField = 4;
    for (uint32_t i = 0; i < packetType -> data[1].i; i++) {
        if (packetField >= packetType -> length) {
            return 0;
        }
        /* we make use of the assumption that big endian machines don't exist - and it's 2025 so they really don't */
        switch (packetType -> data[packetField].i) {
            case UFC_PACKET_FIELD_DELIMETER_UINT64:
            if (packetType -> data[packetField + 1].l != *((uint64_t *) (file + i))) {
                return 0;
            }
            i += 7;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT32:
            // printf("%X ", *((uint32_t *) (file + i)));
            if (packetType -> data[packetField + 1].u != *((uint32_t *) (file + i))) {
                return 0;
            }
            i += 3;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT16:
            if (packetType -> data[packetField + 1].h != *((uint16_t *) (file + i))) {
                return 0;
            }
            i += 1;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT8:
            // printf("%X %X\n", packetType -> data[packetField + 1].b, file[i]);
            if (packetType -> data[packetField + 1].b != file[i]) {
                return 0;
            }
            break;
            case UFC_PACKET_FIELD_DOUBLE:
            case UFC_PACKET_FIELD_UINT64:
            case UFC_PACKET_FIELD_INT64:
            i += 7;
            break;
            case UFC_PACKET_FIELD_FLOAT:
            case UFC_PACKET_FIELD_UINT32:
            case UFC_PACKET_FIELD_INT32:
            i += 3;
            break;
            case UFC_PACKET_FIELD_UINT16:
            case UFC_PACKET_FIELD_INT16:
            i += 1;
            break;
            default:
            break;
        }
        packetField += 2;
    }
    /* packet found */
    list_t *packet = list_init();
    packetField = 4;
    for (uint32_t i = 0; i < packetType -> data[1].i; i++) {
        switch (packetType -> data[packetField].i) {
            case UFC_PACKET_FIELD_DELIMETER_UINT64:
            list_append(packet, (unitype) *((uint64_t *) (file + i)), 'l');
            i += 7;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT32:
            list_append(packet, (unitype) *((uint32_t *) (file + i)), 'u');
            i += 3;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT16:
            list_append(packet, (unitype) *((uint16_t *) (file + i)), 'h');
            i += 1;
            break;
            case UFC_PACKET_FIELD_DELIMETER_UINT8:
            list_append(packet, (unitype) *((uint8_t *) (file + i)), 'b');
            break;
            case UFC_PACKET_FIELD_DOUBLE:
            list_append(packet, (unitype) *((double *) (file + i)), 'd');
            i += 7;
            break;
            case UFC_PACKET_FIELD_UINT64:
            list_append(packet, (unitype) *((uint64_t *) (file + i)), 'l');
            i += 7;
            break;
            case UFC_PACKET_FIELD_INT64:
            list_append(packet, (unitype) *((uint64_t *) (file + i)), 'l');
            i += 7;
            break;
            case UFC_PACKET_FIELD_FLOAT:
            list_append(packet, (unitype) *((float *) (file + i)), 'f');
            i += 3;
            break;
            case UFC_PACKET_FIELD_UINT32:
            list_append(packet, (unitype) *((uint32_t *) (file + i)), 'u');
            i += 3;
            break;
            case UFC_PACKET_FIELD_INT32:
            list_append(packet, (unitype) *((int32_t *) (file + i)), 'i');
            i += 3;
            break;
            case UFC_PACKET_FIELD_UINT16:
            list_append(packet, (unitype) *((uint16_t *) (file + i)), 'h');
            i += 1;
            break;
            case UFC_PACKET_FIELD_INT16:
            list_append(packet, (unitype) *((uint16_t *) (file + i)), 'h');
            i += 1;
            break;
            case UFC_PACKET_FIELD_UINT8:
            list_append(packet, (unitype) *((uint8_t *) (file + i)), 'b');
            break;
            case UFC_PACKET_FIELD_INT8:
            list_append(packet, (unitype) *((uint8_t *) (file + i)), 'b');
            break;
            default:
            break;
        }
        packetField += 2;
    }
    if (packet -> length > 0) {
        packet -> data[0].u &= 0xFFFFFFFE;
    }
    list_append(self.packets -> data[packetIndex].r, (unitype) packet, 'r');
    return 1;
}

void import(char *filename) {
    if (filename[strlen(filename) - 3] == 't' && filename[strlen(filename) - 2] == 'x' && filename[strlen(filename) - 1] == 't') {
        /* txt file - memory map file */
        uint32_t fileSize = 0;
        uint8_t *fileData = mapFile(filename, &fileSize);
        if (fileData == NULL) {
            return;
        }
        for (uint32_t i = 0; i < fileSize; i++) {
            for (uint32_t j = 0; j < self.packetDefinitions -> length; j++) {
                matchPacket(j, fileData + i, fileSize - i);
            }
        }
        unmapFile(fileData);
    } else if (filename[strlen(filename) - 3] == 'c' && filename[strlen(filename) - 2] == 's' && filename[strlen(filename) - 1] == 'v') {
        /* csv file - memory map file */
        uint32_t fileSize = 0;
        uint8_t *fileData = mapFile(filename, &fileSize);
        if (fileData == NULL) {
            return;
        }
        /* gather headers */
        uint32_t headersCheck = 0;
        uint32_t headersIndex = 0;
        list_t *headers = list_init();
        while (headersCheck < fileSize / 2) { // assume headers take up no more than half of the filesize
            if (fileData[headersCheck] == ',') {
                fileData[headersCheck] = '\0';
                list_append(headers, (unitype) (char *) (fileData + headersIndex), 's');
                fileData[headersCheck] = ',';
                headersIndex = headersCheck + 1;
                while (fileData[headersIndex] == ' ') {
                    headersCheck++;
                    headersIndex++;
                }
            }
            if (fileData[headersCheck] == '\n' || fileData[headersCheck] == '\r') {
                char tempHold = fileData[headersCheck];
                fileData[headersCheck] = '\0';
                list_append(headers, (unitype) (char *) (fileData + headersIndex), 's');
                fileData[headersCheck] = tempHold;
                break;
            }
            headersCheck++;
        }
        while (fileData[headersCheck] == '\r' || fileData[headersCheck] == '\n') {
            headersCheck++;
        }
        uint32_t dataIndex = headersCheck;
        uint32_t dataCheck = headersCheck;
        /* create packet */
        list_t *newPacket = list_init();
        list_append(newPacket, (unitype) "newPacket", 's');
        list_append(newPacket, (unitype) (uint32_t) (1 * sizeof(int64_t) + (headers -> length - 1) * sizeof(double)), 'i');
        list_append(newPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
        list_append(newPacket, (unitype) 0, 'i'); // reserved for selected
        list_append(newPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i'); // dummy start delimeter
        list_append(newPacket, (unitype) -1, 'u');
        for (uint32_t i = 0; i < headers -> length; i++) {
            if (i == 0) {
                list_append(newPacket, (unitype) UFC_PACKET_FIELD_INT64, 'i');
                list_append(newPacket, headers -> data[i], 's');
            } else {
                list_append(newPacket, (unitype) UFC_PACKET_FIELD_DOUBLE, 'i');
                list_append(newPacket, headers -> data[i], 's');
            }
        }
        list_append(newPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i'); // dummy end delimeter
        list_append(newPacket, (unitype) -1, 'u');
        list_append(self.packetDefinitions, (unitype) newPacket, 'r');
        list_append(self.packets, (unitype) list_init(), 'r');
        self.directory = malloc(self.packetDefinitions -> length * sizeof(int8_t *)); // me when memory leak
        for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
            self.directory[i] = calloc(self.packetDefinitions -> data[i].r -> length / 2 - 2, 1);
        }
        self.bottomBoxHeight = self.packetDefinitions -> length * 12 + 10;
        list_print(newPacket);
        /* gather data */
        int32_t packetIndex = self.packets -> length - 1;
        int32_t column = 0;
        while (dataCheck < fileSize) {
            // printf("%d %d\n", dataIndex, dataCheck);
            if (fileData[dataCheck] == ',') {
                fileData[dataCheck] = '\0';
                if (column == 0) {
                    list_append(self.packets -> data[packetIndex].r, (unitype) list_init(), 'r');
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) 0, 'l'); // dummy delimeter
                    double timestamp = 0;
                    sscanf((char *) (fileData + dataIndex), "%lf", &timestamp);
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) (uint64_t) (int64_t) (timestamp * 1000), 'l');
                } else {
                    double field;
                    sscanf((char *) (fileData + dataIndex), "%lf", &field);
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) field, 'd');
                }
                fileData[dataCheck] = ',';
                dataIndex = dataCheck + 1;
                while (fileData[dataIndex] == ' ') {
                    dataCheck++;
                    dataIndex++;
                }
                column++;
            }
            if (fileData[dataCheck] == '\n' || fileData[dataCheck] == '\r') {
                char tempHold = fileData[dataCheck];
                fileData[dataCheck] = '\0';
                if (column == 0) {
                    list_append(self.packets -> data[packetIndex].r, (unitype) list_init(), 'r');
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) 0, 'l'); // dummy delimeter
                    double timestamp = 0;
                    sscanf((char *) (fileData + dataIndex), "%lf", &timestamp);
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) (uint64_t) (int64_t) (timestamp * 1000), 'l');
                } else {
                    double field;
                    sscanf((char *) (fileData + dataIndex), "%lf", &field);
                    list_append(self.packets -> data[packetIndex].r -> data[self.packets -> data[packetIndex].r -> length - 1].r, (unitype) field, 'd');
                }
                fileData[dataCheck] = tempHold;
                while (fileData[dataCheck] == '\r' || fileData[dataCheck] == '\n') {
                    dataCheck++;
                }
                dataIndex = dataCheck;
                column = 0;
            }
            dataCheck++;
        }
        unmapFile(fileData);
    }
}

void export(char *filename) {
    strcpy(self.alreadyExported, filename);
    /* gather packets in order of quantity */
    list_t *lengths = list_init();
    list_t *lengthsCreate = list_init();
    for (int32_t i = 0; i < self.packets -> length; i++) {
        list_append(lengthsCreate, (unitype) (self.packets -> data[i].r -> length), 'i');
    }
    list_sort(lengthsCreate);
    for (int32_t i = 0; i < self.packets -> length; i++) {
        for (int32_t j = 0; j < self.packets -> length; j++) {
            if (self.packets -> data[j].r -> length == lengthsCreate -> data[i].i && list_count(lengths, (unitype) j, 'i') == 0) {
                list_append(lengths, (unitype) j, 'i');
                break;
            }
        }
    }
    list_free(lengthsCreate);
    FILE *fp = fopen(filename, "w");
    /* write headers */
    list_t *writeIndices = list_init();
    for (int32_t i = 0; i < self.packetDefinitions -> length; i++) {
        list_append(writeIndices, (unitype) list_init(), 'r');
        for (int32_t j = 5; j < self.packetDefinitions -> data[i].r -> length; j += 2) {
            if (self.packetDefinitions -> data[i].r -> type[j] == 's' && strcmp(self.packetDefinitions -> data[i].r -> data[j].s, "pkt_len") != 0) {
                list_append(writeIndices -> data[i].r, (unitype) (j / 2 - 2), 'i');
            }
        }
    }
    for (int32_t i = 0; i < self.packets -> length; i++) {
        if (self.packets -> data[lengths -> data[i].i].r -> length > 0) {
            for (int32_t j = 0; j < writeIndices -> data[lengths -> data[i].i].r -> length; j++) {
                if ((i == self.packets -> length - 1 || 0 >= self.packets -> data[lengths -> data[i + 1].i].r -> length) && j == writeIndices -> data[lengths -> data[i].i].r -> length - 1) {
                    fprintf(fp, "%s\n", self.packetDefinitions -> data[lengths -> data[i].i].r -> data[writeIndices -> data[lengths -> data[i].i].r -> data[j].i * 2 + 5].s);
                } else {
                    fprintf(fp, "%s, ", self.packetDefinitions -> data[lengths -> data[i].i].r -> data[writeIndices -> data[lengths -> data[i].i].r -> data[j].i * 2 + 5].s);
                }
            }
        }
    }
    for (int32_t iter = 0; iter < self.packets -> data[lengths -> data[0].i].r -> length; iter++) {
        for (int32_t i = 0; i < self.packets -> length; i++) {
            if (self.packets -> data[lengths -> data[i].i].r -> length > iter) {
                for (int32_t j = 0; j < writeIndices -> data[lengths -> data[i].i].r -> length; j++) {
                    // list_print(self.packets -> data[lengths -> data[i].i].r -> data[iter].r);
                    unitype_fprint(fp, self.packets -> data[lengths -> data[i].i].r -> data[iter].r -> data[writeIndices -> data[lengths -> data[i].i].r -> data[j].i], self.packets -> data[lengths -> data[i].i].r -> data[iter].r -> type[writeIndices -> data[lengths -> data[i].i].r -> data[j].i]);
                    if ((i == self.packets -> length - 1 || iter >= self.packets -> data[lengths -> data[i + 1].i].r -> length) && j == writeIndices -> data[lengths -> data[i].i].r -> length - 1) {
                        fprintf(fp, "\n");
                    } else {
                        fprintf(fp, ",");
                    }
                }
            }
        }
    }
    fclose(fp);
    list_free(lengths);
    list_free(writeIndices);
}

void init() {
    /* we assume that big endian machines do not exist */
    strcpy(self.alreadyExported, "null");
    self.dataFiles = list_init();
    self.packetDefinitions = list_init();
    self.packets = list_init();
    self.selected = list_init();

    /* Status packet */
    list_t *statusPacket = list_init();
    list_append(statusPacket, (unitype) "statusPacket", 's');
    list_append(statusPacket, (unitype) 24, 'i'); // status packet is 24 bytes big
    list_append(statusPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(statusPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(statusPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(statusPacket, (unitype) "timestamp", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 1 for status packet
    list_append(statusPacket, (unitype) 1, 'b');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(statusPacket, (unitype) "state", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(statusPacket, (unitype) "pkt_len", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(statusPacket, (unitype) "card_id", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(statusPacket, (unitype) "card_type", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(statusPacket, (unitype) "card_redirect", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(statusPacket, (unitype) "card_status", 's');
    list_append(statusPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(statusPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) statusPacket, 'r');

    /* Alt packet */
    list_t *altPacket = list_init();
    list_append(altPacket, (unitype) "altPacket", 's');
    list_append(altPacket, (unitype) 24, 'i'); // alt packet is 24 bytes big
    list_append(altPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(altPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(altPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(altPacket, (unitype) "timestamp", 's');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 8 for alt packet
    list_append(altPacket, (unitype) 8, 'b');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(altPacket, (unitype) "state", 's');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(altPacket, (unitype) "pkt_len", 's');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(altPacket, (unitype) "temperature", 's');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(altPacket, (unitype) "pressure", 's');
    list_append(altPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(altPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) altPacket, 'r');

    /* BNO packet */
    list_t *bnoPacket = list_init();
    list_append(bnoPacket, (unitype) "bnoPacket", 's');
    list_append(bnoPacket, (unitype) 52, 'i'); // bno packet is 52 bytes big
    list_append(bnoPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(bnoPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(bnoPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(bnoPacket, (unitype) "timestamp", 's');
    list_append(bnoPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 4 for bno packet
    list_append(bnoPacket, (unitype) 4, 'b');
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

    /* GPS packet */
    list_t *gpsPacket = list_init();
    list_append(gpsPacket, (unitype) "gpsPacket", 's');
    list_append(gpsPacket, (unitype) 80, 'i'); // gps packet is 80 bytes big
    list_append(gpsPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(gpsPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(gpsPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "timestamp", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 16 for gps packet
    list_append(gpsPacket, (unitype) 16, 'b');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "state", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(gpsPacket, (unitype) "pkt_len", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "time_of_week", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "time_hour", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "time_min", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "time_sec", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) -1, 'u');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_INT32, 'i');
    list_append(gpsPacket, (unitype) "time_nanosec", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "time_accuracy", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(gpsPacket, (unitype) "pos_lat", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(gpsPacket, (unitype) "pos_lon", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "height_msl", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "height_elip", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "fix_type", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "num_satellites", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(gpsPacket, (unitype) -1, 'u');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "vertical_accuracy", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "horizontal_accuracy", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(gpsPacket, (unitype) "p_DOP", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_INT32, 'i');
    list_append(gpsPacket, (unitype) "vel_north", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_INT32, 'i');
    list_append(gpsPacket, (unitype) "vel_east", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_INT32, 'i');
    list_append(gpsPacket, (unitype) "vel_down", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(gpsPacket, (unitype) "vel_accuracy", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(gpsPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) gpsPacket, 'r');

    /* Sensor packet */
    list_t *sensorPacket = list_init();
    list_append(sensorPacket, (unitype) "sensorPacket", 's');
    list_append(sensorPacket, (unitype) 64, 'i'); // sensor packet is 64 bytes big
    list_append(sensorPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(sensorPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(sensorPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(sensorPacket, (unitype) "timestamp", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 2 for sensor packet
    list_append(sensorPacket, (unitype) 2, 'b');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(sensorPacket, (unitype) "state", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(sensorPacket, (unitype) "pkt_len", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "low_accel_x", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "low_accel_y", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "low_accel_z", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "high_accel_x", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "high_accel_y", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "high_accel_z", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "mag_x", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "mag_y", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "mag_z", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "gyro_x", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "gyro_y", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(sensorPacket, (unitype) "gyro_z", 's');
    list_append(sensorPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(sensorPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) sensorPacket, 'r');

    /* Pitot center packet */
    list_t *pitotCenterPacket = list_init();
    list_append(pitotCenterPacket, (unitype) "pitotCenterPacket", 's');
    list_append(pitotCenterPacket, (unitype) 24, 'i'); // pitot center packet is 24 bytes big
    list_append(pitotCenterPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(pitotCenterPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(pitotCenterPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(pitotCenterPacket, (unitype) "timestamp", 's');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 32 for pitot center packet
    list_append(pitotCenterPacket, (unitype) 32, 'b');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(pitotCenterPacket, (unitype) "state", 's');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(pitotCenterPacket, (unitype) "pkt_len", 's');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotCenterPacket, (unitype) "center_port", 's');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotCenterPacket, (unitype) "static_port", 's');
    list_append(pitotCenterPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(pitotCenterPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) pitotCenterPacket, 'r');

    /* Pitot radial packet */
    list_t *pitotRadialPacket = list_init();
    list_append(pitotRadialPacket, (unitype) "pitotRadialPacket", 's');
    list_append(pitotRadialPacket, (unitype) 32, 'i'); // pitot radial packet is 32 bytes big
    list_append(pitotRadialPacket, (unitype) 0, 'i'); // reserved for index of dataFile source
    list_append(pitotRadialPacket, (unitype) 0, 'i'); // reserved for selected
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(pitotRadialPacket, (unitype) 0xBA5EBA11, 'u');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_UINT32, 'i');
    list_append(pitotRadialPacket, (unitype) "timestamp", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT8, 'i'); // pkt_type always 64 for pitot radial packet
    list_append(pitotRadialPacket, (unitype) 64, 'b');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(pitotRadialPacket, (unitype) "state", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(pitotRadialPacket, (unitype) "pkt_len", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotRadialPacket, (unitype) "up_port", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotRadialPacket, (unitype) "down_port", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotRadialPacket, (unitype) "left_port", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_FLOAT, 'i');
    list_append(pitotRadialPacket, (unitype) "right_port", 's');
    list_append(pitotRadialPacket, (unitype) UFC_PACKET_FIELD_DELIMETER_UINT32, 'i');
    list_append(pitotRadialPacket, (unitype) 0xCA11AB1E, 'u');
    list_append(self.packetDefinitions, (unitype) pitotRadialPacket, 'r');

    self.directory = malloc(self.packetDefinitions -> length * sizeof(int8_t *));
    for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
        self.directory[i] = calloc(self.packetDefinitions -> data[i].r -> length / 2 - 2, 1);
        list_append(self.packets, (unitype) list_init(), 'r');
    }

    /* general */
    self.mx = 0;
    self.my = 0;
    self.mw = 0;
    self.bottomBoxHeight = self.packetDefinitions -> length * 12 + 10;
    self.scrollFactor = 1.15;
    self.scrollFactorCtrl = 2.5;
    self.scrollFactorArrow = 1.001;
    self.dragging = 0;
    self.selecting = 0;
    for (uint32_t i = 0; i < sizeof(self.keys); i++) {
        self.keys[i] = 0;
    }

    self.hoverIndex = -1;
    self.hoverField = -1;
    self.hoverSource = -1;
    self.uiPacketSelected = -1;
    self.fieldSwitches = list_init();
    self.sourceSwitches = list_init();
    self.matchScaleAndOffset = 0;
    self.zeroTimestamp = 0;
    self.syncTimestamp = 0;
    const double buttonXpos = 240;
    self.matchScaleAndOffsetButton = buttonInit("Match Scale and Offset", &self.matchScaleAndOffset, buttonXpos, -180 + self.bottomBoxHeight - 20, 8);
    self.zeroTimestampButton = buttonInit("Zero Timestamp", &self.zeroTimestamp, buttonXpos + 21, -180 + self.bottomBoxHeight - 35, 8);
    self.syncTimestampButton = buttonInit("Sync Timestamp", &self.syncTimestamp, buttonXpos + 21, -180 + self.bottomBoxHeight - 50, 8);
    self.renderByTimestamp = 1;
    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
        self.graph[i].index = -1;
        self.graph[i].field = -1;
        self.graph[i].editable = 0;
        self.graph[i].screenX = 0;
        self.graph[i].screenY = 0;
        self.graph[i].scaleX = 1;
        self.graph[i].scaleY = 1;
        self.graph[i].anchorX = 0;
        self.graph[i].anchorY = 0;
        self.graph[i].anchorScreenX = 0;
        self.graph[i].anchorScreenY = 0;
    }

    double dataColorCopy[] = {
        19, 236, 48, // data color (channel 1)
        0, 221, 255, // data color (channel 2)
        241, 241, 241, // data color (channel 3)
        232, 15, 136, // data color (channel 4)
        255, 219, 0, // data color (channel 5)
        28, 104, 137, // data color (channel 6)
        255, 106, 0, // data color (channel 7)
        244, 11, 45, // data color (channel 8)
    };
    memcpy(self.dataColors, dataColorCopy, sizeof(dataColorCopy));
    self.unselectedColor[0] = 100;
    self.unselectedColor[1] = 100;
    self.unselectedColor[2] = 100;

    self.selectedColor[0] = 140;
    self.selectedColor[1] = 140;
    self.selectedColor[2] = 140;
}

double convertToDouble(unitype value, int32_t type) {
    switch (type) {
        case UFC_PACKET_FIELD_DELIMETER_UINT64:
        return (double) value.l;
        case UFC_PACKET_FIELD_DELIMETER_UINT32:
        return (double) value.u;
        case UFC_PACKET_FIELD_DELIMETER_UINT16:
        return (double) value.h;
        case UFC_PACKET_FIELD_DELIMETER_UINT8:
        return (double) value.b;
        case UFC_PACKET_FIELD_DOUBLE:
        return value.d;
        case UFC_PACKET_FIELD_UINT64:
        return (double) value.l;
        case UFC_PACKET_FIELD_INT64:
        return (double) (int64_t) value.l;
        case UFC_PACKET_FIELD_FLOAT:
        return (double) value.f;
        case UFC_PACKET_FIELD_UINT32:
        return (double) value.u;
        case UFC_PACKET_FIELD_INT32:
        return (double) value.i;
        case UFC_PACKET_FIELD_UINT16:
        return (double) value.h;
        case UFC_PACKET_FIELD_INT16:
        return (double) (int16_t) value.h;
        case UFC_PACKET_FIELD_UINT8:
        return (double) value.b;
        case UFC_PACKET_FIELD_INT8:
        return (double) (int8_t) value.b;
        default:
        return 0.0;
        break;
    }
}

void renderInfo() {
    /* draw box */
    tt_setColor(TT_COLOR_POPUP_BOX);
    turtle.pena = 0.2; // 20% opacity
    turtleRectangle(-320, -180, 320, -180 + self.bottomBoxHeight);

    /* reset hover */
    self.hoverIndex = -1;
    self.hoverField = -1;
    self.hoverSource = -1;

    if (self.uiPacketSelected == -1) {
        /* draw packets */
        for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
            if (self.directory[i][0]) {
                tt_setColor(TT_COLOR_TEXT_ALTERNATE);
                if (turtle.penr + turtle.peng + turtle.penb > 1) {
                    turtle.penr -= 40.0 / 255;
                    turtle.peng -= 40.0 / 255;
                    turtle.penb -= 40.0 / 255;
                } else {
                    turtle.penr += 40.0 / 255;
                    turtle.peng += 40.0 / 255;
                    turtle.penb += 40.0 / 255;
                }
            } else {
                tt_setColor(TT_COLOR_RIBBON_TOP);
            }
            double yPos = -180 + self.bottomBoxHeight - (i + 1) * 12;
            if (self.my > yPos - 6 && self.my < yPos + 6 && self.mx > -310 && self.mx < -302 + turtleTextGetStringLengthf(8, "Total %ss: %d", self.packetDefinitions -> data[i].r -> data[0].s, self.packets -> data[i].r -> length)) {
                tt_setColor(TT_COLOR_TEXT_ALTERNATE);
                self.hoverIndex = i;
            }
            turtleTextWriteStringf(-306, yPos, 8, 0, "Total %ss: %d", self.packetDefinitions -> data[i].r -> data[0].s, self.packets -> data[i].r -> length);
        }
        /* draw sources */
        uint8_t populateSwitches = 0;
        if (self.sourceSwitches -> length == 0) {
            populateSwitches = 1;
        }
        double xPos = 0;
        double yPos = -180 + self.bottomBoxHeight - 20;
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index == -1) {
                continue;
            }
            turtlePenColor(self.dataColors[i * 3], self.dataColors[i * 3 + 1], self.dataColors[i * 3 + 2]);
            if (self.graph[i].editable == 0) {
                turtle.pena = 0.5; // 50% opacity
            }
            if (self.my > yPos - 3 && self.my < yPos + 3 && self.mx > xPos - 10 && self.mx < xPos + 2 + turtleTextGetStringLength(self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s, 6)) {
                if (turtle.pena > 0) {
                    turtle.pena = 0;
                } else {
                    turtle.pena = 0.5;
                }
                self.hoverSource = i;
            }
            turtleTextWriteString(self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s, xPos, yPos, 6, 0);
            if (populateSwitches) {
                list_append(self.sourceSwitches, (unitype) (void *) switchInit("", &(self.graph[i].editable), xPos - 6, yPos, 5), 'l'); // avoid double freeing
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> color.colorOverride = 1;
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> color.color[9] = self.dataColors[i * 3];
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> color.color[10] = self.dataColors[i * 3 + 1];
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> color.color[11] = self.dataColors[i * 3 + 2];
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> style = TT_SWITCH_STYLE_XBOX;
                ((tt_switch_t *) (self.sourceSwitches -> data[self.sourceSwitches -> length - 1].p)) -> enabled = TT_ELEMENT_NO_MOUSE;
            }
            yPos -= 8;
            if (yPos < -170) {
                yPos = -180 + self.bottomBoxHeight - 20;
                xPos += 80;
            }
        }
    } else {
        /* draw fields */
        uint8_t populateSwitches = 0;
        if (self.fieldSwitches -> length == 0) {
            populateSwitches = 1;
        }
        tt_setColor(TT_COLOR_TEXT_ALTERNATE);
        turtleTextWriteStringf(0, -180 + self.bottomBoxHeight - 10, 8, 50, "Total %ss: %d", self.packetDefinitions -> data[self.uiPacketSelected].r -> data[0].s, self.packets -> data[self.uiPacketSelected].r -> length);
        double yPos = -180 + self.bottomBoxHeight - 20;
        double xPos = -290;
        for (uint32_t i = 2; i < self.packetDefinitions -> data[self.uiPacketSelected].r -> length; i += 2) {
            if (self.packetDefinitions -> data[self.uiPacketSelected].r -> type[i + 1] == 's') {
                tt_setColor(TT_COLOR_RIBBON_TOP);
                if (self.my > yPos - 4 && self.my < yPos + 4 && self.mx > xPos - 17 && self.mx < xPos + 4 + turtleTextGetStringLength(self.packetDefinitions -> data[self.uiPacketSelected].r -> data[i + 1].s, 8)) {
                    tt_setColor(TT_COLOR_TEXT_ALTERNATE);
                    self.hoverField = i;
                }
                if (self.directory[self.uiPacketSelected][i / 2 - 2]) {
                    tt_setColor(TT_COLOR_TEXT_ALTERNATE);
                }
                turtleTextWriteString(self.packetDefinitions -> data[self.uiPacketSelected].r -> data[i + 1].s, xPos, yPos, 5, 0);
                if (populateSwitches) {
                    list_append(self.fieldSwitches, (unitype) (void *) switchInit("", &(self.directory[self.uiPacketSelected][i / 2 - 2]), xPos - 10, yPos, 5), 'l'); // avoid double freeing
                    ((tt_switch_t *) (self.fieldSwitches -> data[self.fieldSwitches -> length - 1].p)) -> enabled = TT_ELEMENT_NO_MOUSE;
                }
                yPos -= 8;
                if (yPos < -170) {
                    yPos = -180 + self.bottomBoxHeight - 20;
                    xPos += 80;
                }
            }
        }
        /* draw arrow */
        double arrowX = -310;
        double arrowY = -180 + self.bottomBoxHeight - 8;
        if (self.my > arrowY - 5 && self.my < arrowY + 5 && self.mx > arrowX - 5 && self.mx < arrowX + 5) {
            tt_setColor(TT_COLOR_TEXT_ALTERNATE);
            self.hoverField = -2;
        } else {
            tt_setColor(TT_COLOR_RIBBON_TOP);
        }
        turtleTriangle(arrowX - 3, arrowY, arrowX + 3, arrowY + 4, arrowX + 3, arrowY - 4);
    }
}

void setGraphScale(uint32_t graphIndex) {
    if (self.graph[graphIndex].index == -1) {
        return;
    }
    list_t *dataList = self.packets -> data[self.graph[graphIndex].index].r;
    if (dataList -> length == 0) {
        self.graph[graphIndex].screenX = 0;
        self.graph[graphIndex].screenY = 0;
        self.graph[graphIndex].scaleX = 1;
        self.graph[graphIndex].scaleY = 1;
        return;
    }
    double minValue = 100000000000000000.0; // FLOAT_MAX
    double maxValue = -100000000000000000.0;
    int32_t maxTimestamp = -2147483648;
    int32_t minTimestamp = 2147483647;
    uint32_t stride = 0;
    if (self.renderByTimestamp) {
        stride = dataList -> length / (self.graph[graphIndex].scaleX * 1000000000);
    } else {
        stride = dataList -> length / (self.graph[graphIndex].scaleX * 50000000);
    }
    if (stride < 1) {
        stride = 1;
    }
    int32_t index = 0;
    while (index < dataList -> length) {
        double value = convertToDouble(dataList -> data[index].r -> data[self.graph[graphIndex].field], self.packetDefinitions -> data[self.graph[graphIndex].index].r -> data[self.graph[graphIndex].field * 2 + 4].i);
        if (value > maxValue) {
            maxValue = value;
        }
        if (value < minValue) {
            minValue = value;
        }
        if (dataList -> data[index].r -> data[1].i > maxTimestamp) {
            maxTimestamp = dataList -> data[index].r -> data[1].i;
        }
        if (dataList -> data[index].r -> data[1].i < minTimestamp) {
            minTimestamp = dataList -> data[index].r -> data[1].i;
        }
        index += stride;
    }
    /* check for existing graphs for X scale */
    int32_t subPriority = -1;
    int32_t topPriority = -1;
    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
        if (i == graphIndex) {
            continue;
        }
        if (self.graph[i].index != -1) {
            subPriority = i;
            if (self.graph[i].index == self.graph[graphIndex].index) {
                topPriority = i;
            }
        }
    }
    if (self.renderByTimestamp) {
        self.graph[graphIndex].scaleX = 620.0 / (maxTimestamp - minTimestamp);
        self.graph[graphIndex].screenX = -310 + -minTimestamp * self.graph[graphIndex].scaleX;
    } else {
        self.graph[graphIndex].scaleX = 620.0 / dataList -> length;
        self.graph[graphIndex].screenX = -310;
    }
    if (subPriority != -1) {
        if (self.renderByTimestamp) {
            self.graph[graphIndex].scaleX = self.graph[subPriority].scaleX;
            self.graph[graphIndex].screenX = self.graph[subPriority].screenX;
        } else {
            double packetNumberRatio = (double) self.packets -> data[self.graph[subPriority].index].r -> length / self.packets -> data[self.graph[graphIndex].index].r -> length;
            self.graph[graphIndex].scaleX = self.graph[subPriority].scaleX * packetNumberRatio;
            self.graph[graphIndex].screenX = self.graph[subPriority].screenX;
        }
    }
    if (topPriority != -1) {
        self.graph[graphIndex].scaleX = self.graph[topPriority].scaleX;
        self.graph[graphIndex].screenX = self.graph[topPriority].screenX;
    }
    if (fabs(minValue - maxValue) < 0.001) {
        self.graph[graphIndex].scaleY = 1;
    } else {
        self.graph[graphIndex].scaleY = (320 - self.bottomBoxHeight) / (maxValue - minValue);
    }
    self.graph[graphIndex].screenY = ((maxValue + minValue) / 2) * -self.graph[graphIndex].scaleY + self.bottomBoxHeight / 2;
    // printf("%lf %lf %lf %lf\n", self.scaleX[graphIndex], self.scaleY[graphIndex], self.screenX[graphIndex], self.screenY[graphIndex]);
}

void renderGraph() {
    if (self.matchScaleAndOffset) {
        /* match scales and offsets of editable sources */
        double averageScale = 0;
        double averageOffset = 0;
        int32_t numberOfSelectedSources = 0;
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index >= 0 && self.graph[i].editable) {
                averageScale += self.graph[i].scaleY;
                averageOffset += self.graph[i].screenY;
                numberOfSelectedSources++;
            }
        }
        averageScale /= numberOfSelectedSources;
        averageOffset /= numberOfSelectedSources;
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index >= 0 && self.graph[i].editable) {
                self.graph[i].scaleY = averageScale;
                self.graph[i].screenY = averageOffset;
            }
        }
    }
    if (self.zeroTimestamp) {
        /* zero timestamp at beginning of sources */
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index >= 0 && self.graph[i].editable) {
                uint32_t timestampOffset = self.packets -> data[self.graph[i].index].r -> data[0].r -> data[1].u;
                for (uint32_t j = 0; j < self.packets -> data[self.graph[i].index].r -> length; j++) {
                    self.packets -> data[self.graph[i].index].r -> data[j].r -> data[1].u -= timestampOffset;
                }
            }
        }
    }
    if (self.syncTimestamp) {
        /* phase 1: order data in order of timestamp */
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index >= 0) {
                for (uint32_t j = 1; j < self.packets -> data[self.graph[i].index].r -> length; j++) {
                    if (self.packets -> data[self.graph[i].index].r -> data[j].r -> data[1].u < self.packets -> data[self.graph[i].index].r -> data[j - 1].r -> data[1].u) {
                        list_t *extractPacket = list_delete_no_free(self.packets -> data[self.graph[i].index].r, j).r;
                        int32_t placeIndex = j - 1;
                        while (placeIndex > 0 && self.packets -> data[self.graph[i].index].r -> data[placeIndex].r -> data[1].u > extractPacket -> data[1].u) {
                            placeIndex--;
                        }
                        list_insert(self.packets -> data[self.graph[i].index].r, placeIndex, (unitype) extractPacket, 'r');
                    }
                }
            }
        }
        /* phase 2: remove duplicates */
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].index >= 0) {
                for (uint32_t j = 1; j < self.packets -> data[self.graph[i].index].r -> length; j++) {
                    if (self.packets -> data[self.graph[i].index].r -> data[j].r -> data[1].u == self.packets -> data[self.graph[i].index].r -> data[j - 1].r -> data[1].u) {
                        list_delete(self.packets -> data[self.graph[i].index].r, j);
                        j--;
                    }
                }
            }
        }
        if (self.renderByTimestamp) {
            /* phase 3: match scale */
            double averageXScale = 0;
            double averageXOffset = 0;
            int32_t numberOfSelectedSources = 0;
            for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                if (self.graph[i].index >= 0 && self.graph[i].editable) {
                    averageXScale += self.graph[i].scaleX;
                    averageXOffset += self.graph[i].screenX;
                    numberOfSelectedSources++;
                }
            }
            averageXScale /= numberOfSelectedSources;
            averageXOffset /= numberOfSelectedSources;
            for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                if (self.graph[i].index >= 0 && self.graph[i].editable) {
                    self.graph[i].scaleX = averageXScale;
                    self.graph[i].screenX = averageXOffset;
                }
            }
        } else {
            /* phase 3: sync timestamp across different data sources */
            int32_t zeroIndex = -1;
            double approximateTimeQuanta[NUMBER_OF_GRAPH_SOURCES] = {0}; // ms/samples - how many milliseconds per sample
            for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                if (self.graph[i].index >= 0 && self.graph[i].editable) {
                    if (zeroIndex == -1) {
                        zeroIndex = i;
                    }
                    uint32_t segments = 16;
                    uint32_t stepSize = self.packets -> data[self.graph[i].index].r -> length / segments;
                    if (stepSize == 0) {
                        printf("Could not sync timestamp of source %s\n", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s);
                        continue;
                    }
                    uint32_t index = stepSize;
                    for (uint32_t j = 0; j < segments - 1; j++) {
                        approximateTimeQuanta[i] += (self.packets -> data[self.graph[i].index].r -> data[index].r -> data[1].u - self.packets -> data[self.graph[i].index].r -> data[index - stepSize].r -> data[1].u) / (double) stepSize;
                        index += stepSize;
                    }
                    approximateTimeQuanta[i] /= segments - 1;
                    printf("%s approximateTimeQuanta: %lfms\n", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s, approximateTimeQuanta[i]);
                }
            }
            /* phase 4: calculate new scaleX and screenX */
            if (zeroIndex != -1) {
                double universalScale = approximateTimeQuanta[zeroIndex] / self.graph[zeroIndex].scaleX; // ms/pixels - how many milliseconds per pixel (coordinate not pixel but close enough)
                printf("universal scale (ms/pixels): %lf\n", universalScale);
                double zeroPoint = self.graph[zeroIndex].screenX; // point of 0 timestamp (coordinate)
                printf("zeroPoint: %lf\n", zeroPoint);
                for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                    if (self.graph[i].index >= 0 && self.graph[i].editable) {
                        self.graph[i].scaleX = (approximateTimeQuanta[i] / universalScale); // pixels/samples - how many pixels per sample
                        self.graph[i].screenX = zeroPoint + ((int64_t) self.packets -> data[self.graph[i].index].r -> data[0].r -> data[1].u - self.packets -> data[self.graph[zeroIndex].index].r -> data[0].r -> data[1].u) / universalScale /* / (self.graph[i].scaleX / self.graph[zeroIndex].scaleX) */;
                        printf("scaleX: %lf, screenX: %lf\n", self.graph[i].scaleX, self.graph[i].screenX);
                    }
                }
            }
        }
    }
    list_t *boxContents = list_init();
    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
        if (self.graph[i].index == -1) {
            continue;
        }
        list_t *dataList = self.packets -> data[self.graph[i].index].r;
        tt_setColor(TT_COLOR_TEXT);
        turtlePenSize(1);
        if (dataList -> length == 0) {
            return;
        }
        uint32_t stride = 0;
        if (self.renderByTimestamp) {
            stride = dataList -> length / (self.graph[i].scaleX * 1000000000);
        } else {
            stride = dataList -> length / (self.graph[i].scaleX * 50000000);
        }
        if (stride < 1) {
            stride = 1;
        }
        if (self.graph[i].editable == 0) {
            turtlePenColor(self.unselectedColor[0], self.unselectedColor[1], self.unselectedColor[2]);
            turtle.pena = 0.8;
        } else {
            turtlePenColor(self.dataColors[i * 3], self.dataColors[i * 3 + 1], self.dataColors[i * 3 + 2]);
        }
        double saveColor[3] = {turtle.penr, turtle.peng, turtle.penb};
        /* render data */
        turtlePenShape("triangle");
        double oldX;
        double oldY;
        int32_t index = stride;
        if (dataList -> length > 0) {
            // printf("type: %d\n", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i);
            double value = convertToDouble(dataList -> data[0].r -> data[self.graph[i].field], self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i);
            if (self.renderByTimestamp) {
                oldX = dataList -> data[0].r -> data[1].i * self.graph[i].scaleX + self.graph[i].screenX;
            } else {
                oldX = self.graph[i].screenX;
            }
            oldY = value * self.graph[i].scaleY + self.graph[i].screenY;
        }
        double minimumX = 100000000000000000.0; // DOUBLE_MAX
        int32_t packetIndex = 0;
        while (index < dataList -> length) {
            double x = 0;
            if (self.renderByTimestamp) {
                x = dataList -> data[index].r -> data[1].i * self.graph[i].scaleX + self.graph[i].screenX;
                if (fabs(self.mx - x) < minimumX) {
                    packetIndex = index;
                    minimumX = fabs(self.mx - x);
                }
            } else {
                x = index * self.graph[i].scaleX + self.graph[i].screenX;
            }
            if (x > 340) {
                break;
            }
            if (x > -340) {
                double value = convertToDouble(dataList -> data[index].r -> data[self.graph[i].field], self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i);
                double y = value * self.graph[i].scaleY + self.graph[i].screenY;
                if (dataList -> data[index].r -> data[0].u & 0x1 || dataList -> data[index - stride].r -> data[0].u & 0x1 || (self.selecting && self.graph[i].editable && ((x > self.selectCoords[0] && x < self.selectCoords[2] && y > self.selectCoords[1] && y < self.selectCoords[3]) ||
                    (oldX > self.selectCoords[0] && oldX < self.selectCoords[2] && oldY > self.selectCoords[1] && oldY < self.selectCoords[3])))) {
                    turtlePenColor(self.unselectedColor[0], self.unselectedColor[1], self.unselectedColor[2]);
                } else {
                    turtle.penr = saveColor[0];
                    turtle.peng = saveColor[1];
                    turtle.penb = saveColor[2];
                }
                turtleGoto(oldX, oldY);
                oldX = x;
                oldY = y;
                turtlePenDown();
            }
            index += stride;
        }
        turtlePenUp();

        /* render mouse */
        if (!self.renderByTimestamp) {
            packetIndex = round((self.mx - self.graph[i].screenX) / self.graph[i].scaleX);
        }
        if (packetIndex < 0) {
            packetIndex = 0;
        }
        if (packetIndex >= dataList -> length) {
            packetIndex = dataList -> length - 1;
        }
        double value = convertToDouble(dataList -> data[packetIndex].r -> data[self.graph[i].field], self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i);
        double x = 0;
        if (self.renderByTimestamp) {
            x = dataList -> data[packetIndex].r -> data[1].i * self.graph[i].scaleX + self.graph[i].screenX;
        } else {
            x = packetIndex * self.graph[i].scaleX + self.graph[i].screenX;
        }
        turtlePenSize(3);
        turtlePenColorAlpha(0, 0, 0, 200);
        turtleGoto(x, 180);
        turtlePenDown();
        turtleGoto(x, -180);
        turtlePenUp();
        turtlePenShape("circle");

        turtleGoto(x, value * self.graph[i].scaleY + self.graph[i].screenY);
        turtlePenSize(3);
        tt_setColor(TT_COLOR_TEXT);
        turtlePenDown();
        turtlePenUp();

        char boxAdd[128];
        sprintf(boxAdd, "Timestamp: %.3lf seconds", dataList -> data[packetIndex].r -> data[1].u / 1000.0);
        list_append(boxContents, (unitype) boxAdd, 's');
        list_append(boxContents, (unitype) self.dataColors[i * 3], 'd');
        list_append(boxContents, (unitype) self.dataColors[i * 3 + 1], 'd');
        list_append(boxContents, (unitype) self.dataColors[i * 3 + 2], 'd');
        if (self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i == UFC_PACKET_FIELD_FLOAT || self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i == UFC_PACKET_FIELD_DOUBLE) {
            sprintf(boxAdd, "%s: %lf", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s, value);
        } else {
            sprintf(boxAdd, "%s: %.0lf", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 5].s, round(value));
        }
        list_append(boxContents, (unitype) boxAdd, 's');
        list_append(boxContents, (unitype) self.dataColors[i * 3], 'd');
        list_append(boxContents, (unitype) self.dataColors[i * 3 + 1], 'd');
        list_append(boxContents, (unitype) self.dataColors[i * 3 + 2], 'd');
    }

    double maxLength = 10;
    double boxHeight = 0;
    double boxY = 160;
    for (uint32_t i = 0; i < boxContents -> length; i += 4) {
        double length = turtleTextGetStringLength(boxContents -> data[i].s, 6);
        if (length > maxLength) {
            maxLength = length;
        }
        boxHeight += 10;
    }
    maxLength += 6;
    double boxX = self.mx - maxLength / 2;
    if (boxX < -310) {
        boxX = -310;
    }
    if (boxX + maxLength > 310) {
        boxX = 310 - maxLength;        
    } 
    tt_setColor(TT_COLOR_TEXT_ALTERNATE);
    turtle.pena = 0.2; // 20% opacity
    turtleRectangle(boxX, boxY, boxX + maxLength, boxY - boxHeight);
    tt_setColor(TT_COLOR_POPUP_BOX);
    for (uint32_t i = 0; i < boxContents -> length; i += 4) {
        turtlePenColor(boxContents -> data[i + 1].d, boxContents -> data[i + 2].d, boxContents -> data[i + 3].d);
        turtleTextWriteString(boxContents -> data[i].s, boxX + maxLength / 2, boxY - 10 * (i / 4) - 5, 6, 50);
    }
    list_free(boxContents);

    /* scrolling */
    double scaleFactor = 1;
    if (self.keys[3]) {
        scaleFactor = self.scrollFactorArrow;
    } else {
        if (self.keys[6]) {
            scaleFactor = self.scrollFactorCtrl;
        } else {
            scaleFactor = self.scrollFactor;
        }
    }
    if (self.mw > 0) {
        /* zoom in */
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].editable == 0) {
                continue;
            }
            self.graph[i].screenX -= (self.mx - self.graph[i].screenX) * (scaleFactor - 1);
            self.graph[i].scaleX *= scaleFactor;
            if (!self.keys[5]) {
                self.graph[i].screenY -= (self.my - self.graph[i].screenY) * (scaleFactor - 1);
                self.graph[i].scaleY *= scaleFactor;
            }
        }
    } else if (self.mw < 0) {
        /* zoom out */
        for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
            if (self.graph[i].editable == 0) {
                continue;
            }
            self.graph[i].screenX += (self.mx - self.graph[i].screenX) * (scaleFactor - 1) / scaleFactor;
            self.graph[i].scaleX /= scaleFactor;
            if (!self.keys[5]) {
                self.graph[i].screenY += (self.my - self.graph[i].screenY) * (scaleFactor - 1) / scaleFactor;
                self.graph[i].scaleY /= scaleFactor;
            }
        }
    }
}

void mouseTick() {
    // printf("%d %d\n", self.hoverIndex, self.hoverField);
    // list_print(self.packets);
    if (turtleMouseDown()) {
        if (self.keys[0] == 0) {
            /* first tick */
            self.keys[0] = 1;
            if (self.hoverIndex >= 0) {
                self.uiPacketSelected = self.hoverIndex;
                for (uint32_t i = 0; i < self.sourceSwitches -> length; i++) {
                    switchDeinit((tt_switch_t *) self.sourceSwitches -> data[i].p);
                }
                list_clear(self.sourceSwitches);
                self.matchScaleAndOffsetButton -> enabled = TT_ELEMENT_HIDE;
                self.syncTimestampButton -> enabled = TT_ELEMENT_HIDE;
                self.zeroTimestampButton -> enabled = TT_ELEMENT_HIDE;
            }
            if (self.hoverField >= 0) {
                if (self.directory[self.uiPacketSelected][self.hoverField / 2 - 2]) {
                    self.directory[self.uiPacketSelected][self.hoverField / 2 - 2] = 0;
                    self.directory[self.uiPacketSelected][0] = 0;
                    /* remove from graphSelectedSources */
                    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                        if (self.graph[i].index == self.uiPacketSelected && self.graph[i].field == self.hoverField / 2 - 2) {
                            self.graph[i].index = -1;
                            self.graph[i].field = -1;
                            self.graph[i].editable = 0;
                        }
                        if (self.graph[i].index == self.uiPacketSelected) {
                            self.directory[self.uiPacketSelected][0] = 1;
                        }
                    }
                } else {
                    /* add to graphSelectedSources */
                    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                        if (self.graph[i].index == -1 && self.graph[i].field == -1) {
                            self.graph[i].index = self.uiPacketSelected;
                            self.graph[i].field = self.hoverField / 2 - 2;
                            self.graph[i].editable = 1;
                            self.directory[self.uiPacketSelected][self.hoverField / 2 - 2] = 1;
                            self.directory[self.uiPacketSelected][0] = 1;
                            setGraphScale(i);
                            break;
                        }
                    }
                }
            } else if (self.hoverField == -2) {
                for (uint32_t i = 0; i < self.fieldSwitches -> length; i++) {
                    switchDeinit((tt_switch_t *) self.fieldSwitches -> data[i].p);
                }
                list_clear(self.fieldSwitches);
                self.matchScaleAndOffsetButton -> enabled = TT_ELEMENT_ENABLED;
                self.syncTimestampButton -> enabled = TT_ELEMENT_ENABLED;
                self.zeroTimestampButton -> enabled = TT_ELEMENT_ENABLED;
                self.uiPacketSelected = -1;
            }
            if (self.hoverSource >= 0) {
                if (self.graph[self.hoverSource].editable) {
                    self.graph[self.hoverSource].editable = 0;
                } else {
                    self.graph[self.hoverSource].editable = 1;
                }
            }
            if (self.my < 170 && self.my > -180 + self.bottomBoxHeight) {
                for (uint32_t i = 0; i < self.selected -> length; i++) {
                    selected_t selectedPacket = ((selected_union_t) self.selected -> data[i].l).selected;
                    self.packets -> data[selectedPacket.packetType].r -> data[selectedPacket.index].r -> data[0].u &= 0xFFFFFFFE;
                }
                list_clear(self.selected);
                if (self.keys[5]) {
                    self.selecting = 1;
                    self.anchorSelectX = self.mx;
                    self.anchorSelectY = self.my;
                } else {
                    self.dragging = 1;
                    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                        if (self.graph[i].editable == 0) {
                            continue;
                        }
                        self.graph[i].anchorX = self.mx;
                        self.graph[i].anchorY = self.my;
                        self.graph[i].anchorScreenX = self.graph[i].screenX;
                        self.graph[i].anchorScreenY = self.graph[i].screenY;
                    }
                }
            }
        } else {
            /* mouse hold */
            if (self.dragging) {
                for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                    if (self.graph[i].editable == 0) {
                        continue;
                    }
                    self.graph[i].screenX = self.graph[i].anchorScreenX + (self.mx - self.graph[i].anchorX);
                    self.graph[i].screenY = self.graph[i].anchorScreenY + (self.my - self.graph[i].anchorY);
                }
            }
            if (self.selecting) {
                turtlePenSize(1);
                turtlePenColor(self.selectedColor[0], self.selectedColor[1], self.selectedColor[2]);
                turtleGoto(self.anchorSelectX, self.anchorSelectY);
                turtlePenDown();
                turtleGoto(self.mx, self.anchorSelectY);
                turtleGoto(self.mx, self.my);
                turtleGoto(self.anchorSelectX, self.my);
                turtleGoto(self.anchorSelectX, self.anchorSelectY);
                turtlePenUp();
                if (self.mx > self.anchorSelectX) {
                    self.selectCoords[0] = self.anchorSelectX;
                    self.selectCoords[2] = self.mx;
                } else {
                    self.selectCoords[0] = self.mx;
                    self.selectCoords[2] = self.anchorSelectX;
                }
                if (self.my > self.anchorSelectY) {
                    self.selectCoords[1] = self.anchorSelectY;
                    self.selectCoords[3] = self.my;
                } else {
                    self.selectCoords[1] = self.my;
                    self.selectCoords[3] = self.anchorSelectY;
                }
            }
        }
    } else {
        /* left click released */
        self.dragging = 0;
        if (self.selecting) {
            self.selecting = 0;
            /* add all selected to selected list */
            for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                if (self.graph[i].index == -1 || self.graph[i].editable == 0) {
                    continue;
                }
                list_t *dataList = self.packets -> data[self.graph[i].index].r;
                if (dataList -> length == 0) {
                    return;
                }
                selected_t newItem;
                newItem.packetType = self.graph[i].index;
                for (uint32_t index = 0; index < dataList -> length; index++) {
                    double x = 0;
                    if (self.renderByTimestamp) {
                        x = dataList -> data[index].r -> data[1].i * self.graph[i].scaleX + self.graph[i].screenX;
                    } else {
                        x = index * self.graph[i].scaleX + self.graph[i].screenX;
                    }
                    if (x > 340) {
                        break;
                    }
                    if (x > -340) {
                        double value = convertToDouble(dataList -> data[index].r -> data[self.graph[i].field], self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 4].i);
                        double y = value * self.graph[i].scaleY + self.graph[i].screenY;
                        if (x > self.selectCoords[0] && x < self.selectCoords[2] && y > self.selectCoords[1] && y < self.selectCoords[3]) {
                            /* add to selected */
                            if ((dataList -> data[index].r -> data[0].u & 0x1) == 0) {
                                dataList -> data[index].r -> data[0].u |= 0x1;
                                newItem.index = index;
                                list_append(self.selected, (unitype) ((selected_union_t) newItem).selectedPacked, 'l');
                            }
                        }
                    }
                }
            }
        }
        self.keys[0] = 0;
    }
    if (turtleMouseRight()) {
        if (self.keys[1] == 0) {
            /* first tick */
            self.keys[1] = 1;
        } else {
            /* right click held */
        }
    } else {
        /* right click released */
        self.keys[1] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_UP)) {
        self.keys[3] = 1;
    } else {
        self.keys[3] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_DOWN)) {
        self.keys[4] = 1;
    } else {
        self.keys[4] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
        self.keys[5] = 1;
    } else {
        self.keys[5] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        self.keys[6] = 1;
    } else {
        self.keys[6] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_DELETE) || turtleKeyPressed(GLFW_KEY_BACKSPACE)) {
        if (self.keys[7] == 0) {
            self.keys[7] = 1;
            for (int32_t i = self.selected -> length - 1; i > -1; i--) {
                selected_t selectedPacket = ((selected_union_t) self.selected -> data[i].l).selected;
                list_delete(self.packets -> data[selectedPacket.packetType].r, selectedPacket.index);
                list_pop(self.selected);
            }
        }
    } else {
        self.keys[7] = 0;
    }
}

void parseRibbonOutput() {
    if (ribbonRender.output[0] == 1) {
        ribbonRender.output[0] = 0;
        if (ribbonRender.output[1] == 0) { // File
            if (ribbonRender.output[2] == 1) { // New
                printf("New\n");
            }
            if (ribbonRender.output[2] == 2) { // Save
                if (strcmp(self.alreadyExported, "null") != 0) {
                    export(self.alreadyExported);
                } else {
                    if (osToolsFileDialogPrompt(1, "") != -1) {
                        printf("Saved to: %s\n", osToolsFileDialog.selectedFilename);
                        export(osToolsFileDialog.selectedFilename);
                    }
                }
            }
            if (ribbonRender.output[2] == 3) { // Save As...
                if (osToolsFileDialogPrompt(1, "") != -1) {
                    printf("Saved to: %s\n", osToolsFileDialog.selectedFilename);
                    export(osToolsFileDialog.selectedFilename);
                }
            }
            if (ribbonRender.output[2] == 4) { // Open
                if (osToolsFileDialogPrompt(0, "") != -1) {
                    printf("Loaded data from: %s\n", osToolsFileDialog.selectedFilename);
                    import(osToolsFileDialog.selectedFilename);
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

void utilLoop() {
    turtleGetMouseCoords(); // get the mouse coordinates
    if (turtle.mouseX > 320) { // bound mouse coordinates to window coordinates
        self.mx = 320;
    } else {
        if (turtle.mouseX < -320) {
            self.mx = -320;
        } else {
            self.mx = turtle.mouseX;
        }
    }
    if (turtle.mouseY > 180) {
        self.my = 180;
    } else {
        if (turtle.mouseY < -180) {
            self.my = -180;
        } else {
            self.my = turtle.mouseY;
        }
    }
    self.mw = turtleMouseWheel();
    if (turtleKeyPressed(GLFW_KEY_UP)) {
        self.mw += 1;
    }
    if (turtleKeyPressed(GLFW_KEY_DOWN)) {
        self.mw -= 1;
    }
    turtleClear();
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
    GLFWwindow *window = glfwCreateWindow(windowHeight * 16 / 9, windowHeight, "VisualisingUFC", NULL, NULL);
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
    turtleBgColor(30, 30, 30);
    turtleToolsSetTheme(TT_THEME_DARK); // dark theme preset
    ribbonInit("include/ribbonConfig.txt");
    /* initialise osTools */
    osToolsInit(argv[0], window); // must include argv[0] to get executableFilepath, must include GLFW window
    osToolsFileDialogAddExtension("txt"); // add txt to extension restrictions
    osToolsFileDialogAddExtension("csv"); // add csv to extension restrictions

    uint32_t tps = 120; // ticks per second (locked to fps in this case)
    uint64_t tick = 0; // count number of ticks since application started
    clock_t start, end;

    init();

    if (argc > 1) {
        import(argv[1]);
    }

    while (turtle.shouldClose == 0) {
        start = clock();
        utilLoop();
        renderGraph();
        renderInfo();
        mouseTick();
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