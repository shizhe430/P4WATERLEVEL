/*
 * ESP32-P4 port of the STM32 APP_MODE_XCAM_VIEW camera path.
 */

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_cam_ctlr_dvp_ext.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "example_video_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define XCAM_WIDTH                 320U
#define XCAM_HEIGHT                240U
#define XCAM_BUFFER_COUNT          2U
#define XCAM_BAD_FRAME_THRESHOLD   0U
#define XCAM_LOG_EVERY_FRAMES      30U
#define XCAM_LOG_BAD_EVERY_FRAMES  100U
#define XCAM_OV2640_WRITE_ADDRESS  0x60U
#define XCAM_OV2640_READ_ADDRESS   0x61U
#define XCAM_OV2640_EXPECTED_MID   0x7fa2U
#define XCAM_OV2640_EXPECTED_PID   0x2642U
#define XCAM_OV2640_PROBE_ATTEMPTS 3U

static const char *TAG = "xcam_view";

#define XCAM_SCCB_DELAY_US  10U

static void xcam_sccb_delay(void)
{
    esp_rom_delay_us(XCAM_SCCB_DELAY_US);
}

static void xcam_sccb_start(void)
{
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
}

static void xcam_sccb_stop(void)
{
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
}

static void xcam_sccb_write_byte(uint8_t value)
{
    for (uint32_t bit = 0U; bit < 8U; bit++) {
        gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP,
                       (value & 0x80U) != 0U);
        xcam_sccb_delay();
        gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
        xcam_sccb_delay();
        gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
        value <<= 1U;
    }

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
}

static uint8_t xcam_sccb_read_byte(void)
{
    uint8_t value = 0U;

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    for (uint32_t bit = 0U; bit < 8U; bit++) {
        value <<= 1U;
        xcam_sccb_delay();
        gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
        value |= (uint8_t)gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP);
        xcam_sccb_delay();
        gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
    }

    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    return value;
}

static void xcam_sccb_write_reg(uint8_t reg, uint8_t value)
{
    xcam_sccb_start();
    xcam_sccb_write_byte(XCAM_OV2640_WRITE_ADDRESS);
    xcam_sccb_write_byte(reg);
    xcam_sccb_write_byte(value);
    xcam_sccb_stop();
}

static uint8_t xcam_sccb_read_reg(uint8_t reg)
{
    uint8_t value;

    xcam_sccb_start();
    xcam_sccb_write_byte(XCAM_OV2640_WRITE_ADDRESS);
    xcam_sccb_write_byte(reg);
    xcam_sccb_stop();

    xcam_sccb_start();
    xcam_sccb_write_byte(XCAM_OV2640_READ_ADDRESS);
    value = xcam_sccb_read_byte();
    xcam_sccb_stop();
    return value;
}

static void xcam_sccb_log_line_test(void)
{
    int release_scl;
    int release_sda;
    int scl_low_scl;
    int scl_low_sda;
    int sda_low_scl;
    int sda_low_sda;

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    release_scl = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP);
    release_sda = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP);

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    scl_low_scl = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP);
    scl_low_sda = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP);

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 0);
    xcam_sccb_delay();
    sda_low_scl = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP);
    sda_low_sda = gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP);

    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();
    ESP_LOGI(TAG, "SCCB line test: release=%d%d SCL-low=%d%d SDA-low=%d%d expected=11/01/10",
             release_scl, release_sda, scl_low_scl, scl_low_sda,
             sda_low_scl, sda_low_sda);
}

static void xcam_log_dvp_activity(void)
{
    const gpio_config_t input_config = {
        .pin_bit_mask = BIT64(CONFIG_EXAMPLE_DVP_PCLK_PIN) |
                        BIT64(CONFIG_EXAMPLE_DVP_VSYNC_PIN) |
                        BIT64(CONFIG_EXAMPLE_DVP_DE_PIN),
        .mode = GPIO_MODE_INPUT,
    };
    uint32_t pclk_edges = 0U;
    uint32_t vsync_edges = 0U;
    uint32_t href_edges = 0U;
    int previous_pclk;
    int previous_vsync;
    int previous_href;

    if (gpio_config(&input_config) != ESP_OK) {
        ESP_LOGE(TAG, "DVP activity GPIO setup failed");
        return;
    }

    previous_pclk = gpio_get_level(CONFIG_EXAMPLE_DVP_PCLK_PIN);
    previous_vsync = gpio_get_level(CONFIG_EXAMPLE_DVP_VSYNC_PIN);
    previous_href = gpio_get_level(CONFIG_EXAMPLE_DVP_DE_PIN);
    const int64_t end_time = esp_timer_get_time() + 20000;

    while (esp_timer_get_time() < end_time) {
        const int pclk = gpio_get_level(CONFIG_EXAMPLE_DVP_PCLK_PIN);
        const int vsync = gpio_get_level(CONFIG_EXAMPLE_DVP_VSYNC_PIN);
        const int href = gpio_get_level(CONFIG_EXAMPLE_DVP_DE_PIN);

        pclk_edges += (pclk != previous_pclk);
        vsync_edges += (vsync != previous_vsync);
        href_edges += (href != previous_href);
        previous_pclk = pclk;
        previous_vsync = vsync;
        previous_href = href;
    }

    ESP_LOGI(TAG, "DVP activity 20ms: PCLK=%" PRIu32 " VSYNC=%" PRIu32 " HREF=%" PRIu32,
             pclk_edges, vsync_edges, href_edges);
    gpio_reset_pin(CONFIG_EXAMPLE_DVP_PCLK_PIN);
    gpio_reset_pin(CONFIG_EXAMPLE_DVP_VSYNC_PIN);
    gpio_reset_pin(CONFIG_EXAMPLE_DVP_DE_PIN);
}

