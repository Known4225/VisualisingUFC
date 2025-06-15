#include "include/turtleTools.h"
#include "include/osTools.h"
#include <time.h>

/* up to 8 sources graphed at once */
#define NUMBER_OF_GRAPH_SOURCES 8

typedef struct {
    int32_t index;
    int32_t field;
} graph_source_t;

typedef struct {
    list_t *data;
    list_t *packetDefinitions;
    list_t *packets;
    list_t *graphSourceSelected;
    int8_t **directory;
    graph_source_t graph[NUMBER_OF_GRAPH_SOURCES];
    double screenX[NUMBER_OF_GRAPH_SOURCES];
    double screenY[NUMBER_OF_GRAPH_SOURCES];
    double scaleX[NUMBER_OF_GRAPH_SOURCES];
    double scaleY[NUMBER_OF_GRAPH_SOURCES];
    double anchorX[NUMBER_OF_GRAPH_SOURCES];
    double anchorY[NUMBER_OF_GRAPH_SOURCES];
    double anchorScreenX[NUMBER_OF_GRAPH_SOURCES];
    double anchorScreenY[NUMBER_OF_GRAPH_SOURCES];

    /* mouse */
    double mx; // mouseX
    double my; // mouseY
    double mw; // mouseWheel
    uint8_t keys[12];
    uint8_t dragging;
    double scrollFactor;
    double scrollFactorCtrl;
    double scrollFactorArrow;

    /* UI */
    double bottomBoxHeight;
    double dataColors[90];
    int32_t hoverIndex;
    int32_t hoverField;
    int32_t uiPacketSelected;
    list_t *fieldSwitches;

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
    uint32_t packetField = 2;
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
    packetField = 2;
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
            case UFC_PACKET_FIELD_INT8:
            list_append(packet, (unitype) *((uint8_t *) (file + i)), 'b');
            break;
            case UFC_PACKET_FIELD_UINT8:
            list_append(packet, (unitype) *((uint8_t *) (file + i)), 'b');
            break;
            default:
            break;
        }
        packetField += 2;
    }
    list_append(self.packets -> data[packetIndex].r, (unitype) packet, 'r');
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
    // fileSize /= 15;
    for (uint32_t i = 0; i < fileSize; i++) {
        for (uint32_t j = 0; j < self.packetDefinitions -> length; j++) {
            matchPacket(j, fileData + i, fileSize - i);
        }
    }
    // list_print(self.packets -> data[3].r);
    // for (uint32_t i = 0; i < 100; i++) {
    //     printf("%X ", fileData[i]);
    // }
    unmapFile(fileData);
}

void init() {
    /* we assume that big endian machines do not exist */
    self.data = list_init();
    self.packetDefinitions = list_init();
    self.packets = list_init();

    /* Status packet */
    list_t *statusPacket = list_init();
    list_append(statusPacket, (unitype) "statusPacket", 's');
    list_append(statusPacket, (unitype) 24, 'i'); // status packet is 24 bytes big
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
    list_append(gpsPacket, (unitype) 24, 'i'); // gps packet is 80 bytes big
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
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT16, 'i');
    list_append(gpsPacket, (unitype) -1, 'u');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) -1, 'u');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "num_satellites", 's');
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
    for (uint32_t i = 0; i < sizeof(self.keys); i++) {
        self.keys[i] = 0;
    }

    self.hoverIndex = -1;
    self.hoverField = -1;
    self.uiPacketSelected = -1;
    self.graphSourceSelected = list_init();
    self.fieldSwitches = list_init();
    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
        list_append(self.graphSourceSelected, (unitype) i, 'i');
        self.graph[i].index = -1;
        self.graph[i].field = -1;
        self.screenX[i] = 0;
        self.screenY[i] = 0;
        self.scaleX[i] = 1;
        self.scaleY[i] = 1;
        self.anchorX[i] = 0;
        self.anchorY[i] = 0;
        self.anchorScreenX[i] = 0;
        self.anchorScreenY[i] = 0;
    }

    double dataColorCopy[] = {
        19, 236, 48, // data color (channel 1)
        0, 221, 255, // data color (channel 2)
        200, 200, 200, // data color (channel 3)
        232, 15, 136, // data color (channel 4)
        19, 236, 48, // data color (channel 5)
        0, 221, 255, // data color (channel 6)
        200, 200, 200, // data color (channel 7)
        232, 15, 136, // data color (channel 8)
    };
    memcpy(self.dataColors, dataColorCopy, sizeof(dataColorCopy));
}

