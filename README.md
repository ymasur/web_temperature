# web_temperature
WEB part for solar hot water monitoring
Used by Yun, or any WEB server by pushing the datas with ftp.
Create the appropriate directory, and copy all files.
A web page is created by PHP function, showing a day of data.

The data:
Each month, a new file is created by Arduino Yun, with name: <yymm>data.txt
yy= year with 2 digits (eg. 2017 -> 17)
mm= Month with 2 digits (eg. april -> 04)

Content: sample each 10 minutes, written on a line in this order:

{date} \t hour:minutes:seconds \t percent of pump usage \t temp1 \t temp2 \t temp3 \n

Details of fields, separated by TAB:

date format: yy/mm/dd with yy= year; mm= month; dd= day; (eg. 17/11/20 is the november 11, 2017
time format: hh:mm:ss with hh=hour; mm= minute; ss= second (eg. 05:30:00)
Note: because the sample are made each 10 minutes, mm is alway 00.

pump usage : 0 to 100 percent of use in the sample (eg. if the pump usage is 5 minutes, gives 50 % -> 50)
temp1 : temperature un degrees of solar cicuit
temp 2: temperature of solar water volume
temp 3 : temperature of hot water volume
\n : end of line, also CRLF

More details (but in french) on http://microclub.ch/2014/10/11/yun-monitoring-de-leau-solaire/ 
