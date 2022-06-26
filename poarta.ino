///comenzi:
///1. POARTA => deschide poarta
///2. ADD:"0712345678","Nume" => adauga numar pe SIM
///3. REMOVE:"0712345678" => sterge numar de pe SIM
#include <SoftwareSerial.h>
#include <ctype.h>
#define poarta 10
#define rst 4
#define DEBUG A0

void(* resetFunc) (void) = 0;///functia de resetare arduino

String getcommand(bool ok=0);
char ch;
String s, nr,nume,ath="ATH",OK="OK",ReadEntry="AT+CPBR=",RING="RING";
int i,j,mx=0,l,v[251],errnr=0;
bool ok,bypass=0;
unsigned long prec,curent,ulmax=4294967295,timp;
//Create software serial object to communicate with SIM800L
SoftwareSerial mySerial(3, 2); //SIM800L Tx & Rx is connected to Arduino #3 & #2
void setup()
{
  pinMode(poarta,OUTPUT);
  digitalWrite(poarta,LOW);
  pinMode(rst,OUTPUT);
  digitalWrite(rst,LOW);
  pinMode(DEBUG,INPUT);
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  Serial.println("Initializing...");
  delay(5000);///foarte important se fut mesajele fara
  //Begin serial communication with Arduino and SIM800L
  mySerial.begin(9600);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,LOW);
  delay(1000);
  if(analogRead(DEBUG)<=50)Serial.println("==DEBUG MODE==");
  while(analogRead(DEBUG)<=50)
  {
    Serial.print("Valoare pin: ");
    Serial.println(analogRead(DEBUG));
    while (Serial.available())
    {
      mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
    }
    updateSerial();
  }
  
  mySerial.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  
  /*mySerial.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  mySerial.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();*/
  cls();
  
  mySerial.println("ALT+CLIP=1");// afiseaza numarul de telefon care suna
  updateSerial();
  mySerial.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CNMI=1,2,0,0,0"); // Decides how newly arrived SMS messages should be handled
  updateSerial();
  mySerial.println("AT+CPBS=\"SM\"");
  updateSerial();
  for(i=1;i<=250;i++)
    {//delay(100);
      sendcommand(ReadEntry + String(i));///cere contactul cu numarul i
      s=getcommand();
      if(s.length()>7&&checkcifre(getnumber(s)))v[++mx]=i;
      else if(s==OK)continue;
      else {i--;error();cls();continue;}
    }
  Serial.println(String(mx)+" nr incarcate");
  /*s="+CPBR: 1,\"0767000747\",162,kjghdkjb";
  Serial.println(s.indexOf('\"'));*/
  cls();
  ok=1;
  do
  {sendcommand("AT+CREG?"); //Check whether it has registered in the network
     s=getcommand();
     ch=s.charAt(9);
     if(ch!='1')error();
     else ok=0;
     cls();
     if(ch=='0')retry();
     cls();
  }while(ok);
  mySerial.println("AT+CREG=1"); //afisare schimbari retea oricand
  updateSerial();
  cls();
  digitalWrite(LED_BUILTIN,HIGH);
  delay(2000);
  digitalWrite(LED_BUILTIN,LOW);
  prec=millis();
}