static void xcam_log_dvp_activity_runtime(void)
{
    uint32_t pclk_edges = 0U;
    uint32_t vsync_edges = 0U;
    uint32_t href_edges = 0U;
    int previous_pclk;
    int previous_vsync;
    int previous_href;

    previous_pclk = gpio_get_level(CONFIG_EXAMPLE_DVP_PCLK_PIN);
    previous_vsync = gpio_get_level(CONFIG_EXAMPLE_DVP_VSYNC_PIN);
    previous_href = gpio_get_level(CONFIG_EXAMPLE_DVP_DE_PIN);

    const int64_t end_time = esp_timer_get_time() + 20000;

    while (esp_timer_get_time() < end_time) {
        const int pclk = gpio_get_level(CONFIG_EXAMPLE_DVP_PCLK_PIN);
        const int vsync = gpio_get_level(CONFIG_EXAMPLE_DVP_VSYNC_PIN);
        const int href = gpio_get_level(CONFIG_EXAMPLE_DVP_DE_PIN);

        pclk_edges += (pclk != previous_pclk);
        vsync_edges += (vsync != previous_vsync);
        href_edges += (href != previous_href);

        previous_pclk = pclk;
        previous_vsync = vsync;
        previous_href = href;
    }

    ESP_LOGI(TAG, "DVP runtime activity 20ms: PCLK=%" PRIu32 " VSYNC=%" PRIu32 " HREF=%" PRIu32,
             pclk_edges, vsync_edges, href_edges);
}

static void xcam_probe_sccb(void)
{
    const gpio_config_t control_config = {
        .pin_bit_mask = BIT64(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN) |
                        BIT64(CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    const gpio_config_t sccb_config = {
        .pin_bit_mask = BIT64(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP) |
                        BIT64(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t ret;

    ESP_LOGI(TAG, "ST camera sequence: SCL=%d SDA=%d RESET=%d PWDN=%d onboard XCLK",
             CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP,
             CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP,
             CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,
             CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN);

    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN, 1);
    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 0);
    ret = gpio_config(&control_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB diagnostic GPIO setup failed: %s", esp_err_to_name(ret));
        return;
    }

    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 0);
    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(30U));
    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(80U));

    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(30U));
    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(80U));
    vTaskDelay(pdMS_TO_TICKS(150U));

    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(30U));
    gpio_set_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(80U));
    vTaskDelay(pdMS_TO_TICKS(150U));

    ret = gpio_config(&sccb_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB diagnostic GPIO bus setup failed: %s", esp_err_to_name(ret));
        goto cleanup_gpio;
    }
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP, 1);
    gpio_set_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP, 1);
    xcam_sccb_delay();

    ESP_LOGI(TAG, "SCCB active levels: SCL=%d SDA=%d RESET=%d PWDN=%d",
             gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP),
             gpio_get_level(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP),
             gpio_get_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN),
             gpio_get_level(CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN));

    xcam_sccb_log_line_test();

    for (uint32_t attempt = 1U; attempt <= XCAM_OV2640_PROBE_ATTEMPTS; attempt++) {
        uint8_t mid_high;
        uint8_t mid_low;
        uint8_t pid_high;
        uint8_t pid_low;
        uint16_t mid;
        uint16_t pid;

        xcam_sccb_write_reg(0xffU, 0x01U);
        xcam_sccb_write_reg(0x12U, 0x80U);
        vTaskDelay(pdMS_TO_TICKS(50U));

        xcam_sccb_write_reg(0xffU, 0x01U);
        mid_high = xcam_sccb_read_reg(0x1cU);
        mid_low = xcam_sccb_read_reg(0x1dU);
        pid_high = xcam_sccb_read_reg(0x0aU);
        pid_low = xcam_sccb_read_reg(0x0bU);
        mid = ((uint16_t)mid_high << 8U) | mid_low;
        pid = ((uint16_t)pid_high << 8U) | pid_low;

        ESP_LOGI(TAG, "ST SCCB probe attempt=%" PRIu32 " MID=0x%04x PID=0x%04x",
                 attempt, mid, pid);
        if ((mid == XCAM_OV2640_EXPECTED_MID) &&
            (pid == XCAM_OV2640_EXPECTED_PID)) {
            ESP_LOGI(TAG, "OV2640 SCCB identification passed");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(150U));
    }

    xcam_log_dvp_activity();

    gpio_reset_pin(CONFIG_EXAMPLE_SCCB_I2C_SCL_PIN_INIT_BY_APP);
    gpio_reset_pin(CONFIG_EXAMPLE_SCCB_I2C_SDA_PIN_INIT_BY_APP);
