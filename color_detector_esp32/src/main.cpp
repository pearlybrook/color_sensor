#include <Arduino.h>

// OLED display libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//------------------------------------------------------------------------------
// Function declarations
//------------------------------------------------------------------------------
void display_init();
void display_refresh();
int read_color_channel(uint8_t &color_index);
void write_color_to_display(uint8_t &color_index);
uint8_t map_color_vals();
void display_splash_screen();
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Color sensor
//------------------------------------------------------------------------------
// Pin mapping for the color sensor->Adafruit ESP32
// Frequency scaling pins.
//  Currently have these hardwired HIGH so they aren't necessary.
//#define S0 27
//#define S1 33
// Photodiode selection pins
#define S2 15
#define S3 33
#define color_sensor_in 32

// Mapping for different color logic paths.
//  Warning: Order is assumed elsewhere through direct mapping using integers 
//    and should not be modified.
enum COLOR_CHANNELS {
  RED =   0,
  GREEN = 1,
  BLUE =  2,
  CLEAR=  3
};

// Used to store the last reading for each color channel.
//  Intended use is for the enum COLOR_CHANNELS to be the access key.
int color_readings[4];

// Stores the minimum and maximum readings taken since last power on for each
//  color channel.
//  Note that these are raw values, direct from the sensor, and do not have any
//    mapping applied to them.
//  enum COLOR_CHANNELS form the columns and {MIN, MAX} form the rows.
int color_min_max_readings[4][2]{
  {32767, -32768},
  {32767, -32768},
  {32767, -32768},
  {32767, -32768}
};

// Photodiode selection pin logic values for each color channel.
//  Intended use is to use the enum COLOR_CHANNELS to access the row.
//  Read columns as output pins mapped to color sensor S2 and S3.
int color_read_pin_maps[4][2]{
  {LOW,   LOW},   // Red
  {HIGH,  HIGH},  // Green
  {LOW,   HIGH},  // Blue
  {HIGH,  LOW}    // Clear
};

// Min and max reading values used to map each  of the color channels to typical
//  RGB 0->255 values.
//  Format is [COLOR_CHANNELS::<COLOR>][{MIN, MAX}]
//  Note: these values are determined empirically, and for a cheap sensor like
//    the TCS2300 (w/o mountains of effort at least) these are nothing but a 
//    vain attempt at real calibration.
//    To calibrate though use whatever power source the final circuit intends to
//      use, in lighting conditions similar to the use case environment, and 
//      read in a few readings using known red, green, and blue colored objects
//      in front of the sensor. Note down the lowest and highest values, then
//      plug those two values in to this array.
//  Think the best reasonable result here is to find the absolute min/max values
//    possible and use those, that way the values mapped will never exceed the
//    [0,255] range.
//  Final values we chose the lowest/highest for each between the two tests.
int color_read_calib_vals[4][2]{
  {1, 111}, 
  {2, 125},
  {1, 101},
  {0, 255}
};

// Cursor locations for the color output lines on the OLED display.
//  These are mapped using starting location for each color segment, i.e. the
//    direct values are where the text denoting each color is placed.
//  Values are setup as [COLOR][{WIDTH,HEIGHT,OFFSET}] where:
//    WIDTH: is the X-axis cursor location.
//    HEIGHT: is the Y-axis cursor location.
//    OFFSET: additive offset, from WIDTH, to where the color data cursor is
//      located.
uint8_t color_cursor_locations[4][3]{
  {3,   20,   10},
  {50,  20,   10},
  {97,  20,   10},
  {0,   30,   10}
};

// Maximum number of characters allowed for the displaying of labels for the
//  RGBC channels on the OLED display.
//  Note that this must include N+1 for the null terminator.
#define COLOR_DISPLAY_TEXT_CHARS 3
char color_display_text[][3] = {
  "R:",
  "G:",
  "B:",
  "C:"
};
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Definitions
//------------------------------------------------------------------------------
// Number of RGB mapping values we have stored, this is used to link the lengths
//  of the RGB values array and the string mapping array.
#define RGB_VAL_MAPPING_LEN 6

// Max length of the display strings used to map RGB values to human readable
//  values, plus the null terminator.
#define RGB_DISPLAY_STR_MAX_LEN 6

// Used for mapping int's to output strings in the arrays that follow.
enum COLOR_STR_MAP {
  RED_STR   = 0,
  GREEN_STR = 1,
  BLUE_STR  = 2,
  BLACK_STR = 3,
  WHITE_STR = 4,
  UNDEF_STR = 5
};

