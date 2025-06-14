#include "include/turtleTools.h"
#include "include/osTools.h"
#include <time.h>

typedef struct {
    list_t *data;
    list_t *packetDefinitions;
    list_t *packets;
    double mx; // mouseX
    double my; // mouseY
    double mw; // mouseWheel
    double bottomBoxHeight;

    double dataColors[36];

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
    list_append(gpsPacket, (unitype) "UNUSED", 's');
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
    list_append(gpsPacket, (unitype) "UNUSED", 's');
    list_append(gpsPacket, (unitype) UFC_PACKET_FIELD_UINT8, 'i');
    list_append(gpsPacket, (unitype) "UNUSED", 's');
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

    list_print(self.packetDefinitions);

    for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
        list_append(self.packets, (unitype) list_init(), 'r');
    }

    self.graphLower = 0;
    self.graphUpper = 

    /* general */
    self.mx = 0;
    self.my = 0;
    self.mw = 0;
    self.bottomBoxHeight = self.packetDefinitions -> length * 12 + 3;

    double dataColorCopy[] = {
        19, 236, 48, // data color (channel 1)
        0, 221, 255, // data color (channel 2)
        200, 200, 200, // data color (channel 3)
        232, 15, 136, // data color (channel 4)
    };
    memcpy(self.dataColors, dataColorCopy, sizeof(dataColorCopy));
}

void renderInfo() {
    tt_setColor(TT_COLOR_RIBBON_TOP);
    turtle.pena = 0.8; // 80% opacity
    turtleRectangle(-320, -180, 320, -180 + self.bottomBoxHeight);
    tt_setColor(TT_COLOR_TEXT_ALTERNATE);
    for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
        turtleTextWriteStringf(-316, -180 + self.bottomBoxHeight - i * 12, 8, 0, "Total %ss: %d", self.packetDefinitions -> data[i].r -> data[0].s, self.packets -> data[i].r -> length);
    }
}

void renderGraph(uint32_t packetIndex) {
    list_t *dataList = self.packets -> data[packetIndex].r;
    tt_setColor(TT_COLOR_TEXT);
    turtlePenSize(1);
    int32_t pitotIndex = -1;
    for (uint32_t i = 0; i < self.packetDefinitions -> length; i++) {
        if (strcmp(self.packetDefinitions -> data[i].r -> data[0].s, "pitotCenterPacket") == 0) {
            pitotIndex = i;
            break;
        }
    }
    if (pitotIndex == -1) {
        return;
    }
    if (self.packets -> data[pitotIndex].r -> length == 0) {
        return;
    }
    // printf("pitotIndex: %d\n", pitotIndex);
    list_t *pitotList = self.packets -> data[pitotIndex].r;
    uint32_t lowerBound = 0;
    uint32_t upperBound = pitotList -> length;
    // uint32_t stride = 0;
    uint32_t stride = (upperBound - lowerBound) / 2500;
    turtlePenColor(self.dataColors[0], self.dataColors[1], self.dataColors[2]);
    turtlePenShape("square");
    turtleGoto(-310 + (620.0 / (upperBound - lowerBound)) * 0, pitotList -> data[0].r -> data[5].f * 180 - 290);
    turtlePenDown();
    /* delim, timestamp, pkt_type, state, pkt_len, center, static, footer */
    for (uint32_t i = 0; i < upperBound; i++) {
        float center_port = pitotList -> data[i].r -> data[5].f;
        turtleGoto(-310 + (620.0 / (upperBound - lowerBound)) * i, center_port * 180 - 290);
        i += stride;
        // printf("%.6f, %.6f\n", center_port, static_port);
    }
    turtlePenUp();

    /* render mouse */
    int32_t packetIndex = ((upperBound - lowerBound) / 620.0) * (turtle.mouseX + 310) + lowerBound;
    if (packetIndex < lowerBound) {
        packetIndex = lowerBound;
    }
    if (packetIndex >= upperBound) {
        packetIndex = upperBound - 1;
    }
    float center_port = pitotList -> data[packetIndex].r -> data[5].f;
    turtlePenSize(3);
    turtlePenColorAlpha(0, 0, 0, 200);
    turtleGoto(-310 + (620.0 / (upperBound - lowerBound)) * packetIndex, 180);
    turtlePenDown();
    turtleGoto(-310 + (620.0 / (upperBound - lowerBound)) * packetIndex, -180);
    turtlePenUp();
    turtlePenShape("circle");

    turtleGoto(-310 + (620.0 / (upperBound - lowerBound)) * packetIndex, center_port * 180 - 290);
    turtlePenSize(3);
    tt_setColor(TT_COLOR_TEXT);
    turtlePenDown();
    turtlePenUp();
    tt_setColor(TT_COLOR_TEXT);
    turtleTextWriteStringf(turtle.mouseX, 140, 6, 50, "Timestamp: %.2lf seconds", pitotList -> data[packetIndex].r -> data[1].u / 1000.0);
    turtleTextWriteStringf(turtle.mouseX, 130, 6, 50, "Center port voltage: %fV", center_port);

    /* scrolling */
    if (self.mw > 0) {
        /* zoom in */
        self.graphZoom *= scaleFactor;
        if (self.graphZoom > 100.0) {
            self.graphZoom = 100.0;
        }
        self.freqLeftBound += round(buckets - (buckets / scaleFactor));
        if (self.freqLeftBound >= self.freqRightBound) {
            self.freqLeftBound = self.freqRightBound - 1;
        }
    } else if (self.mw < 0) {
        /* zoom out */
        self.freqZoom /= scaleFactor;
        if (self.freqZoom < 1.0) {
            self.freqZoom = 1.0;
        }
        self.freqLeftBound -= round(buckets - (buckets / scaleFactor));
        if (self.freqLeftBound < 0) {
            self.freqLeftBound = 0;
        }
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
    // if (turtleKeyPressed(GLFW_KEY_UP)) {
    //     self.mw += 1;
    // }
    // if (turtleKeyPressed(GLFW_KEY_DOWN)) {
    //     self.mw -= 1;
    // }
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
        turtleGetMouseCoords();
        turtleClear();
        renderGraph(5);
        renderInfo();
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