void loop()
{ if(bypass)bypass=0;
  else s=getcommand(true);
  if(s==RING)
  {ok=1;
    s=getcommand();
    if(s==OK)s=getcommand();
    nr=getnumber(s);
    l=nr.length();
    if(l==0){Serial.println("Numar ascuns");sendcommand(ath);}
    else if(checkcifre(nr))
    {
      nr=add40(nr);
      sendcommand(ath);///inchide-i lui gigel
      while(mySerial.available())mySerial.read();
      Serial.println(nr);
      if(checknr(nr))activate();
    }
  }
  else if(s.startsWith("+CMT:"))///am primit un SMS :)))))
  {
    nr=add40(getnumber(s));
    Serial.println(nr);
    s=getcommand();
    if(s==OK)s=getcommand();
    i=checknr(nr);
    Serial.println(String(i));
    if(i)
    {
      if(s.startsWith("P"))activate();
      else if(s.startsWith("ADD"))
      {
        nr=remove40(getnumber(s));
        Serial.println("Nr de adaugat "+nr);
        if(checkcifre(nr))
        {Serial.println("s este "+s);
          nume=getnumber(s);
          Serial.println("Nume de adaugat "+nume);
          for(i=1;i<=250;i++)
          {
            for(j=1;j<=mx;j++)if(i==v[j])break;
            if(j>mx)break;
          }
          sendcommand("AT+CPBW="+String(i)+",\""+nr+"\",129,\""+nume+'\"');
          v[++mx]=i;
        }
      }
      else if(s.startsWith("REMOVE"))
      {
        nr=add40(getnumber(s));
        i=checknr(nr);
        for(j=1;j<=mx;j++)
          if(v[j]==i){v[j]=v[mx--];break;}
        if(j<=mx)sendcommand("AT+CPBW="+String(i));
      }
    }
    cls();
  }
  else if(s.startsWith("+CREG: ")&&s.charAt(7)==0)///schimbare statut retea in 0
  {
    error();
    retry();
  }
  while (Serial.available())
  {
    mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  //updateSerial();
  s="";
  curent=millis();
  if(curent<prec)timp=ulmax-prec+curent+1;///s-a trecut peste limita unsigned long
  else timp=curent-prec;
  if(timp>=30000)///daca timpul este mai mare de 30 sec
  {
    sendcommand(ReadEntry + String(250));///cere contactul cu numarul 250
    s=getcommand();
    if(s.startsWith(RING)||s.startsWith("+CMT:")||s.startsWith("+CREG: "))bypass=1;
    else if(s.startsWith("ERROR"))
    {
      digitalWrite(rst,HIGH);
      delay(110);
      digitalWrite(rst,LOW);
      resetFunc();
    }
    //prec=curent; ///////versiunea care strica tot 
  }
  prec=curent;
}

void updateSerial()
{

  delay(500);
  while (Serial.available())
  {
    mySerial.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(mySerial.available()) 
  {
    Serial.write(mySerial.read());//Forward what Software Serial received to Serial Port
  }
}
String getcommand(bool ok=0)
{String s = "";char ch;
  if(ok&&!mySerial.available())return s;
  do
  {
    while(!mySerial.available());
    ch = mySerial.read();
    //Serial.print(ch);
    s += ch;
  }
  while (ch != '\n');
  s.trim();
  s.toUpperCase();
  if(s=="")return getcommand();
  Serial.println(s);
  return s;
}
void sendcommand(String s)
{String temp;
  mySerial.println(s);
  delay(20);
  temp=getcommand();
  delay(20);
  if(s!=temp){cls();sendcommand(s);}
}
String getnumber(String &s)
{int k=s.indexOf('\"');
  if(k==-1)return "0";
  s=s.substring(k+1);
  k=s.indexOf('\"');
  if(k==-1)return "0";
  s.setCharAt(k,' ');
  return s.substring(0,k);
}
String add40(String s)
{
  if (s.length()==10)s = "+4" + s;
  return s;
}
String remove40(String s)
{
  if (s.length()==12)s = s.substring(2);
  return s;
}
int checknr(String nr)
{int i;String temp;
  for(i=1;i<=mx;i++)
  {
    sendcommand(ReadEntry + String(v[i]));
    temp=getcommand();
    cls();
    if(s.startsWith(OK))continue;
    temp=getnumber(temp);
    if(!checkcifre(temp)){i--;error();continue;}
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
int checkcifre(String nr)
{int l=nr.length(),i;
  if(l!=10&&l!=12)return 0;
  for(i=0;i<l;i++)if(!isdigit(nr.charAt(i))&&nr.charAt(i)!='+')return 0;
  return l;
}
void cls()
{
  while(mySerial.available())mySerial.read();
}
void error()
{int i;
  for(i=1;i<=5;i++){digitalWrite(LED_BUILTIN,HIGH);delay(100);digitalWrite(LED_BUILTIN,LOW);delay(100);}
}
void retry()
{
  sendcommand("AT+CSQ");
  getcommand();
  sendcommand("AT+COPS=1,2,\"22601\"");
    getcommand();
    delay(1000);
    sendcommand("AT+COPS=0");
}
