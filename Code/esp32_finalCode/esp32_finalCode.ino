#include <crabV4_inferencing.h>

#include "esp_camera.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include <EEPROM.h>            // read and write from flash memory


// define the number of bytes you want to access
#define EEPROM_SIZE 1

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
uint8_t *snapshot_buf; //points to the output of the capture
uint8_t crabCount = 0;
uint8_t oldCrabCount = 255;
struct Coordinate{
  uint8_t xas;
  uint8_t yas;
};
//I never expect too see more than 16 crabs in one image, change this value if tests prove otherwise
Coordinate oldCentroids[16];
Coordinate centroids[16];

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) ;

void setup() {
   Serial.begin(115200);
   
  //pin12 output
  pinMode(12, OUTPUT);
  
  // Init Camera
  ei_camera_init();

  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  
  esp_sleep_enable_timer_wakeup(300000);
}

void loop() {
 delay(1);
      crabCount = 0;
      snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
  
      // check if allocation was successful
      if(snapshot_buf == nullptr) {
          ei_printf("ERR: Failed to allocate snapshot buffer!\n");
          return;
      }
  
      ei::signal_t signal;
      signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
      signal.get_data = &ei_camera_get_data;

      // Turns on the ir leds connected to GPIO 12
      digitalWrite(12, HIGH);
      //unfortunatly it seems like this delay is neccesary otherwise the leds are not on when the picture is taken
      delay(100);
  
      if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
          ei_printf("Failed to capture image\r\n");
          free(snapshot_buf);
          return;
      }
      
      digitalWrite(12, LOW);

      // Run the classifier
      ei_impulse_result_t result = { 0 };
  
      EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
      if (err != EI_IMPULSE_OK) {
          ei_printf("ERR: Failed to run classifier (%d)\n", err);
          return;
      }
  
      // print the predictions
      ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                  result.timing.dsp, result.timing.classification, result.timing.anomaly);
  
      bool bb_found = result.bounding_boxes[0].value > 0;
      for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
          auto bb = result.bounding_boxes[ix];
          if (bb.value == 0) {
              continue;
          }
          crabCount++;
          ei_printf("    %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\n", bb.label, bb.value, bb.x, bb.y, bb.width, bb.height);
          centroids[ix].xas = bb.x;
          centroids[ix].yas = bb.y;
      }
      trackAndCount();
      if (!bb_found) {
          free(snapshot_buf);
          ei_printf("    No objects found this capture, ending cycle\n");
          ei_printf("    Total crab Count now %u\n", EEPROM.read(0));
          esp_deep_sleep_start();
      }
      oldCrabCount = crabCount;
      for (uint8_t i = 0; i < 16; i++){
        oldCentroids[i] = centroids[i];
      }
      free(snapshot_buf);
}

bool ei_camera_init(){
    if(is_initialised) return true;
    
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1); // flip it back
      s->set_brightness(s, 1); // up the brightness just a bit
      s->set_saturation(s, 0); // lower the saturation
    }

    
    is_initialised = true;
    return true;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }


    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 2];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}


void trackAndCount() {
  //Dont track on first image
  if (oldCrabCount == 255){
    oldCrabCount = crabCount;
    return;
  }

  //Seperate code for end of detection cycle
  if (crabCount == 0){
    ei_printf("    tracker: %u crabs left FOV\n", oldCrabCount - crabCount);
    uint8_t crabsToCount = 0;
    for(uint8_t i = 0; i < oldCrabCount; i++){
      if (oldCentroids[i].xas > 43){
        crabsToCount++;
      }
    }
    ei_printf("    tracker: %u crabs crossed on counting side of FOV\n", crabsToCount);
    uint8_t oldCount = EEPROM.read(0);
    ei_printf("    tracker: %u crabs did not cross imaginary line and are not counted\n", oldCrabCount - crabsToCount);
    EEPROM.write(0, oldCount + crabsToCount);
    EEPROM.commit();
    return;
  }
  
  int matrix[16][16];

  //generate distance matrix
  for (uint8_t i = 0; i < oldCrabCount; i++){
    matrix[i][crabCount] = 0;
    for (uint8_t j = 0; j < crabCount; j++){
      int distance = sqrt(pow(oldCentroids[i].xas - centroids[j].xas, 2) + pow(oldCentroids[i].yas - centroids[j].yas, 2));
      //0 messes up the branch and bound algorithm
      distance == 0 ? 1 : distance;
      matrix[i][j] = distance;
    }
  }
  ei_printf("starnge: %u", matrix[0][crabCount+1]);
  findMinCost(matrix);
}
