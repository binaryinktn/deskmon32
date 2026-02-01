#include "aroma.h"
#include <Arduino.h>
#include "images/drive_icon.h"
#include "images/video_card_icon.h"
#include "images/cpu_icon.h"

#define WINDOW_WIDTH        480
#define WINDOW_HEIGHT       320
#define MARGIN_X            40
#define MARGIN_Y            80
#define WIDGET_WIDTH        220
#define LABEL_HEIGHT        30
#define PROGRESS_HEIGHT     16
#define VERTICAL_SPACING    20
#define SPLASH_TIMEOUT_MS   4000
#define TAB_CYCLE_MS        6000
#define MAX_DRIVES          10
#define MAX_DRIVE_LABELS    3

static AromaWindow* window = NULL;
static AromaNode* tabs = NULL;

static AromaNode* cpu_label = NULL;
static AromaNode* cpu_usage = NULL;
static AromaNode* ram_label = NULL;
static AromaNode* ram_usage = NULL;
static AromaNode* cpu_icon = NULL;


static AromaNode* gpu_fan_label = NULL;
static AromaNode* gpu_fan_speed = NULL;
static AromaNode* gpu_util_label = NULL;
static AromaNode* gpu_utilisation = NULL;
static AromaNode* gpu_temp_label = NULL;
static AromaNode* gpu_temp_usage = NULL;


static AromaNode* disk_icon = NULL;
static AromaNode* gpu_icon = NULL;
static AromaNode* drive_labels[MAX_DRIVE_LABELS] = {0};

static AromaLabel* splash_label = NULL;
#pragma pack(push, 1)
typedef struct {
    uint8_t header[2];

    float cpu;
    float ram;

    float cpu_temp;
    float cpu_fan;

    float gpu_fan;
    float gpu_util;
    float gpu_temp;

    char  drive_letters[MAX_DRIVES];
    float drive_used[MAX_DRIVES];
    float drive_total[MAX_DRIVES];
    float drive_free[MAX_DRIVES];

    uint8_t drive_count;
    uint32_t timestamp;
} MetricPacket;
#pragma pack(pop)

static MetricPacket rx_packet;
static bool packet_ready = false;

static bool splash_active = true;
static uint32_t last_packet_time = 0;
static uint32_t last_tab_switch = 0;
static uint8_t active_tab = 0;

static inline int y_pos(int i) {
    return MARGIN_Y + i * (LABEL_HEIGHT + PROGRESS_HEIGHT + VERTICAL_SPACING);
}

void receive_packets() {
    static uint8_t buffer[sizeof(MetricPacket)];
    static uint16_t index = 0;
    static enum { SYNC_AA, SYNC_55, PAYLOAD } state = SYNC_AA;

    while (Serial.available()) {
        uint8_t b = Serial.read();

        switch (state) {
            case SYNC_AA:
                if (b == 0xAA) {
                    buffer[0] = b;
                    index = 1;
                    state = SYNC_55;
                }
                break;

            case SYNC_55:
                if (b == 0x55) {
                    buffer[1] = b;
                    index = 2;
                    state = PAYLOAD;
                } else {
                    state = SYNC_AA;
                }
                break;

            case PAYLOAD:
                buffer[index++] = b;
                if (index >= sizeof(MetricPacket)) {
                    memcpy(&rx_packet, buffer, sizeof(MetricPacket));
                    packet_ready = true;
                    last_packet_time = millis();
                    index = 0;
                    state = SYNC_AA;
                }
                break;
        }
    }
}

void create_splash() {
    splash_label = (AromaLabel*) aroma_label_create(
        (AromaNode*) window,
        "Waiting for connection...",
        120,
        WINDOW_HEIGHT / 2 - 15,
        LABEL_STYLE_LABEL_LARGE
    );
}

