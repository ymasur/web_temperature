/*
  Temperature web interface and record

 This example shows how to serve data from onewire input  
 via the Arduino Yun's built-in webserver using the Bridge library.

 The circuit:
 * 
 * SD card attached to SD card slot of the Arduino Yun
 * 3 x onewire probe attached to input 8
 * Print LearnCbot
 * One relay contact to setup pin 10

 Prepare your SD card with an empty folder in the SD root 
 named "arduino" and a subfolder of that named "www". 
 This will ensure that the Yun will create a link 
 to the SD to the "/mnt/sd" path.

 You can remove the SD card while the Linux and the 
 sketch are running but be careful not to remove it while
 the system is writing to it.

 created  6 July 2013 by Tom Igoe
 2014-08-09 YM Version with onewire + data logger
 2014-08-31 YM: log file named with Month and year; add pump state input
 2014-09-26 YM: compute proportion of unused time by pump
 2016-05-05 YM: memorize last temp 1 with pump activity
 */
#define VERSION "1.5" // Module version, displayed on a WEB page
#include <Bridge.h>
#include <YunServer.h>
#include <YunClient.h> 
#include <FileIO.h>
#include <OneWire.h>
#include <string.h>

// prototype
int read_1_wire(int *val, byte max_scan);
#define CLEAR 1
int pump_ratio(int action);

// Const #defined here
#define LED13 13
#define P1 2
#define P2 3
#define LED1 4
#define LED2 5
#define LED3 6
#define LED4 7

// DS18S20 Temperature onewire probes
OneWire ds(8);  // connected on pin 8
#define MAXWIRED 3
#define PUMP 10 // measure of pump activity

// Global vars
//                          1         2         3
//                0123456789012345678901234567890
char fname[40] = "/mnt/sd/arduino/www/tempdata.txt";
//conservative filename 8.3 format
//the four chars 'temp' are replaced by year and month, as 1409
#define OFFSET_YYMM 20  //used to modify the filename

// time given by Unix cmd                        1
//                                     01234567890123456
char dateTime[20];  // asci format, as 14/08/29 22:10:42
// char position for second
#define TIME_S 16

String dataStr; // used for conversion 
int t_val[MAXWIRED];
int t0_mem;   //track last value of t_val[0] if pump is active
YunServer server;
YunClient client;
char startTime[20];
long hits = 0;
byte errFile = 0;
float pump_on, pump_off;

void setup() 
{  
  // we use serial for log messages
  Serial.begin(9600);
  // LearnBot circuit
  pinMode(P1, INPUT);
  pinMode(P2, INPUT);
  pinMode(PUMP, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  
  // Bridge startup
  pinMode(LED13,OUTPUT);
  digitalWrite(LED13, LOW);
  Bridge.begin();
  digitalWrite(LED13, HIGH);

  // Listen for incoming connection only from localhost
  // (no one from the external network could connect)
  server.listenOnLocalhost();
  server.begin();

  // get the time that this sketch started:
  getTimeStamp(dateTime);
  strcpy(startTime, dateTime);
  FileSystem.begin();
  
  //init vars
  pump_ratio(CLEAR);
  // first call gives garbage
  readProbes();
}// End setup()

/*  is_pump()
    ---------
    Prédicat pour connaître l'état actif de la pompe et de sa simulation
    par /P1 (0 actif)
    Retourne: true si active; false autrement
*/
bool is_pump()
{
  if (!digitalRead(P1) || digitalRead(PUMP))
    return true;
  else
    return false;
}

/* pump_ratio()
   ------------
   Cacule le taux d'utilisation, selon les compteurs pump_on 
   et pump_off, incrémentés quasi tous les 50 ms
   Paramètre: action non nul, -> RAZ des compteurs
   Retour: valeur de 0 à 100
*/
int pump_ratio(int action)
{
  int ratio = 0;
  if ((pump_on + pump_off) < 1.0) return ratio;
  ratio = (int) 100.0 * (pump_on / (pump_on + pump_off));
  if (action)
  {
    pump_on = pump_off = 0.0;
  }
  return ratio;
}

/*
  readProbes()
  -----------
  Read temperature on MAXWIRED probes.  
  fill the t_val[] table with values.
*/
void readProbes()
{
  // values zeroed
  for (int i = 0; i < MAXWIRED; i++)
    t_val[i] = 0; 
    
  // read temperatures
  while(read_1_wire(t_val, MAXWIRED) == 0);
}


/*
  readTempStr()
  -------------
  return a String with probes values, TAB separated
*/
String readTempStr()
{
  String tempStr;
  int whole;
  int fract;
  
  // convert int in String
  for (int i = 0; i < MAXWIRED; i++)
  {
    whole = t_val[i] / 10; // separate whole part
    fract = t_val[i] % 10; // and fractional part
    tempStr += "\t";    
    tempStr += String(whole);
    tempStr += ".";
    tempStr += String(fract);
  }
  return tempStr;
}

/* 
  IsSyncTime000(String s)
  --------------------
  Prédicat sur les secondes à 0, permettant une synchro toute les 10 minutes
  s: String au format date comme: 06/10/14-23:02:21
     avec l'alignement            01234567890123456
   Retour: 1 si synchronisation effective, 0 sinon  
*/
bool IsSyncTime000(char s[])
{
  if (s[16] == '0' && s[15] == '0' && s[13] =='0')
//  if (s[16] == '0' && s[15] == '0')
    return true;
  else
    return false;
}

/*  storeTemp()
    -----------
    Store the actual temperature in a file on SD card
    Global vars used:
    - char [] fname, contain the full path
    - char[] dateTime, that contain the actual date and time
    Modified vars:
    - fname, the filename 
    - errFile: false if OK; then true if an error occures
*/
void storeTemp()
{
 // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  // The FileSystem card is mounted at the following "/mnt/SD"
  // create the name with year and month on 4 digits
  fname[OFFSET_YYMM +0] = dateTime[0];
  fname[OFFSET_YYMM +1] = dateTime[1];
  fname[OFFSET_YYMM +2] = dateTime[3];
  fname[OFFSET_YYMM +3] = dateTime[4];
  
  File dataFile = FileSystem.open(fname, FILE_APPEND);

  // if the file is available, write to it:
  if (dataFile) 
  {
    dataFile.print(dateTime);
    dataFile.print("\t");
    dataFile.print(pump_ratio(CLEAR));
    dataFile.println(dataStr);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataStr);
    errFile = false;
  }  
  else  // if the file isn't open, pop up an error: 
  {
    Serial.print("error opening ");
    Serial.println(fname);
    errFile = true;
  }
}

