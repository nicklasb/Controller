/*********************
 *      This is the user interface task.
 *      Basically, this is where the main UI loop is initiated and performed.
 *********************/


/*********************
 *      INCLUDES
 *********************/
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "ui_task.h"
#include "ui_builder.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
lv_color_t *buf1;
/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void ui_task(void *pvParameter);

void ui_init(const char *log_prefix)
{
    strcpy(ui_tag,log_prefix);
    strupr(ui_tag);
    strcat(ui_tag, "_UI_TASK\0");

    char taskname[35] = "\0";
    strcpy(taskname, log_prefix);
    strcat(taskname, " UI task");
    ESP_LOGI(ui_tag, "Initialising UI task, name: %s", taskname);

    // Create pinned GUI-task to avoid concurrency issues, Core 0 because it is not Bluetooth or Wifi
    xTaskCreatePinnedToCore(ui_task, taskname, 8192, NULL, 0, NULL, 0);
}

static void init_task(void *pvParameter)
{

    (void)pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    /* Allocate the screen buffer */
    buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Do not double buffered when not working with multicolor displays */
    static lv_color_t *buf2 = NULL;

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

    /* Initialize the working buffer depending on the selected display. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    /* Initialize the display driver */
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /* Register the function that flushes the buffer to the driver */
    disp_drv.flush_cb = disp_driver_flush;

    /* Register the display buffer */
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register the touch controller */

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
}

/* This is the main GUI task */
static void ui_task(void *pvParameter)
{

    /* All initialization has been move out */

    init_task(pvParameter);

    /* Create the User interface */
    build_ui();

    int loop_count = 0;

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }

        if (loop_count > 100)
        {
            if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
            {
                //lv_label_set_text_fmt(vberth, "I just set this");
                // lv_label_set_text_fmt(vberth, "Hum: %.1f Tmp: %.1f\n", hum, temp);
                // ESP_LOGI("UI update",  "Hum: %.1f Tmp: %.1f\n", hum, temp);
                xSemaphoreGive(xGuiSemaphore);
                loop_count = 0;
            }
            else
            {
                ESP_LOGE("UI update", "Failed to get a semaphore!");
            }
        }
        loop_count++;
    }

    /* A task should NEVER return */
    /* Only free buf1, buf2 isn't used */
    free(buf1);

    /* The task deletes itself
    It is just the initialisation task, should only be run once.
    Passing NULL means delete the calling task.
    */
    vTaskDelete(NULL);
}

static void lv_tick_task(void *arg)
{
    (void)arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}