// RGB values for the nearest match color mapping.
//  Warning: this is index locked with the rgb string mapping array.
// TODO: with the inclusion of 'Undef' as an output string in the display map
//  array we've broken the index locking between this and that. We aren't 
//  actively using this outside the depreciated Euclidean color mapping though
//  so not handling this yet.
uint8_t RGB_VALS[RGB_VAL_MAPPING_LEN][3]{
  {255,0,0},      // Red
  {128,0,128},    // Green
  {0,0,255},      // Blue
  {0,0,0},        // Black
  {255,255,255}   // White
};

// Display string mappings for the RGB value array.
//  Warning: this is index locked to the RGB values array.
char RGB_DISPLAY_MAP[RGB_VAL_MAPPING_LEN][RGB_DISPLAY_STR_MAX_LEN] = {
  "Red",
  "Green",
  "Blue",
  "Black",
  "White",
  "Undef"
};
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// OLED Display
//------------------------------------------------------------------------------
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

Adafruit_SSD1306 screen(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
//------------------------------------------------------------------------------


// Notes:
//  **** - We need to calibrate while on final battery power, the returned 
//    values change considerably with the drop from steady 5V to unsteady ~4V.
//    Likely this has to do with the square wave read in not having as steady a
//    reference voltage, it probably assumes 5V or something like that.
//    To easily map the returned values from whatever to 0-255 use the map()
//      function: map(input_val, min_out_val, max_out_val, 255, 0);
//        Verify that these parameters are mapped correctly, think the max/min
//        output reversal order is to reverse the mapping to get proper RGB 
//        values since the basic output from the sensor is reversed from the 
//        typical RGB value reading values.
//        For this we want to connect to battery power, get red, green, and blue
//        test surfaces, run each a few times, note the min and max values for
//        each channel, then set the mapping for each channel using those min/
//        max values.
//
//  Clear removes the filters that are in place when reading the specific RGB
//    values, this helps the sensor operate in dim light but (I think) more
//    importantly serves as a calibration value for the more specific readings.
//    Perhaps something like we want to subtract this amount from all the other
//      amounts? This is just conjecture though, probably not worth the effort
//      for this project.
//
//  Can programatically turn off the color sensor board by setting S0 and S1 to
//    LOW. Not sure if this will ever be useful or not since this doesn't effect
//    the LEDs on that board.

// TODO:
//  Need to record some or all of this code production.
//    i.e. we need to screen record moving this code from here and into a new
//    file just for the recording, don't need to use that code, just need some
//    clips or something of programming parts/all of this.
//
//  Need to get the input into a state where we know we can work with it before
//    proceeding to the case build.
//    Actually walking back on this a little bit, think we can proceed 
//      regardless of performance since realistically the only thing we will 
//      want to change here is the LED placement. The other solutions are all
//      going to be in software or prohibitive for one reason or another.
//    To this end may be worth trying out the sensor inside a tube, like it will
//      be eventually (i.e. in this scenario we've found it best to push the
//      sensor back up in a tube a little ways which may be true).
//    May also be worth testing out some magnification, the clip on lens we have
//      for cell phone cameras is a good candidate here as we could use that in
//      the actual build directly.
//      The eye glass attachment magnifying lenses could work well too.
//    May just need some cleaner test materials for calibration, finding some
//      things that are much more clearly red green and blue.
//    May just need to fudge this and have the final product not show the actual
//      RGB readouts. Concievably we could make this work for very basic colors,
//      and leave the rest out.
//    May be worth adding in external light, or removing the onboard LEDs and 
//      replacing them with longer leads or something we can angle inwards 
//      towards the sensor better since this is how I see all the guides using 
//      this module.
//  
//  Think we can cheese this hard by restricting colors to black, white, and RGB
//    RGB determined by choosing the highest value of the three channels, and 
//      picking black or white if each of the channels is within some margin of
//      each other or all channels are above/below some threshold at the same 
//      time.

void setup() {
  // Color sensor communication pins setup
  //pinMode(S0, OUTPUT);
  //pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(color_sensor_in, INPUT);
  
  /*
  // Setting frequency scaling to 20%
  // Note: this was necessary for downscaling the frequency values for the
  //  Arduino Nano, but the ESP32 doesn't need this scaled down.
  //  Since we don't need scaling, and these lines pulled high return 100% freq
  //    we've wired these pins direct HIGH instead of digital output pins.
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);
  */

  // Start serial communication
  Serial.begin(115200);

  // Delay to give Serial time to boot
  delay(500);

  Serial.println("Initializing OLED display...");
  // OLED Monitor bootup and failure check
  // TODO: should probably do something if this fails but it's unreportable if
  //  Serial isn't connected anyway so not worth the effort unless we add in a
  //  fault LED or something similar.
  if(!screen.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("OLED Monitor init failed...");
  }

  // Initialize the OLED monitor
  display_init();

  display_splash_screen();

  Serial.println("Initialization finished, starting main program loop...");
}

void loop() {
  // Refresh the OLED display to clear old data.
  display_refresh();

  // Read the color sensor for all 4 channels.
  for(uint8_t color=0; color<3; color++){
      // Read the color.
      color_readings[color] = read_color_channel(color);

      // Write the color to the OLED display.
      write_color_to_display(color);
  }

  // Update OLED display
  screen.display();

  /*
  // Calibration settings stuff, can remove once the system is built.
  Serial.println("------------------------------");
  Serial.print("R-min: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::RED][0]);
  Serial.print(" ");
  Serial.print("R-max: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::RED][1]);
  Serial.println();

  Serial.print("G-min: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::GREEN][0]);
  Serial.print(" ");
  Serial.print("G-max: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::GREEN][1]);
  Serial.println();

  Serial.print("B-min: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::BLUE][0]);
  Serial.print(" ");
  Serial.print("B-max: ");
  Serial.print(color_min_max_readings[COLOR_CHANNELS::BLUE][1]);
  Serial.println();
  */

  delay(100);
}

// Handles any one-time OLED display initialization logic.
void display_init(){
  screen.clearDisplay();
  screen.setTextSize(1);
  screen.setTextColor(WHITE);

  return;
}

// Handles any static logic necessary when the OLED display needs to be 
//  refreshed. Ex. static logos and text that persist between each program loop.
//  Intended use is to be called each program loop, before that loop's specific
//    data is written out to the display. 
void display_refresh(){
  screen.clearDisplay();
  screen.setCursor(15, 5);
  screen.setTextSize(1);
  screen.println("Pearlybrook Ind.");
  screen.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, 1);

  return;
}

