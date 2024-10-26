#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

size_t writeBuf(fs::FS &fs, const char *path, const uint8_t *buf, const size_t len);

#define PWR_DWN_PIN 17

static camera_config_t camera_example_config = {
        .pin_pwdn       = PWR_DWN_PIN,
        .pin_reset      = 18,
        // If pin < 0 then clock is disabled
        .pin_xclk       = 0,
        // I2C SDA and SCL
        .pin_sccb_sda   = 8,
        .pin_sccb_scl   = 9,

        // Data pins, for some reason breakout labels these D9-2
        .pin_d7         = 33,
        .pin_d6         = 38,
        .pin_d5         = 1,
        .pin_d4         = 3,
        .pin_d3         = 7,
        .pin_d2         = 10,
        .pin_d1         = 11,
        .pin_d0         = 14,

        // Vertical and horizontal sync, high when a new frame and row of data begins
        .pin_vsync      = 6,
        .pin_href       = 12,

        // Pixel clock, used to indicate when new data is available
        .pin_pclk       = 5,

        // Clock freq used by camera, generated at 24MHz
        .xclk_freq_hz   = 24000000,

        // LEDC timers used to gen camera clock signal, unused here but should be filled with
        // these enums to prevent compiler complaining
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,

        // Format and framesize settings, not sure if jpeg quality has any effect outside of JPEG mode
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_HD,
        .jpeg_quality   = 20,

        .fb_count       = 2,

        .fb_location    = CAMERA_FB_IN_PSRAM,
        /* 'When buffers should be filled' <- Not sure what this means */
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
    };

static const int miso = 37;
static const int mosi = 35;
static const int sclk = 36;
static const int cs = 44;

void setup() {
  pinMode(PWR_DWN_PIN, OUTPUT);

  Serial.begin(115200);
  while(!Serial){delay(10);}
  Serial.println("Alive");

  if (psramFound())
  {
    Serial.println("PSRAM present");
  }
  else
  {
    Serial.println("PSRAM not present");
  }

  // Probe for cameras
  camera_model_t type;
  esp_err_t err = camera_probe(&config, &type);

  while (err != ESP_OK)
  {
    Serial.println("!Camera probe failed!");
    Serial.println("Re-probing...");
    delay(100);
  }

  err = esp_camera_init(&camera_example_config);
  if (err != ESP_OK)
  {
    Serial.println("!Camera setup failed!");
    digitalWrite(PWR_DWN_PIN, HIGH);
    return;
  }

  Serial.println("Camera setup sucsessful");

  sensor_t * sensor = esp_camera_sensor_get();
  camera_sensor_info_t * info = esp_camera_sensor_get_info(&sensor->id);

  Serial.print("Camera name is: ");
  Serial.println(info->name);

  //capture a frame
  Serial.println("Capturing frame...");
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("Frame buffer could not be acquired");
      // Pull camera power-off pin high to prevent heating
      digitalWrite(PWR_DWN_PIN, HIGH);
      return;
  }
  //replace this with your own function
  Serial.print("Image dimensions: ");
  Serial.print("Width: "); Serial.println(fb->width);
  Serial.print("Height: "); Serial.println(fb->height);
  Serial.print("Len: "); Serial.println(fb->len);

  Serial.println("Attempting to start SPI to SD card");

  SPI.begin(sclk, miso, mosi, cs);
  if (!SD.begin(cs))
  {
    Serial.println("!Failed to mount SD, skipping write!");
  }
  else
  {
    Serial.println("SD card mounted");
    writeBuf(SD, "/image", fb->buf, fb->len);
  }
  //return the frame buffer back to be reused
  esp_camera_fb_return(fb);

  err = esp_camera_deinit();

  // Pull camera power-off pin high to prevent heating
  digitalWrite(PWR_DWN_PIN, HIGH);
  
  if (err != ESP_OK)
  {
    Serial.println("!camera deinit failed!");
    Serial.println(esp_err_to_name(err));
  }
  else
  {
    Serial.println("Camera deinit sucsessful");
  }
}


void loop() {
  // nothing happens after setup

}


size_t writeBuf(fs::FS &fs, const char *path, const uint8_t *buf, const size_t len) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return 0;
  }

  size_t len_written = file.write(buf, len);
  Serial.print(len); Serial.println(" bytes written");

  file.close();
  return len_written;
}
