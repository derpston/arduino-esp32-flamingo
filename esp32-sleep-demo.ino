#include <Audio.h>
#include "FS.h"
#include "SPIFFS.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true


// Digital I/O used
#define I2S_DOUT      25  // DIN connection
#define I2S_BCLK      27  // Bit clock
#define I2S_LRC       26  // Left Right Clock

#define BLUELED       2
#define NECKMOTOR     33
#define DACSHUTDOWN   32

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR char startupFileName[64] = {0};
RTC_DATA_ATTR char chosenFileName[64] = {0};

Audio audio;

//char chosenFileName[64] = {0};
//char startupFileName[64] = {0};


void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

int countFiles(fs::FS &fs, const char * dirname){
  int numfiles = 0;
    
  File root = fs.open(dirname);
  File file = root.openNextFile();
  while(file){
    numfiles++;
    file = root.openNextFile();
  }

  return numfiles;
}

bool getFilenameAtIndex(fs::FS &fs, const char * dirname, int index, char * buf){
  int numfiles = 0;
    
  File root = fs.open(dirname);
  File file = root.openNextFile();
  while(file){
    Serial.println(file.name());
    if(index == numfiles){
      Serial.print("Chosen "); Serial.println(file.name());
      sprintf(buf, "%s", file.name());
      return true;
    }
    numfiles++;
    file = root.openNextFile();
  }

  return false;
}

void chooseNextFile(){
  int numFiles = countFiles(SPIFFS, "/random");
  Serial.print("numfiles "); Serial.println(numFiles);

  int chosenFileIndex = random(0, numFiles);
  Serial.print("chosenfileindex "); Serial.println(chosenFileIndex);

  getFilenameAtIndex(SPIFFS, "/random", chosenFileIndex, chosenFileName);
  Serial.print("chosenfilename "); Serial.println(chosenFileName);
  
  getFilenameAtIndex(SPIFFS, "/startup", 0, startupFileName);
  Serial.print("startupfilename "); Serial.println(startupFileName);
}

// the setup function runs once when you press reset or power the board
void setup() {
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector  
  
  Serial.begin(115200);
  Serial.println("Up");

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  
  pinMode(BLUELED, OUTPUT);
  pinMode(NECKMOTOR, OUTPUT);
  pinMode(DACSHUTDOWN, OUTPUT);


  digitalWrite(BLUELED, HIGH);
  delay(25);
  digitalWrite(BLUELED, LOW);
  
  // Format the internal filesystem if this is the first boot.
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  //listDir(SPIFFS, "/random", 0);
  //listDir(SPIFFS, "/startup", 0);

  if(bootCount == 1)
    chooseNextFile();
  
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1); //1 = High, 0 = Low

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  const char *selected_sound = 0;

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15); // 0...21

  if(ESP_SLEEP_WAKEUP_EXT0 == wakeup_reason)  {
    // Wing button pressed - time to play a sound.

    if(2 == bootCount) {
      // First button press after a power failure.
      selected_sound = startupFileName;
      
    } else {
      // TODO select a sound at random from the ones on the filesystem
      // TODO put the filename in selected_sound
      selected_sound = chosenFileName;
    }

    // Audio playback is handled in the loop() function.
  } else if(ESP_RST_BROWNOUT == wakeup_reason) {
    // Battery dead - just go to sleep without playing anything or setting any wakeup conditions. :(
    deepsleep();
    while(true);

   } else {
    // First boot after power failure, probably battery replacement or power switch frobbing.
   }

  if(0 != selected_sound) {
    audio.connecttoFS(SPIFFS, selected_sound);
    digitalWrite(BLUELED, HIGH); 
    digitalWrite(NECKMOTOR, HIGH);
    digitalWrite(DACSHUTDOWN, HIGH);
  } else {
    Serial.println("No sound selected, nothing to do.");
    // Configure a wakeup event when GPIO 4 goes high.
    deepsleep();
  }
  
}

void deepsleep() {
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop() {
  audio.loop();
}

// Dumps some debug information about the currently-playing audio file.
void audio_info(const char *info){
    Serial.print("info        ");Serial.println(info);
}

// Callback when mp3 playback ends.
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);

    // Configure a wakeup event when GPIO 4 goes high.
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1); //1 = High, 0 = Low
    digitalWrite(BLUELED, LOW);
    digitalWrite(NECKMOTOR, LOW);
    digitalWrite(DACSHUTDOWN, LOW);


    // Pick a new file for next time.
    chooseNextFile();

    // Put the device back to sleep.
    deepsleep();
}