// Takes a color index, mapped to enum COLOR_CHANNELS, and reads that channel,
//  performing any sanitization and mapping, and returns that value.
// Optional arg determines the number of readings to average together to produce
//  the actual returned reading value.
int read_color_channel(uint8_t &color_index){
  int ret_val = 0;
  
  // Set the color filter channel.
  digitalWrite(S2, color_read_pin_maps[color_index][0]);
  digitalWrite(S3, color_read_pin_maps[color_index][1]);

  // Read the color channel.
  ret_val = pulseIn(color_sensor_in, LOW);

  // TODO: on the first pass this could technically update both min and max 
  //  values with the same reading. This shouldn't be an issue but if it is just
  //  remove the else and run each check on each loop.
  // TODO: determine if this is an appropriate method for rough setting the min
  //  max values. If not we can remove it, or move it into some sort of 
  //  calibration mode. Currently we aren't using these values for anything, 
  //  unless we print them to the console.
  // Check for localized min/max reading and update the storage array if so.
  if(ret_val < color_min_max_readings[color_index][0]){
    // Set new minimum.
    color_min_max_readings[color_index][0] = ret_val;
  }
  else if (ret_val > color_min_max_readings[color_index][1]){
    // Set new maximum.
    color_min_max_readings[color_index][1] = ret_val;
  }

  // Map values to a typical RGB 0-255 format.
  //  Note the reversal of min/max in the second pair of params is intentional
  //    as the raw TCS2300 output is reversed from RGB value expectations.
  ret_val = map(
    ret_val,
    color_read_calib_vals[color_index][0],
    color_read_calib_vals[color_index][1],
    255,
    0
  );

  return ret_val;
}

// Takes a color index, mapped to enum COLOR_CHANNELS, and displays that 
//  channel's static and variable output data in the OLED display.
void write_color_to_display(uint8_t &color_index){
  screen.setTextSize(1);
  
  // Set cursor and write static color label text.
  screen.setCursor(
    color_cursor_locations[color_index][0], 
    color_cursor_locations[color_index][1]
  );
  screen.print(color_display_text[color_index]);
  
  // Set cursor and write the color reading value.
  screen.setCursor(
    color_cursor_locations[color_index][0] + 
      color_cursor_locations[color_index][2], 
    color_cursor_locations[color_index][1]
  );
  screen.print(color_readings[color_index]);

  // Map the values to a human readable format and print to display.
  //char out_str[RGB_DISPLAY_STR_MAX_LEN] = "MAP ERR";
  uint8_t map_val = map_color_vals();

  // TODO: need to define this color string readout cursor location as a 
  //  constant like we do for the other UI elements.
  screen.setCursor(35, 40);
  screen.setTextSize(2);

  // Write the human readable mapped RGB color value to the OLED screen.
  if(map_val < RGB_VAL_MAPPING_LEN){
    // TEMP: just for the thumbnail
    screen.print(RGB_DISPLAY_MAP[COLOR_STR_MAP::BLACK_STR]);
    //screen.print(RGB_DISPLAY_MAP[map_val]);
  }
  else{
    /*
    Serial.print("RGB->Str mapping error for RGB values: ");
    for(int i=0; i<3; i++)
      Serial.println(color_readings[i]);
    */

    screen.print("MAP ERR");
  }

  return;
}

