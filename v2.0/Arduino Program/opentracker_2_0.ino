
  //tracker config
  #include "tracker.h" 
  
  //External libraries
  #include <TinyGPS.h>  
  #include <avr/dtostrf.h>
  
  #include <DueFlashStorage.h>

  
  #ifdef DEBUG
    #define debug_print(x)  debug_port.println(x)
  #else
    #define debug_print(x)
  #endif
  
  
  // Variables will change:
  int ledState = LOW;             // ledState used to set the LED
  long previousMillis = 0;        // will store last time LED was updated
  long watchdogMillis = 0;        // will store last time modem watchdog was reset
 
  long time_start, time_stop, time_diff;             //count execution time to trigger interval  
  int interval_count = 0;         //current interval count (increased on each data collection and reset after sending)


  char data_current[DATA_LIMIT];   //data collected in one go, max 2500 chars 
  int data_index = 0;        //current data index (where last data record stopped)
  char imei[20];             //IMEI number
  char time_char[20];             //time attached to every data line
  char modem_reply[200];     //data received from modem, max 200 chars   
  long logindex = STORAGE_DATA_START;
  byte save_config = 0;      //flag to save config to flash
  byte power_reboot = 0;           //flag to reboot everything (used after new settings have been saved)
  

  unsigned long last_time_gps, last_date_gps;      
  
  TinyGPS gps;  
  DueFlashStorage dueFlashStorage;
  

  //settings structure  
  struct settings {
  
    char apn[20];
    char user[20];
    char pwd[20];
    long interval;          //how often to collect data (milli sec, 600000 - 10 mins)
    int interval_send;      //how many times to collect data before sending (times), sending interval interval*interval_send
    byte powersave;
    char key[12];           //key for connection, will be sent with every data transmission
    char sim_pin[5];        //PIN for SIM card
    char sms_key[12];       //password for SMS commands
    
  };

  settings config;
  
  //define serial ports 
  #define gps_port Serial1
  #define debug_port SerialUSB
  #define gsm_port Serial2
  

void setup() {
  
    
   
    //setting serial ports
    gsm_port.begin(115200);  
    debug_port.begin(9600);  
    gps_port.begin(9600);  
    
    //setup led pin
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); 
    
    pinMode(PIN_C_REBOOT, OUTPUT);
    digitalWrite(PIN_C_REBOOT, LOW);  //this is required
    
    debug_print(F("setup() started"));
     
    //blink software start
    blink_start();   
  
    //debug remove
    delay(5000);   

    settings_load();
    
    //GPS setup 
    gps_setup();
    gps_on_off();
    
    //GSM setup
    gsm_setup();
    
    //turn on GSM
    gsm_restart();
    
    //send AT
    gsm_send_at();
    delay(1000);
    gsm_send_at();
    delay(1000);
    
    //supply PIN code is needed 
    gsm_set_pin();
    
    //get GSM IMEI
    gsm_get_imei();
    
    //misc GSM startup commands (disable echo)
    gsm_startup_cmd();

    //set GSM APN
    gsm_set_apn();    
    
    //get current log index
    #ifdef STORAGE
      storage_get_index();
    #endif     
    
    //set to connect once started
    interval_count = config.interval_send;    
    
     
    debug_print(F("setup() completed"));

}

void loop() {

    //start counting time
    time_start = millis();  
    
       //debug       
     if(data_index >= DATA_LIMIT)
       {
        data_index = 0;
       }        
    
    status_led();
    sms_check();   
  
   //collecting GPS data
   collect_all_data();
 
   debug_print(F("Current:")); 
   debug_print(data_current); 
   
   
   int i = gsm_send_data();         
   if(i != 1)
    {
      //current data not sent, save to sd card  
      debug_print(F("Can not send data, saving to flash memory"));   

       #ifdef STORAGE
          storage_save_current();   //in case this fails - data is lost
       #endif   
    
   
    }
    else
    {
      debug_print(F("Data sent successfully."));
    }
   
   #ifdef STORAGE
    //send available log files    
    storage_send_logs();
   
   //debug
   storage_dump();
  #endif   
  
    //reset current data and counter
    data_index = 0;   
    
    
  
    if(save_config == 1)
       {
         //config should be saved
         settings_save();
         save_config = 0; 
       }
         
     if(power_reboot == 1)
       {
         //reboot unit
         reboot(); 
         power_reboot = 0;        
       }  
     
    
     time_stop = millis();  
     if(time_stop > time_start)
       {
         //check how many  
         time_diff = time_stop-time_start;
         time_diff = config.interval-time_diff;         
         
         debug_print(F("Sleeping for:"));
         debug_print(time_diff);
         debug_print(F("ms"));
         
         if(time_diff > 0)
           {
             delay(time_diff);
           }
           else 
           {
             debug_print(F("Error: negative sleep time."));
             delay(500); 
           }
       }
       else
       {
         //probably counter reset, 50 days passed
        debug_print(F("Time counter reset")); 
        delay(1000); 
       }
    
     

}
