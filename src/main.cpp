/*
 Copyright (c) 2026 BinaryInkTN

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in
 the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */



#include "BluetoothSerial.h"
#include "aroma.h"
#include <Arduino.h>
#include "backends/aroma_abi.h"
#include "backends/graphics/aroma_graphics_interface.h"

#define WINDOW_WIDTH    480
#define WINDOW_HEIGHT   320
#define MARGIN_X        40      
#define MARGIN_Y        40      
#define WIDGET_WIDTH    320     
#define LABEL_HEIGHT    30      
#define PROGRESS_HEIGHT 16      
#define VERTICAL_SPACING 20     
#define SECTION_SPACING 30  

#pragma pack(push, 1)
struct MetricPacket {
    uint8_t header[2];
    float current_cpu_usage;
    float current_ram_usage;
    uint32_t timestamp;
};
#pragma pack(pop)

MetricPacket rx_packet;
bool packet_ready = false;
AromaWindow* window = NULL;
AromaLabel* cpu_label = NULL;
AromaProgressBar* cpu_usage = NULL;
AromaLabel* ram_label = NULL;
AromaProgressBar* ram_usage = NULL;
AromaLabel* status_label = NULL;
AromaLabel* timestamp_label = NULL;

uint32_t packet_count = 0;
uint32_t last_received_time = 0;
float last_cpu_value = 0.0f;
float last_ram_value = 0.0f;

int calculateYPosition(int widgetIndex) {
    return MARGIN_Y + (widgetIndex * (LABEL_HEIGHT + PROGRESS_HEIGHT + VERTICAL_SPACING));
}

void receive_packets() {
    static uint8_t buffer[sizeof(MetricPacket)];
    static int index = 0;
    static enum { SYNC1, SYNC2, DATA } state = SYNC1;

    while (Serial.available() > 0) {
        uint8_t byte = Serial.read();

        switch (state) {
            case SYNC1:
                if (byte == 0xAA) {
                    buffer[0] = byte;
                    index = 1;
                    state = SYNC2;
                }
                break;

            case SYNC2:
                if (byte == 0x55) {
                    buffer[1] = byte;
                    index = 2;
                    state = DATA;
                } else {
                    state = SYNC1; 

                }
                break;

            case DATA:
                if (index < sizeof(MetricPacket)) {
                    buffer[index++] = byte;

                    if (index >= sizeof(MetricPacket)) {

                        memcpy(&rx_packet, buffer, sizeof(MetricPacket));

                        if (rx_packet.current_cpu_usage >= 0.0f && 
                            rx_packet.current_cpu_usage <= 100.0f &&
                            rx_packet.current_ram_usage >= 0.0f && 
                            rx_packet.current_ram_usage <= 100.0f) {

                            packet_ready = true;
                            packet_count++;
                            last_received_time = millis();
                            last_cpu_value = rx_packet.current_cpu_usage;
                            last_ram_value = rx_packet.current_ram_usage;
                        }

                        index = 0;
                        state = SYNC1;
                    }
                } else {

                    index = 0;
                    state = SYNC1;
                }
                break;
        }
    }
}

void update_ui() {
    if (!packet_ready) return;

    if (cpu_usage) {
        float cpu_percent = rx_packet.current_cpu_usage / 100.0f;
        aroma_progressbar_set_progress((AromaNode*)cpu_usage, cpu_percent);
    }

    if (ram_usage) {
        float ram_percent = rx_packet.current_ram_usage / 100.0f;
        aroma_progressbar_set_progress((AromaNode*)ram_usage, ram_percent);
    }

    if (cpu_label) {
        char cpu_text[32];
        snprintf(cpu_text, sizeof(cpu_text), "CPU Usage: %.1f%%", rx_packet.current_cpu_usage);
        aroma_label_set_text((AromaNode*)cpu_label, cpu_text);
    }

    if (ram_label) {
        char ram_text[32];
        snprintf(ram_text, sizeof(ram_text), "RAM Usage: %.1f%%", rx_packet.current_ram_usage);
        aroma_label_set_text((AromaNode*)ram_label, ram_text);
    }

    if (timestamp_label) {
        char status_text[64];
        uint32_t seconds = rx_packet.timestamp / 1000;
        snprintf(status_text, sizeof(status_text), 
                "Packets: %lu | Time: %lu:%02lu | %lu ms ago", 
                packet_count,
                seconds / 60, seconds % 60,
                millis() - last_received_time);
        aroma_label_set_text((AromaNode*)timestamp_label, status_text);
    }

    packet_ready = false;
}

void check_connection_status() {
    static uint32_t last_status_check = 0;
    uint32_t now = millis();

    if (now - last_status_check > 1000) {
        if (status_label) {
            if (now - last_received_time > 3000) {
                aroma_label_set_text((AromaNode*)status_label, "Status: Disconnected");
            } else {
                aroma_label_set_text((AromaNode*)status_label, "Status: Connected");
            }
        }
        last_status_check = now;
    }
}

void setup() {

    Serial.begin(9600);  

    aroma_ui_init();

    AromaTheme preset = aroma_theme_create_material_blue_dark();
    aroma_ui_set_theme(&preset);

    window = aroma_ui_create_window("System Monitor", WINDOW_WIDTH, WINDOW_HEIGHT);
    aroma_event_set_root((AromaNode*)window);

    int cpu_label_y = calculateYPosition(0);
    int cpu_progress_y = cpu_label_y + LABEL_HEIGHT;
    int ram_label_y = calculateYPosition(2); 
    int ram_progress_y = ram_label_y + LABEL_HEIGHT;
    int status_y = calculateYPosition(4);
    int timestamp_y = calculateYPosition(5);

    cpu_label = (AromaLabel*)aroma_label_create(
        (AromaNode*)window, 
        "CPU Usage: 0.0%", 
        MARGIN_X, 
        cpu_label_y, 
        LABEL_STYLE_LABEL_LARGE
    );

    cpu_usage = (AromaProgressBar*)aroma_progressbar_create(
        (AromaNode*)window, 
        MARGIN_X, 
        cpu_progress_y, 
        WIDGET_WIDTH, 
        PROGRESS_HEIGHT, 
        PROGRESS_TYPE_DETERMINATE
    );

    ram_label = (AromaLabel*)aroma_label_create(
        (AromaNode*)window, 
        "RAM Usage: 0.0%", 
        MARGIN_X, 
        ram_label_y, 
        LABEL_STYLE_LABEL_LARGE
    );

    ram_usage = (AromaProgressBar*)aroma_progressbar_create(
        (AromaNode*)window, 
        MARGIN_X, 
        ram_progress_y, 
        WIDGET_WIDTH, 
        PROGRESS_HEIGHT, 
        PROGRESS_TYPE_DETERMINATE
    );

    status_label = (AromaLabel*)aroma_label_create(
        (AromaNode*)window, 
        "Status: Connecting...", 
        MARGIN_X, 
        status_y, 
        LABEL_STYLE_LABEL_SMALL
    );

    timestamp_label = (AromaLabel*)aroma_label_create(
        (AromaNode*)window, 
        "Packets: 0", 
        MARGIN_X, 
        timestamp_y, 
        LABEL_STYLE_LABEL_SMALL
    );

    aroma_ui_request_redraw(NULL);

    Serial.println("ESP32 System Monitor Ready");
    Serial.printf("Expecting packet size: %d bytes\n", sizeof(MetricPacket));
}

void loop() {

    receive_packets();

    update_ui();

    //check_connection_status();

    aroma_ui_process_events();

    aroma_ui_render(window);

    //delay(16); 

}