// Helper function that reads the current values in the global color reading
//  storage array and maps them to an index that can be used to lookup the 
//  corresponding closest color string.
// Returns the index of the closest color match, mapping to the constant display
//  string map.
//  Returns 255 on error.
uint8_t map_color_vals(){
  //    Testing for outliers (Grubb's test or ESD (extreme studentized deviate))
  //      Z = ABS(mean - value) / SD  
  //        Where SD is standard deviation(?)
  //        Max value of Z can be computed with (N - 1) / SQR(N).
  //          In our case of N = 3 Zmax = 1.155
  //      SD = SQR((1 / N) * SUM((Ni - MEAN)^2))
  //        Explained: 
  //          Find MEAN (just an average of set N).
  //          For each value in set N, indexed by i:
  //            Subtract the mean, square the result.
  //          Divide the total summation by N to get the mean of the differences
  //          Take the square root of the result to get the standard deviation.

  // +- value determining spread of values that indicate the read color is 
  //  either black or white (closest match of the 5 alloted colors we have 
  //  mapped).
  // Note that this value is radial, meaning full deviation range can be twice
  //  this value.
  // To determine an appropriate value here we need to finalize our test samples
  //  test each, find the smallest deviation between the RGB tests and the 
  //  readings, i.e. for each color tested figure out what the smallest range
  //  between the positive RGB color reading is and the negative RGB colors then
  //  that value, minus some tolerance, is this value.
  //  Ex. Red paper: RGB reads 255,200,200.
  //      Grn paper: RGB reads 200,245,200.
  //      Blu paper: RGB reads 200,200,235.
  //        Blue's 235 is the smallest difference between the red and green for
  //          its reading, so our b/w threshold determiner is 35.
  uint8_t wb_deviation = 8;

  // Threshold for determining whether a positive black/white reading is either
  //  black or white.
  //  Note that we are currently defining this as a multiplicative value for 
  //    ease of transcription.
  //    i.e. if each channel returns a value greater than N, where N * 3 = deter
  //      then we assume white, else black.
  uint16_t wb_determine = 220 * 3;

  bool red_in_bw_rng = false;
  bool grn_in_bw_rng = false;
  bool blu_in_bw_rng = false;

  // TODO: think we're currently using the mapped values for calculations here,
  //  this shouldn't be an issue, but it can throw off some of the calibration
  //  values we use (like wb_deviation values) when we modify the mapping HI/LOW
  //  values. Should instead use the raw values, this however does come at the
  //  cost of inverting the logic, since the mapping is moving the raw from
  //  0=white to 255=white.

  // Determine if each color channel's reading is within deviation range for
  //  eiter black or white.
  if(
    abs(color_readings[0] - color_readings[1]) < wb_deviation &&
    abs(color_readings[0] - color_readings[2]) < wb_deviation
  ){
    red_in_bw_rng = true;
  }
  if(
    abs(color_readings[1] - color_readings[0]) < wb_deviation &&
    abs(color_readings[1] - color_readings[2]) < wb_deviation
  ){
    grn_in_bw_rng = true;
  }  
  if(
    abs(color_readings[2] - color_readings[0]) < wb_deviation &&
    abs(color_readings[2] - color_readings[1]) < wb_deviation
  ){
    blu_in_bw_rng = true;
  }

  // If color is in range of black or white determine which of the two it is.
  if(red_in_bw_rng && grn_in_bw_rng && blu_in_bw_rng){
    if(color_readings[0] + color_readings[1] + color_readings[2] > wb_determine)
      return COLOR_STR_MAP::WHITE_STR;
    else
      return COLOR_STR_MAP::BLACK_STR;
  }

  // Color readings do not indicate black or white, so we need to find which of
  //  the other three possible colors it is.
  //  To do so we use an outlier test, assuming a single outlier on a color
  //    channel is the positive color. This works only with a very limited set 
  //    of primary colors, i.e. any mixed colors will throw this off 
  //    significantly and we'd need to go back to something like Euclidean 
  //    distance for mapping the RGB values to strings instead.
  double avg = (color_readings[0] + color_readings[1] + color_readings[2]) / 3;
  double std_dev = 
    sqrt(
      (
        pow(color_readings[0] - avg, 2) + 
        pow(color_readings[1] - avg, 2) + 
        pow(color_readings[2] - avg, 2)
      )
      / 3
    );
  
  // Compute the actual Grubb's outlier value for each channel.
  double red_outlier = abs(avg - color_readings[0]) / std_dev;
  double grn_outlier = abs(avg - color_readings[1]) / std_dev;
  double blu_outlier = abs(avg - color_readings[2]) / std_dev;   

  // Return the highest outlier found as the color mapping.
  // Nested if statement checks for the edge case where the outlier is in the
  //  negative direction (i.e. below the other two readings). This doesn't
  //  indicate a positive for that color but rather a negative for any color we
  //  have mapped, so we return the undefined condition signifier.
  if(red_outlier > grn_outlier && red_outlier > blu_outlier){
    if(color_readings[0] < color_readings[1] || 
       color_readings[0] < color_readings[1])
        return COLOR_STR_MAP::UNDEF_STR;
    else
      return COLOR_STR_MAP::RED_STR;
  }

  if(grn_outlier > red_outlier && grn_outlier > blu_outlier){
    if(color_readings[1] < color_readings[0] || 
       color_readings[1] < color_readings[2])
        return COLOR_STR_MAP::UNDEF_STR;
    else
      return COLOR_STR_MAP::GREEN_STR;
  }

  if(blu_outlier > red_outlier && blu_outlier > grn_outlier){
    if(color_readings[2] < color_readings[0] || 
       color_readings[2] < color_readings[1])
        return COLOR_STR_MAP::UNDEF_STR;
    else
      return COLOR_STR_MAP::BLUE_STR;
  }

  // 255 is color mapping error code, if we reach this we haven't 
  //  deterministically found a string to map the read in color to and couldn't
  //  determine that the color was just undefined.
  //  Note that with how simplistically we're determining color mappings here 
  //    hitting this condition isn't atypical, just not ideal.
  return 255;

  /*
  // Method using the Euclidean distance formula.
  //  This method is much more verbose, but also much more dependent on clean
  //    input data, so it didn't work well.
  //    Math.sqrt(
  //            Math.pow(targetRgb[0] - reading[0], 2) +
  //            Math.pow(targetRgb[1] - reading[1], 2) +
  //            Math.pow(targetRgb[2] - reading[2], 2)

  double min_dist = 99999;
  uint8_t min_dist_index = 255;

  for(uint8_t i=0; i<RGB_VAL_MAPPING_LEN; i++){
    // Using Euclidean distance formula to find closest color match.
    double dist = sqrt(
      pow(RGB_VALS[i][0] - color_readings[0], 2) + 
      pow(RGB_VALS[i][1] - color_readings[1], 2) + 
      pow(RGB_VALS[i][2] - color_readings[2], 2)
    );

    //Serial.print(RGB_VALS[i][0]);
    //Serial.print(" - ");
    //Serial.print(color_readings[0]);
    //Serial.print(" = ");
    //Serial.println(RGB_VALS[i][0] - color_readings[0]);

    if(dist < min_dist){
      min_dist = dist;
      min_dist_index = i;
    }
  }

  return min_dist_index;
  */
}

// Helper function for displaying the boot-up splash screen.
void display_splash_screen(){
  // Delay, in ms, between each . displayed during the "Initializing..." display
  uint16_t init_dot_delay = 500;
  
  screen.setTextSize(2);

  screen.setCursor(30, 20);
  screen.print("Pearly");

  screen.setCursor(37, 40);
  screen.print("brook");
  screen.display();
  delay(1500);

  screen.clearDisplay();

  screen.setTextSize(1);
  screen.setCursor(30, 30);
  screen.print("Initializing");
  screen.display();
  delay(init_dot_delay);
  screen.print(".");
  screen.display();
  delay(init_dot_delay);
  screen.print(".");
  screen.display();
  delay(init_dot_delay);
  screen.print(".");
  screen.display();
  delay(init_dot_delay);

  screen.clearDisplay();
  screen.setCursor(40, 30);
  screen.print("Welcome");
  screen.display();
  delay(500);

  return;
}