void create_cpu_content() {
    cpu_label = aroma_label_create((AromaNode*)window, "CPU Usage: 0%", MARGIN_X, y_pos(0), LABEL_STYLE_LABEL_LARGE);
    cpu_usage = aroma_progressbar_create((AromaNode*)window, MARGIN_X, y_pos(0) + LABEL_HEIGHT, WIDGET_WIDTH, PROGRESS_HEIGHT, PROGRESS_TYPE_DETERMINATE);

    ram_label = aroma_label_create((AromaNode*)window, "RAM Usage: 0%", MARGIN_X, y_pos(1), LABEL_STYLE_LABEL_LARGE);
    ram_usage = aroma_progressbar_create((AromaNode*)window, MARGIN_X, y_pos(1) + LABEL_HEIGHT, WIDGET_WIDTH, PROGRESS_HEIGHT, PROGRESS_TYPE_DETERMINATE);

    cpu_icon = aroma_image_create_from_memory(
        (AromaNode*)window,
        (unsigned char*) cpu_icon_raw,
        cpu_icon_raw_len,
        WINDOW_WIDTH - MARGIN_X - 96,
        WINDOW_HEIGHT / 2 - 32,
        64,
        64
    );
}

void create_gpu_content() {
    gpu_fan_label = aroma_label_create((AromaNode*)window, "GPU Fan: 0%", MARGIN_X, y_pos(0), LABEL_STYLE_LABEL_LARGE);
    gpu_fan_speed = aroma_progressbar_create((AromaNode*)window, MARGIN_X, y_pos(0) + LABEL_HEIGHT, WIDGET_WIDTH, PROGRESS_HEIGHT, PROGRESS_TYPE_DETERMINATE);

    gpu_util_label = aroma_label_create((AromaNode*)window, "GPU Usage: 0%", MARGIN_X, y_pos(1), LABEL_STYLE_LABEL_LARGE);
    gpu_utilisation = aroma_progressbar_create((AromaNode*)window, MARGIN_X, y_pos(1) + LABEL_HEIGHT, WIDGET_WIDTH, PROGRESS_HEIGHT, PROGRESS_TYPE_DETERMINATE);

    gpu_temp_label = aroma_label_create((AromaNode*)window, "GPU Temp: 0C", MARGIN_X, y_pos(2), LABEL_STYLE_LABEL_LARGE);
    gpu_temp_usage = aroma_progressbar_create((AromaNode*)window, MARGIN_X, y_pos(2) + LABEL_HEIGHT, WIDGET_WIDTH, PROGRESS_HEIGHT, PROGRESS_TYPE_DETERMINATE);

    gpu_icon = aroma_image_create_from_memory(
        (AromaNode*)window,
        (unsigned char*) video_card_raw,
        video_card_raw_len,
        WINDOW_WIDTH - MARGIN_X - 96,
        WINDOW_HEIGHT / 2 - 32,
        64,
        64
    );

}

void create_disk_content() {
    disk_icon = aroma_image_create_from_memory(
        (AromaNode*)window,
        (unsigned char*) drive_icon_raw,
        drive_icon_raw_len,
        WINDOW_WIDTH / 2 - 32,
        y_pos(0),
        64,
        64
    );

    for (int i = 0; i < 2; i++) {
        drive_labels[i] = aroma_label_create(
            (AromaNode*)window,
            "--",
            MARGIN_X,
            y_pos(i + 1) + 30,
            LABEL_STYLE_LABEL_LARGE
        );
    }
}

void show_splash(bool show) {
    aroma_node_set_hidden((AromaNode*) splash_label, !show);
    aroma_node_set_hidden(tabs, show);
}

void update_drives() {
    char buf[64];
    int count = rx_packet.drive_count;
    if (count > MAX_DRIVE_LABELS) count = MAX_DRIVE_LABELS;

    for (int i = 0; i < 2; i++) {
        float used_gb = rx_packet.drive_total[i] - rx_packet.drive_free[i];
        snprintf(
            buf,
            sizeof(buf),
            "%c: %.0f%% (%.2fGB / %.2fGB)",
            rx_packet.drive_letters[i],
            rx_packet.drive_used[i],
            used_gb,
            rx_packet.drive_total[i]
        );
        aroma_label_set_text(drive_labels[i], buf);
    }

}

