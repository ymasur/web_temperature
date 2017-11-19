/*
  read_1_wire()
  -------------
  Y. Masur (7/2007), adapted of F.Benninger V1.05 original code.
  10.08.2014 - YM: values in 0.1 degree
  
  Read 1 wire temperature sensors. Each time the function is called, chose the next avaiable device adresse
  and get the temp value, convert it in degree C x 10.
  Eg. 23.52 °C -> 235
  
  Input parameters:
  - pval: pointer on a int table, with max_scan elements (as val[3])
  - max_scan: dimension of the table above (eg. max_scan = 3)
  
  Modified values:
  - content of pval[], one element at each call, with the temperature in degree x 10
  
  Return value: 
  0 : conversion is done, then stored
  1 : the max device nb is reached, reset index
  -1: CRC error, data not stored
  -2: no device found
  
*/
#define PRINT_VAL 0

int read_1_wire(int *pval = NULL, byte max_scan = 8)
{
  static byte scan = 0;
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
  if ( !ds.search(addr) || scan >= max_scan) 
  {
#if PRINT_VAL 
      Serial.print("No more addresses. Reset search.\n");
#endif      
      ds.reset_search();
      scan = 0;
      return 1;  // 1 given: terminated
  }

#if PRINT_VAL   
  Serial.print("R=");
  for( i = 0; i < 8; i++)
  {
    Serial.print(addr[i], HEX);
    Serial.print(" ");
  }
#endif  
  if ( OneWire::crc8( addr, 7) != addr[7])
  {
#if PRINT_VAL     
      Serial.print("CRC error!\n");
#endif
      return -1;
  }

#if PRINT_VAL   
  if ( addr[0] == 0x10)
  {   
      Serial.print("Device is a DS18S20 family device +-0.5 deg\n");
  }
  else if ( addr[0] == 0x28)
  {
      Serial.print("Device is a DS18B20 family device +- 0.5 deg\n");
  }
  else if ( addr[0] == 0x22)
  {
      Serial.print("Device is a DS1822 family device +-2 deg\n");
  }
  else
  {
      Serial.print("Device family is not recognized: 0x");
      Serial.println(addr[0],HEX);
      return -2;
  }
#endif

  ds.reset();
  ds.select(addr);
  ds.write(0x44,1);         // start conversion, with parasite power on at the end
  delay(10);               // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  
#if PRINT_VAL   
  Serial.print("P=");
  Serial.print(present,HEX);
  Serial.print(" ");
#endif

  for ( i = 0; i < 9; i++) 
  {           // we need 9 bytes
    data[i] = ds.read();
#if PRINT_VAL     
    Serial.print(data[i], HEX);
    Serial.print(" ");
#endif    
  }
 
#if PRINT_VAL  
  Serial.print(" CRC=");
  Serial.print( OneWire::crc8( data, 8), HEX);
  Serial.println();
#endif
  byte LowByte = data[0];
  byte HighByte = data[1];
  
  int TReading = (HighByte << 8) + LowByte;
  int SignBit = TReading & 0x8000;  // test most sig bit
  
  if (SignBit) // negative
  {
    int TReading = (TReading ^ 0xffff) + 1; // 2's comp
  }
  int Tc_100 = (6 * TReading) + TReading / 4;    // multiply by (100 * 0.0625) or 6.25
  
#if PRINT_VAL   
  int Whole = Tc_100 / 100;  // separate off the whole and fractional portions
  int Fract = Tc_100 % 100;
  if (SignBit) // If its negative
  {
     Serial.print("-");
  }
  Serial.print(Whole);
  Serial.print(".");
  if (Fract < 10)
  {
     Serial.print("0");
  }
  Serial.print(Fract);
  Serial.println("\n");
#endif

  //Storage, rounded at 0.05
  pval[scan] = (Tc_100+5)/10;
  scan++;
  return 0;
}// end of read_1_wire()