/*
 * webArduino()
 * ------------
 * servicing WEB page if needed
 */
void webArduino()
{
  // Is there a new client?
  client = server.accept();
  if (client) 
  {
    // read the command
    String command = client.readString();
    command.trim();        //kill whitespace
    Serial.println(command);
    // is "temperature" command?
    if (command == "temperature") 
    {  
      // reverse LED13 state, to show interaction
      digitalWrite(LED13, 1 - digitalRead(LED13));
      Serial.println(dateTime);
      // print the temperature:
      client.print("Date:\t");
      client.println(dateTime);
      client.print("Temperatures:\t");
      client.print(readTempStr());
      client.println(" [degres C]");
      client.print("Pompe:\t");
      client.print(is_pump() ? "On ":"Off");
      client.print("\tratio:\t");
      client.print(pump_ratio(0));
      client.println("%");
      // Add general infos
      client.print("Module V " VERSION " actif depuis ");
      client.println(startTime);
      client.print("Appels:\t");
      client.println(++hits);
      // Show if recording is ready, or not:
      if (errFile)
        client.print("Fichier SD en erreur: ");
      else
        client.print("Fichier SD alimente: ");
      client.println(fname);
    }
    // Close connection and free resources.
    client.stop();
  }
}

/*   loop
 *   ----
 *   red continuously the temperature probes
 *   toggle LED3 at second frequency
 *   at sync time, store the temperatures datas in a file
 *   maintain ration of pump activity
 *   servicing WEB page if needed
 */
void loop() 
{
  // maintain state while sync is active
  static byte stored;
  
  // set pump state or simulation by P1 on LEDs
  if (is_pump())
  {
    pump_on = pump_on + 1.0;
    digitalWrite(LED1, 1);
    t0_mem = t_val[0];  //track the value when pump is active
  }
  else
  {
    pump_off = pump_off + 1.0;    
    digitalWrite(LED1, 0);
  }
  
  digitalWrite(LED2, digitalRead(PUMP) ? 1:0 );
    
  // temp reading, continuously
  readProbes();
  
  // get the time from the server:
  getTimeStamp(dateTime);
  dateTime[TIME_S+1] = 0; // end garanted
  // Toggle LED 3 a the second pulse
  digitalWrite(LED3, (dateTime[TIME_S] & 1) ? 1:0);
  
  //store values only at second 00
  if (IsSyncTime000(dateTime))//Q: sync windows reached?
  {
    if (stored == false)    //Q: not already done?
    {                       //A: yes, write on file
      if (t_val[0] < t0_mem)  //Q: memorized is greater?
          t_val[0] = t0_mem;  //A: yes, use it
      dataStr = readTempStr(); // refresh values
      storeTemp();          
      t0_mem = 0;           // clear temperature memorized
      pump_ratio(CLEAR);
      stored = true;
    }
  }
  else  //A: no, out of sync windows, reset flag
  {
    stored = false;
  }
  
  webArduino();
    
   // Poll every ~50ms
  delay(50);
}// end loop()

/*  getTimeStamp(char *p)
    ---------------------
    This function fill a string with the time stamp
    Vars used:
    - *p pointer to the destination str
    
    returned value:
    - number of chr written
*/
int getTimeStamp(char *p) 
{
  int i = 0;
  char c;
  Process time;
  // date is a command line utility to get the date and the time
  // in different formats depending on the additional parameter
  time.begin("date");
  // parameters: for the complete date yy/mm/dd hh:mm:ss
  time.addParameter("+%y/%m/%d\t%T");
  time.run();  // run the command

  // read the output of the command
  while(time.available() > 0) 
  {
    c = time.read();
    if (c != '\n')
       p[i] = c;
    i++;
  }
  p[i] = '\0'; // end of the collected string
    
  return i;
}