void update_ui() {
    if (!packet_ready) return;

    aroma_progressbar_set_progress(cpu_usage, rx_packet.cpu / 100.0f);
    aroma_progressbar_set_progress(ram_usage, rx_packet.ram / 100.0f);
    aroma_progressbar_set_progress(gpu_fan_speed, rx_packet.gpu_fan / 100.0f);
    aroma_progressbar_set_progress(gpu_utilisation, rx_packet.gpu_util / 100.0f);
    aroma_progressbar_set_progress(gpu_temp_usage, rx_packet.gpu_temp / 100.0f);
    char buf[48];
    snprintf(buf, sizeof(buf), "CPU Usage: %.1f%%", rx_packet.cpu);
    aroma_label_set_text(cpu_label, buf);

    snprintf(buf, sizeof(buf), "RAM Usage: %.1f%%", rx_packet.ram);
    aroma_label_set_text(ram_label, buf);

    snprintf(buf, sizeof(buf), "GPU Fan: %.1f%%", rx_packet.gpu_fan);
    aroma_label_set_text(gpu_fan_label, buf);

    snprintf(buf, sizeof(buf), "GPU Usage: %.1f%%", rx_packet.gpu_util);
    aroma_label_set_text(gpu_util_label, buf);

    snprintf(buf, sizeof(buf), "GPU Temp: %.1fC", rx_packet.gpu_temp);
    aroma_label_set_text(gpu_temp_label, buf);
    
    update_drives();
    aroma_node_invalidate(tabs);
    aroma_node_invalidate(disk_icon);
    aroma_node_invalidate(gpu_icon);
    aroma_node_invalidate(cpu_icon);
    packet_ready = false;
}

void setup() {
    Serial.begin(9600);
    aroma_ui_init();

    AromaTheme theme = aroma_theme_create_material_black();
    theme.colors.primary = 0xFF0000;
    aroma_ui_set_theme(&theme);

    AromaFont* font = aroma_font_create("FreeSans12pt7b", 12);

    window = aroma_ui_create_window("System Monitor", WINDOW_WIDTH, WINDOW_HEIGHT);
    aroma_event_set_root((AromaNode*) window);

    create_cpu_content();
    create_gpu_content();
    create_disk_content();

    const char* labels[] = {"CPU", "GPU", "Disk"};
    tabs = aroma_tabs_create((AromaNode*)window, 0, 0, WINDOW_WIDTH, 50, labels, 3);
    aroma_tabs_set_font(tabs, font);

    AromaNode* cpu_nodes[]  = {cpu_label, cpu_usage, ram_label, ram_usage, cpu_icon};
 
    AromaNode* gpu_nodes[]  = {gpu_fan_label, gpu_fan_speed, gpu_util_label, gpu_utilisation, gpu_temp_label, gpu_temp_usage, gpu_icon};

    AromaNode* disk_nodes[1 + MAX_DRIVE_LABELS];
    disk_nodes[0] = disk_icon;
    for (int i = 0; i < MAX_DRIVE_LABELS; i++) {
        disk_nodes[i + 1] = drive_labels[i];
    }

    aroma_tabs_set_content(tabs, 0, cpu_nodes, 5);
    aroma_tabs_set_content(tabs, 1, gpu_nodes, 7);
    aroma_tabs_set_content(tabs, 2, disk_nodes, 1 + MAX_DRIVE_LABELS);
   //workaround for initial visibility bug
    aroma_node_set_hidden((AromaNode*) cpu_label, true);
    aroma_node_set_hidden((AromaNode*) cpu_usage, true);
    aroma_node_set_hidden((AromaNode*) ram_label, true);
    aroma_node_set_hidden((AromaNode*) ram_usage, true);
    aroma_node_set_hidden((AromaNode*) cpu_icon, true);
    create_splash();
    show_splash(true);
    aroma_ui_request_redraw(NULL);
}

void loop() {
    receive_packets();
    uint32_t now = millis();

    if (splash_active) {
        if (packet_ready) {
            splash_active = false;
            show_splash(false);
            last_tab_switch = now;
        }
    } else {
        if (now - last_packet_time > SPLASH_TIMEOUT_MS) {
            splash_active = true;
            show_splash(true);
        } else {
            update_ui();

            if (now - last_tab_switch >= TAB_CYCLE_MS) {
                active_tab = (active_tab + 1) % 3;
                aroma_tabs_set_selected(tabs, active_tab);
                last_tab_switch = now;
            }
        }
    }

    aroma_ui_process_events();
    aroma_ui_render(window);
}