void renderInfo() {
    /* draw box */
    tt_setColor(TT_COLOR_POPUP_BOX);
    turtle.pena = 0.2; // 50% opacity
    turtleRectangle(-320, -180, 320, -180 + self.bottomBoxHeight);

    /* reset hover */
    self.hoverIndex = -1;
    self.hoverField = -1;

    if (self.uiPacketSelected == -1) {
        /* draw packets */
        for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
            tt_setColor(TT_COLOR_TEXT_ALTERNATE);
            double yPos = -180 + self.bottomBoxHeight - (i + 1) * 12;
            if (self.my > yPos - 6 && self.my < yPos + 6 && self.mx > -310 && self.mx < -302 + turtleTextGetStringLengthf(8, "Total %ss: %d", self.packetDefinitions -> data[i].r -> data[0].s, self.packets -> data[i].r -> length)) {
                tt_setColor(TT_COLOR_RIBBON_TOP);
                self.hoverIndex = i;
            }
            turtleTextWriteStringf(-306, yPos, 8, 0, "Total %ss: %d", self.packetDefinitions -> data[i].r -> data[0].s, self.packets -> data[i].r -> length);
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
                if (self.directory[self.uiPacketSelected][i / 2 - 1]) {
                    tt_setColor(TT_COLOR_TEXT_ALTERNATE);
                }
                turtleTextWriteString(self.packetDefinitions -> data[self.uiPacketSelected].r -> data[i + 1].s, xPos, yPos, 5, 0);
                if (populateSwitches) {
                    list_append(self.fieldSwitches, (unitype) (void *) switchInit("", &(self.directory[self.uiPacketSelected][i / 2 - 1]), xPos - 10, yPos, 5), 'l'); // avoid double freeing
                    ((tt_switch_t *) (self.fieldSwitches -> data[self.fieldSwitches -> length - 1].p)) -> enabled = 0;
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
            tt_setColor(TT_COLOR_RIBBON_TOP);
            self.hoverField = -2;
        } else {
            tt_setColor(TT_COLOR_TEXT_ALTERNATE);
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
        self.screenX[graphIndex] = 0;
        self.screenY[graphIndex] = 0;
        self.scaleX[graphIndex] = 1;
        self.scaleY[graphIndex] = 1;
        return;
    }
    double minValue = 100000000000000000.0;
    double maxValue = -100000000000000000.0;
    uint32_t stride = dataList -> length / (self.scaleX[graphIndex] * 50000000);
    if (stride < 1) {
        stride = 1;
    }
    int32_t index = 0;
    while (index < dataList -> length) {
        float value = dataList -> data[index].r -> data[self.graph[graphIndex].field].f;
        if (value > maxValue) {
            maxValue = value;
        }
        if (value < minValue) {
            minValue = value;
        }
        index += stride;
    }
    self.scaleX[graphIndex] = 640.0 / dataList -> length;
    self.scaleY[graphIndex] = (320 - self.bottomBoxHeight) / (maxValue - minValue);
    self.screenX[graphIndex] = -320;
    self.screenY[graphIndex] = ((maxValue + minValue) / 2) * -self.scaleY[graphIndex] + self.bottomBoxHeight / 2;
    printf("%lf %lf %lf %lf\n", self.scaleX[graphIndex], self.scaleY[graphIndex], self.screenX[graphIndex], self.screenY[graphIndex]);
}

void renderGraph() {
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
        uint32_t stride = dataList -> length / (self.scaleX[i] * 50000000);
        if (stride < 1) {
            stride = 1;
        }
        turtlePenColor(self.dataColors[i * 3], self.dataColors[i * 3 + 1], self.dataColors[i * 3 + 2]);
        turtlePenShape("square");
        int32_t index = 0;
        while (index < dataList -> length) {
            if (index * self.scaleX[i] + self.screenX[i] > 320) {
                break;
            }
            if (index * self.scaleX[i] + self.screenX[i] > -340) {
                float value = dataList -> data[index].r -> data[self.graph[i].field].f;
                turtleGoto(index * self.scaleX[i] + self.screenX[i], value * self.scaleY[i] + self.screenY[i]);
                turtlePenDown();
            }
            index += stride;
        }
        turtlePenUp();

        /* render mouse */
        int32_t packetIndex = round((self.mx - self.screenX[i]) / self.scaleX[i]);
        if (packetIndex < 0) {
            packetIndex = 0;
        }
        if (packetIndex >= dataList -> length) {
            packetIndex = dataList -> length - 1;
        }
        float value = dataList -> data[packetIndex].r -> data[self.graph[i].field].f;
        turtlePenSize(3);
        turtlePenColorAlpha(0, 0, 0, 200);
        turtleGoto(packetIndex * self.scaleX[i] + self.screenX[i], 180);
        turtlePenDown();
        turtleGoto(packetIndex * self.scaleX[i] + self.screenX[i], -180);
        turtlePenUp();
        turtlePenShape("circle");

        turtleGoto(packetIndex * self.scaleX[i] + self.screenX[i], value * self.scaleY[i] + self.screenY[i]);
        turtlePenSize(3);
        tt_setColor(TT_COLOR_TEXT);
        turtlePenDown();
        turtlePenUp();

        char boxAdd[128];
        sprintf(boxAdd, "Timestamp: %.3lf seconds", dataList -> data[packetIndex].r -> data[1].u / 1000.0);
        list_append(boxContents, (unitype) boxAdd, 's');
        sprintf(boxAdd, "%s: %f", self.packetDefinitions -> data[self.graph[i].index].r -> data[self.graph[i].field * 2 + 3].s, value);
        list_append(boxContents, (unitype) boxAdd, 's');
    }

    double maxLength = 10;
    double boxHeight = 0;
    double boxY = 155;
    for (uint32_t i = 0; i < boxContents -> length; i++) {
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
    turtle.pena = 0.2;
    turtleRectangle(boxX, boxY, boxX + maxLength, boxY - boxHeight);
    tt_setColor(TT_COLOR_POPUP_BOX);
    for (uint32_t i = 0; i < boxContents -> length; i++) {
        turtleTextWriteString(boxContents -> data[i].s, boxX + maxLength / 2, boxY - 10 * i - 5, 6, 50);
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
        for (uint32_t i = 0; i < self.graphSourceSelected -> length; i++) {
            uint32_t modify = self.graphSourceSelected -> data[i].i;
            self.screenX[modify] -= (self.mx - self.screenX[modify]) * (scaleFactor - 1);
            self.scaleX[modify] *= scaleFactor;
            if (!self.keys[5]) {
                self.screenY[modify] -= (self.my - self.screenY[modify]) * (scaleFactor - 1);
                self.scaleY[modify] *= scaleFactor;
            }
        }
    } else if (self.mw < 0) {
        /* zoom out */
        for (uint32_t i = 0; i < self.graphSourceSelected -> length; i++) {
            uint32_t modify = self.graphSourceSelected -> data[i].i;
            self.screenX[modify] += (self.mx - self.screenX[modify]) * (scaleFactor - 1) / scaleFactor;
            self.scaleX[modify] /= scaleFactor;
            if (!self.keys[5]) {
                self.screenY[modify] += (self.my - self.screenY[modify]) * (scaleFactor - 1) / scaleFactor;
                self.scaleY[modify] /= scaleFactor;
            }
        }
    }
}

void mouseTick() {
    if (turtleMouseDown()) {
        if (self.keys[0] == 0) {
            self.keys[0] = 1;
            if (self.hoverIndex >= 0) {
                self.uiPacketSelected = self.hoverIndex;
                /* reset zoom */
                // self.screenX = 0;
                // self.screenY = 0;
                // self.scaleX = 1;
                // self.scaleX = 1;
                // /* auto adjust height */
                // setGraphScale();
            }
            if (self.hoverField >= 0) {
                if (self.directory[self.uiPacketSelected][self.hoverField / 2 - 1]) {
                    self.directory[self.uiPacketSelected][self.hoverField / 2 - 1] = 0;
                    /* remove from graphSelectedSources */
                    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                        if (self.graph[i].index == self.uiPacketSelected && self.graph[i].field == self.hoverField / 2 - 1) {
                            self.graph[i].index = -1;
                            self.graph[i].field = -1;
                        }
                    }
                } else {
                    /* add to graphSelectedSources */
                    for (uint32_t i = 0; i < NUMBER_OF_GRAPH_SOURCES; i++) {
                        if (self.graph[i].index == -1 && self.graph[i].field == -1) {
                            self.graph[i].index = self.uiPacketSelected;
                            self.graph[i].field = self.hoverField / 2 - 1;
                            self.directory[self.uiPacketSelected][self.hoverField / 2 - 1] = 1;
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
                self.uiPacketSelected = -1;
            }
            if (self.my < 170 && self.my > -180 + self.bottomBoxHeight) {
                self.dragging = 1;
                for (uint32_t i = 0; i < self.graphSourceSelected -> length; i++) {
                    uint32_t modify = self.graphSourceSelected -> data[i].i;
                    self.anchorX[modify] = self.mx;
                    self.anchorY[modify] = self.my;
                    self.anchorScreenX[modify] = self.screenX[modify];
                    self.anchorScreenY[modify] = self.screenY[modify];
                }
            }
        } else {
            if (self.dragging) {
                for (uint32_t i = 0; i < self.graphSourceSelected -> length; i++) {
                    uint32_t modify = self.graphSourceSelected -> data[i].i;
                    self.screenX[modify] = self.anchorScreenX[modify] + (self.mx - self.anchorX[modify]);
                    self.screenY[modify] = self.anchorScreenY[modify] + (self.my - self.anchorY[modify]);
                }
            }
        }
    } else {
        self.dragging = 0;
        self.keys[0] = 0;
    }
    if (turtleMouseRight()) {
        if (self.keys[1] == 0) {
            self.keys[1] = 1;
        }
    } else {
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
                import(osToolsFileDialog.selectedFilename);
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