cleanup_gpio:
    return;
}

static bool xcam_jpeg_is_valid(const uint8_t *data, size_t length)
{
    if ((data == NULL) || (length < 4U)) {
        return false;
    }

    return (data[0] == 0xffU) && (data[1] == 0xd8U) &&
           (data[length - 2U] == 0xffU) && (data[length - 1U] == 0xd9U);
}

#if CONFIG_XCAM_RAW_JPEG_STREAM
static esp_err_t xcam_uart_output_init(void)
{
    esp_err_t ret = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                                        16 * 1024,
                                        0,
                                        0,
                                        NULL,
                                        0);
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        return ret;
    }

    ESP_RETURN_ON_ERROR(uart_set_baudrate(CONFIG_ESP_CONSOLE_UART_NUM,
                                          CONFIG_ESP_CONSOLE_UART_BAUDRATE),
                        TAG, "set UART baudrate failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM,
                                     CONFIG_ESP_CONSOLE_UART_TX_GPIO,
                                     CONFIG_ESP_CONSOLE_UART_RX_GPIO,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE),
                        TAG, "set UART pins failed");
    return ESP_OK;
}

static bool xcam_write_all(const uint8_t *data, size_t length)
{
    size_t offset = 0U;

    while (offset < length) {
        int written = uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM,
                                       data + offset,
                                       length - offset);
        if (written <= 0) {
            return false;
        }
        offset += (size_t)written;
    }

    return true;
}
#endif

static esp_err_t xcam_capture_jpeg_stream(void)
{
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint8_t *buffers[XCAM_BUFFER_COUNT] = {0};
    size_t buffer_lengths[XCAM_BUFFER_COUNT] = {0};
    struct v4l2_requestbuffers request = {
        .count = XCAM_BUFFER_COUNT,
        .type = type,
        .memory = V4L2_MEMORY_MMAP,
    };
    struct v4l2_format format = {
        .type = type,
        .fmt.pix.width = XCAM_WIDTH,
        .fmt.pix.height = XCAM_HEIGHT,
        .fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG,
    };
    uint32_t good_frames = 0U;
    uint32_t bad_frames = 0U;
    uint32_t consecutive_bad_frames = 0U;
    esp_err_t ret = ESP_FAIL;
    bool stream_started = false;
    int fd = -1;

    fd = open(EXAMPLE_CAM_DEV_PATH, O_RDONLY);
    ESP_GOTO_ON_FALSE(fd >= 0, ESP_FAIL, cleanup, TAG,
                      "open %s failed", EXAMPLE_CAM_DEV_PATH);

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_S_FMT, &format) == 0,
                      ESP_FAIL, cleanup, TAG, "set QVGA JPEG format failed");
    ESP_GOTO_ON_FALSE((format.fmt.pix.width == XCAM_WIDTH) &&
                      (format.fmt.pix.height == XCAM_HEIGHT) &&
                      (format.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG),
                      ESP_ERR_NOT_SUPPORTED, cleanup, TAG,
                      "driver selected format=%" PRIu32 " size=%" PRIu32 "x%" PRIu32,
                      format.fmt.pix.pixelformat,
                      format.fmt.pix.width,
                      format.fmt.pix.height);

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_REQBUFS, &request) == 0,
                      ESP_FAIL, cleanup, TAG, "request buffers failed");
    ESP_GOTO_ON_FALSE(request.count >= XCAM_BUFFER_COUNT,
                      ESP_ERR_NO_MEM, cleanup, TAG,
                      "driver returned only %" PRIu32 " buffers", request.count);

    for (uint32_t i = 0U; i < XCAM_BUFFER_COUNT; i++) {
        struct v4l2_buffer buffer = {
            .type = type,
            .memory = V4L2_MEMORY_MMAP,
            .index = i,
        };

        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QUERYBUF, &buffer) == 0,
                          ESP_FAIL, cleanup, TAG,
                          "query buffer %" PRIu32 " failed", i);
        buffers[i] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, buffer.m.offset);
        ESP_GOTO_ON_FALSE(buffers[i] != MAP_FAILED,
                          ESP_ERR_NO_MEM, cleanup, TAG,
                          "map buffer %" PRIu32 " failed", i);
        buffer_lengths[i] = buffer.length;
        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0,
                          ESP_FAIL, cleanup, TAG,
                          "queue buffer %" PRIu32 " failed", i);
    }

    ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_STREAMON, &type) == 0,
                      ESP_FAIL, cleanup, TAG, "stream start failed");
    stream_started = true;
    ESP_LOGI(TAG, "OV2640 stream started: 320x240 JPEG, onboard 24MHz XCLK");
    xcam_log_dvp_activity_runtime();
