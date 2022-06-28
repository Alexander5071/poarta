//commands:
//1. POARTA => open the gate
//2. ADD:"0712345678","Nume" => adds phone number on the SIM
//3. REMOVE:"0712345678" => removes phone number from the SIM
//AT+CPIN? => checks wheter SIM is ready

#include <SoftwareSerial.h>
#define poarta 10
#define SIM_SIZE 250
#define rst A5
#define DEBUG A0
#define Tx_GSM 3
#define Rx_GSM 2

void(* resetFunc) (void) = 0;//arduino reset function

String getCommand(bool ok=0);
char ch;
String s,nr,name,ath="ATH",OK="OK",ReadEntry="AT+CPBR=",RING="RING";
int i,j,mx=0,l,v[SIM_SIZE+1],errnr=0;
bool ok,bypass=0;
unsigned long prec,curent,ulmax=4294967295,timp;

//Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(Tx_GSM,Rx_GSM);//SIM800L Tx & Rx is connected to Arduino #3 & #2
void setup()
{
	//init control pins
	pinMode(poarta,OUTPUT);
	digitalWrite(poarta,LOW);
	pinMode(rst,OUTPUT);
	digitalWrite(rst,LOW);
	pinMode(DEBUG,INPUT);

	//Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
	Serial.begin(9600);
	Serial.println("Initializing...");
	delay(5000);//SMS breaks without this pause

	//Begin serial communication with Arduino and SIM800L
	mySerial.begin(9600);

	//setup built-in LED
	pinMode(LED_BUILTIN,OUTPUT);
	digitalWrite(LED_BUILTIN,LOW);
	delay(1000);

	//Debug Mode Activation
	if(analogRead(DEBUG)<=50)Serial.println("==DEBUG MODE==");
	while(analogRead(DEBUG)<=50)
	{
		//Serial.print("Valoare pin: ");
		//Serial.println(analogRead(DEBUG));
		while(Serial.available())
		{
			mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
		}
		updateSerial();
	}
	
	/*mySerial.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
	updateSerial();
	mySerial.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
	updateSerial();*/

	//setup
	setupSMS();
	readPhonebook();
	Serial.println(String(mx)+" nr incarcate");
	cls();
	firstNetworkRegister();

	//finished setup
	digitalWrite(LED_BUILTIN,HIGH);
	delay(2000);
	digitalWrite(LED_BUILTIN,LOW);
	prec=millis();
}

void loop()
{
  	if(bypass)bypass=0;
	else s=getCommand(true);
	if(s==RING)//call received
	{ok=1;
		s=getCommand();
		if(s==OK)s=getCommand();
		nr=getNumber(s);
		l=nr.length();
		if(l==0){Serial.println("Hidden number");sendCommand(ath);}
		else if(checkDigits(nr))
		{
			nr=add40(nr);
			sendCommand(ath);//reject the call
			while(mySerial.available())mySerial.read();
			Serial.println(nr);
			if(checkNr(nr))activate();
		}
	}
	else if(s.startsWith("+CMT:"))//SMS received
	{
		nr=add40(getNumber(s));//get sender of SMS
		Serial.println(nr);
		s=getCommand();
		if(s==OK)s=getCommand();
		i=checkNr(nr);
		Serial.println(String(i));
		if(i)
		{
			if(s.startsWith("P"))activate();//received message "POARTA"
			else if(s.startsWith("ADD"))
			{
				nr=remove40(getNumber(s));
				Serial.println("Number to be added: "+nr);
				if(checkDigits(nr))
				{//Serial.println("s este "+s);
					name=getNumber(s);
					Serial.println("Name to be added: "+name);
					for(i=1;i<=SIM_SIZE;i++)
					{
						for(j=1;j<=mx;j++)if(i==v[j])break;
						if(j>mx)break;
					}
					sendCommand("AT+CPBW="+String(i)+",\""+nr+"\",129,\""+name+'\"');
					v[++mx]=i;
				}
			}
			else if(s.startsWith("REMOVE"))
			{
				nr=add40(getNumber(s));
				i=checkNr(nr);
				for(j=1;j<=mx;j++)
					if(v[j]==i){v[j]=v[mx--];break;}
				if(j<=mx)sendCommand("AT+CPBW="+String(i));
			}
		}
		cls();
	}
	else if(s.startsWith("+CREG: ")&&s.charAt(7)==0)//if network status changed to 0
	{
		error();
		retry();
	}
	while(Serial.available())
	{
		mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
	}
	//updateSerial();
	s="";
	curent=millis();
	if(curent<prec)timp=ulmax-prec+curent+1;//ulong size limit exceeded
	else timp=curent-prec;
	//Serial.println(timp/1000);//to be removed when debugging is finished
	if(timp>=30000)//if there's been more that 30 seconds from last SIM check
	{
		sendCommand(ReadEntry+String(SIM_SIZE));//request last number stored in SIM
		s=getCommand();
		if(s.startsWith(RING)||s.startsWith("+CMT:")||s.startsWith("+CREG: "))bypass=1;
		else if(s.startsWith("ERROR"))
		{
			error();
			digitalWrite(rst,HIGH);
			mySerial.end();
			delay(110);
			digitalWrite(rst,LOW);
			delay(5000);//SMS breaks without this pause
			mySerial.begin(9600);
			delay(1000);//just to be sure
			setupSMS();
			firstNetworkRegister();

			///to be removed after debugging
			sendCommand("ATD+ +40767000747;");
			delay(5000);
			sendCommand(ath);
			cls();
			///to be removed after debugging


			//////resetFunc();
		}
		prec=millis();/////the line that broke everything
	}
	//prec=curent; ///backup of working (but flawed) version
}