#if CONFIG_XCAM_RAW_JPEG_STREAM
    ESP_LOGI(TAG, "switching console UART to raw JPEG output");
    ESP_GOTO_ON_ERROR(xcam_uart_output_init(), cleanup, TAG,
                      "raw JPEG UART init failed");
    esp_log_level_set("*", ESP_LOG_NONE);
#endif

    while (true) {
        struct v4l2_buffer buffer = {
            .type = type,
            .memory = V4L2_MEMORY_MMAP,
        };
        bool valid;

        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_DQBUF, &buffer) == 0,
                          ESP_FAIL, cleanup, TAG, "dequeue frame failed");
        ESP_GOTO_ON_FALSE(buffer.index < XCAM_BUFFER_COUNT,
                          ESP_ERR_INVALID_RESPONSE, cleanup, TAG,
                          "invalid buffer index=%" PRIu32, buffer.index);

        valid = ((buffer.flags & V4L2_BUF_FLAG_DONE) != 0U) &&
                ((buffer.flags & V4L2_BUF_FLAG_ERROR) == 0U) &&
                xcam_jpeg_is_valid(buffers[buffer.index], buffer.bytesused);

        if (valid) {
#if CONFIG_XCAM_RAW_JPEG_STREAM
            valid = xcam_write_all(buffers[buffer.index], buffer.bytesused);
            if (!valid) {
                ESP_LOGE(TAG, "raw JPEG UART write failed: bytes=%" PRIu32, buffer.bytesused);
            }
#endif
        }

        if (valid) {
            good_frames++;
            consecutive_bad_frames = 0U;
#if !CONFIG_XCAM_RAW_JPEG_STREAM
            if ((good_frames % XCAM_LOG_EVERY_FRAMES) == 0U) {
                ESP_LOGI(TAG,
                         "JPEG ok: frame=%" PRIu32 " bytes=%" PRIu32
                         " total_bad=%" PRIu32,
                         good_frames, buffer.bytesused, bad_frames);
            }
#endif
        } else {
            bad_frames++;
            consecutive_bad_frames++;
#if !CONFIG_XCAM_RAW_JPEG_STREAM
            if ((bad_frames % XCAM_LOG_BAD_EVERY_FRAMES) == 0U) {
            ESP_LOGW(TAG,
                     "bad JPEG: seq=%" PRIu32 " bytes=%" PRIu32
                     " flags=0x%" PRIx32 " consecutive=%" PRIu32
                     " time=%lld.%06lld",
                     buffer.sequence, buffer.bytesused, buffer.flags,
                     consecutive_bad_frames,
                     (long long)buffer.timestamp.tv_sec,
                     (long long)buffer.timestamp.tv_usec);
            }
#endif
        }

        ESP_GOTO_ON_FALSE(ioctl(fd, VIDIOC_QBUF, &buffer) == 0,
                          ESP_FAIL, cleanup, TAG, "requeue frame failed");

#if XCAM_BAD_FRAME_THRESHOLD > 0U
        if (consecutive_bad_frames >= XCAM_BAD_FRAME_THRESHOLD) {
            ESP_LOGE(TAG, "two consecutive invalid frames; restarting video stack");
            ret = ESP_ERR_INVALID_RESPONSE;
            goto cleanup;
        }
#endif
    }

cleanup:
    if (stream_started) {
        (void)ioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    for (uint32_t i = 0U; i < XCAM_BUFFER_COUNT; i++) {
        if ((buffers[i] != NULL) && (buffers[i] != MAP_FAILED)) {
            (void)munmap(buffers[i], buffer_lengths[i]);
        }
    }
    if (fd >= 0) {
        close(fd);
    }
    return ret;
}

void app_main(void)
{
    xcam_probe_sccb();

    while (true) {
        esp_err_t ret = example_video_init();

        if (ret == ESP_OK) {
            ret = xcam_capture_jpeg_stream();
            (void)example_video_deinit();
        }

        ESP_LOGE(TAG, "camera cycle failed: %s; retrying in 1 second",
                 esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