void updateSerial()
{
	delay(500);
	while(Serial.available())
	{
		mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
	}
	while(mySerial.available()) 
	{
		Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
	}
}
String getCommand(bool ok=0)/*ok parameter controls whether the function waits
for input or just forwards whatever is available at the time of being called*/
{String s = "";char ch;
	if(ok&&!mySerial.available())return s;
	do
	{
		while(!mySerial.available());//wait for a character to be received
		ch=mySerial.read();
		s+=ch;
	}
	while(ch!='\n');//wait for newline
	s.trim();//remove leading and trailing whitespaces
	s.toUpperCase();
	if(s=="")return getCommand();//get another command if empty line was read
	Serial.println(s);
	return s;
}
void sendCommand(String s)
{String temp;
	mySerial.println(s);
	delay(20);
	temp=getCommand();
	delay(20);
	if(s!=temp)//command erroneously received by the GSM module
	{
		cls();
		sendCommand(s);//retry sending the same command
	}
}
String getNumber(String &s)
{int k=s.indexOf('\"');
	if(k==-1)return "0";//if there's no " in the string
	s=s.substring(k+1);//erase first " in string and anything before it
	k=s.indexOf('\"');
	if(k==-1)return "0";//if there's no second " in the string
	s.setCharAt(k,' ');//erase second " in string for future number extractions
	return s.substring(0,k);
}
String add40(String s)//adds +40 prefix to a non-prefixed Romanian phone number
{
	if(s.length()==10)s="+4"+s;
	return s;
}
String remove40(String s)//removes the +40 prefix of a Romanian phone number
{
	if(s.length()==12)s=s.substring(2);
	return s;
}
int checkNr(String nr)
{int i;String temp;
	for(i=1;i<=mx;i++)
	{
		sendCommand(ReadEntry+String(v[i]));
		temp=getCommand();
		cls();
		if(s.startsWith(OK))continue;
		temp=getNumber(temp);
		if(!checkDigits(temp)){i--;error();continue;}
		if(nr==add40(temp))return i;
		delay(20);
	}
	return 0;
}
void activate()
{
	digitalWrite(poarta,HIGH);
	digitalWrite(LED_BUILTIN,HIGH);
	delay(500);
	digitalWrite(poarta,LOW);
	digitalWrite(LED_BUILTIN,LOW);
	Serial.println("Activated");
}
int checkDigits(String nr)
{int l=nr.length(),i;
	if(l!=10&&l!=12)return 0;
  	if(!isdigit(nr.charAt(0))&&nr.charAt(0)!='+')return 0;
	for(i=1;i<l;i++)if(!isdigit(nr.charAt(i)))return 0;
	return l;
}
void cls()
{
	while(mySerial.available())mySerial.read();
}
void error()
{int i;
	for(i=1;i<=5;i++)
	{
		digitalWrite(LED_BUILTIN,HIGH);
		delay(100);
		digitalWrite(LED_BUILTIN,LOW);
		delay(100);
	}
}
void retry()
{
	sendCommand("AT+CSQ");
	getCommand();
	sendCommand("AT+COPS=1,2,\"22601\"");
	getCommand();
	delay(1000);
	sendCommand("AT+COPS=0");
}

void setupSMS()//various SMS settings
{
	while(handshake()!=true);//wait until the handshake is successful
	cls();
	mySerial.println("ALT+CLIP=1");//display calling phone number
	updateSerial();
	mySerial.println("AT+CMGF=1");//configuring TEXT mode
	updateSerial();
	mySerial.println("AT+CNMI=1,2,0,0,0");//decides how newly arrived SMS messages should be handled
	updateSerial();
	mySerial.println("AT+CPBS=\"SM\"");
	updateSerial();
}

bool handshake()
{
	cls();
	sendCommand("AT");//once the handshake test is successful, it will back to OK
	s=getCommand();
	cls();
	return s==OK;
}

void readPhonebook()//reading the phonebook to keep in RAM
{
	for(i=1;i<=SIM_SIZE;i++)
	{//delay(100);
		sendCommand(ReadEntry+String(i));//ask for the number saved in position i
		s=getCommand();
		if(s.length()>7&&checkDigits(getNumber(s)))v[++mx]=i;//if valid number is stored
		else if(s==OK)continue;//if no number is stored
		else {i--;error();cls();continue;}//error, retry reading same number
	}
}

void firstNetworkRegister()
{
	ok=1;
	do
	{sendCommand("AT+CREG?");//Check whether it has registered in the network
		s=getCommand();
		ch=s.charAt(9);
		if(ch!='1')error();
		else ok=0;
		cls();
		if(ch=='0')retry();
		cls();
	}while(ok);
	mySerial.println("AT+CREG=1");//always announce network changes on serial
	updateSerial();
	cls();
}