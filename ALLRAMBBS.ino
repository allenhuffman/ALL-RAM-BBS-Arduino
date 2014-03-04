// BOF preprocessor bug prevent - insert me on top of your arduino-code
/*
#if 1
__asm volatile ("nop");
#endif
*/
/*-----------------------------------------------------------------------------
 
 *ALL RAM* BBS for Arduino
 by Allen C. Huffman (alsplace@pobox.com / www.appleause.com)
 
 This is an experiment to see how easy it is to translate Microsoft BASIC to
 Arduino C. The translated C code is incredibly ugly and will be disturbing
 to most modern programmers. It was done just to see if it could be done, and
 what better thing to translate than a program that was also originally done
 just to see if it could be done -- a cassette based BBS package from 1983!
 
 About the *ALL RAM* BBS:
 
 The *ALL RAM* BBS System was writtin in 1983 for the Radio Shack TRS-80
 Color Computer ("CoCo"). At this time, existing BBS packages for the CoCo
 required "2-4 disk drives" to operate, so *ALL RAM* was created to prove
 a BBS could run from a cassette-based computer. Instead of using floppy
 disks to store the userlog and message base, they were contained entirely
 in RAM. The in-memory databases could be saved to cassette tape and
 reloaded later.
 
 The original BASIC source code is included in comments, followed by a very
 literal line-by-line translation to Arduino C. Remember, it's not a rewrite
 in C -- it's BASIC code done in C.
 
 Be warned. There be "gotos" ahead!
 
 References about memory saving:
 
 http://www.controllerprojects.com/2011/05/23/saving-ram-space-on-arduino-when-you-do-a-serial-printstring-in-quotes/
 http://www.adafruit.com/blog/2008/04/17/free-up-some-arduino-sram/
 
 2013-04-02 0.0 allenh - Initial version with working userlog.
 2013-04-03 1.0 allenh - Message base working. Core system fully functional.
 Preliminary support for Arduino Ethernet added.
 2013-04-04 1.1 allenh - SD card support for loading/saving.
 2013-04-05 1.2 allenh - Ethernet (telnet) support.
 2013-04-06 1.3 allenh - Cleanup for posting to www.appleause.com
 2013-04-09 1.4 allenh - Fixed a bug with INPUT_SIZE and input.
 2013-04-12 1.5 allenh - Integration with new Telnet server code, misc fixes.
 -----------------------------------------------------------------------------*/
#include <avr/pgmspace.h>

#define VERSION "1.5"

// To enable SD card support, define the PIN used by SD.begin(x)
//#define SD_PIN        4

// To enable Ethernet support, defined the PORT used by EthernetServer(PORT)
#define ENET_PORT     23

// NOTE: On smaller Arduinos (UNO), there is not enough Flash or RAM to have
// both SD and Ethernet support at the same time.

#if defined(SD_PIN)
#include <SD.h>
#endif

#if defined(ENET_PORT)
#include <SPI.h>
#include <Ethernet.h>

extern EthernetClient telnetClient;

// Prototypes...
byte telnetRead(EthernetClient client);
byte telnetInput(EthernetClient client, char *cmdLine, byte len);
#endif

// And so it begins.
void setup()
{
  Serial.begin(9600);

  while(!Serial);

  showHeader();

#if defined(ENET_PORT)
  telnetInit();
#endif

  showConfig();

  // Scroll off any leftover console output.
  //for(int i=0; i<24; i++) print();
}

// We don't really have anything loop worthy for this program, so we'll
// just have some fun and pretend like we are starting up the CoCo each
// time through.
void loop()
{
  showCoCoHeader(); // Just for fun...

  allram();

  print();
  print(F("BREAK IN 520"));
  print(F("OK"));

  delay(5000);
}

// Show program header.
void showHeader()
{
  // Emit some startup stuff to the serial port.
  print(F("\n"
    "*ALL RAM* BBS for Arduino "VERSION" - 30 year anniversary edition!\n"
    "Copyright (C) 1983 by Allen C. Huffman\n"
    "Ported from TRS-80 Color Computer Extended Color BASIC.\n"
    "Build Date: "__DATE__" "__TIME__"\n"));
}

/*---------------------------------------------------------------------------*/
// In BASIC, strings are dynamic. For C, we have to pre-allocate buffers for
// the strings.
#define INPUT_SIZE  32  // 64. For aStr, etc.

#define MAX_USERS   3   // NM$(200) Userlog size. (0-200, 0 is Sysop)
#define NAME_SIZE   12   // 20. Username size (nmStr)
#define PSWD_SIZE   8   // 8. Password size (psStr & pwStr) 
#define ULOG_SIZE   (NAME_SIZE+1+PSWD_SIZE+1+1)

// To avoid hard coding some values, we define these here, too. Each message
// is made up of lines, and the first line will contain the From, To, and
// Subject separated by a character. So, while the original BASIC version
// hard coded this, we will calculate it, letting the subject be as large
// as whatever is left over (plus room for separaters and NULL at the end).
#define FR_SIZE     NAME_SIZE                      // From
#define TO_SIZE     NAME_SIZE                      // To
#define SB_SIZE     (INPUT_SIZE-FR_SIZE-1-TO_SIZE) // "From\To\Subj"

// The original BASIC version was hard-coded to hold 20 messages of 11 lines
// each (the first line was used for From/To/Subject). The Arduino has far
// less RAM, so these have been made #defines so they can be changed.
#define MAX_MSGS    3   // 19  (0-19, 20 messages)
#define MAX_LINE    2   // 10  (0-10, 11 lines)

// Rough estimate of how many bytes these items will take up.
#define ULOG_MEM    ((MAX_USERS+1)*(ULOG_SIZE))
#define MBASE_MEM   ((MAX_MSGS+1)*MAX_LINE*INPUT_SIZE)

// Validate the settings before compiling.
#if (FR_SIZE+1+TO_SIZE+SB_SIZE > INPUT_SIZE)
#error INPUT_SIZE too small to hold "From\To\Sub".
#endif

/*---------------------------------------------------------------------------*/

//0 REM *ALL RAM* BBS System 1.0
//1 REM   Shareware / (C) 1983
//2 REM     By Allen Huffman
//3 REM  110 Champions Dr, #811
//4 REM     Lufkin, TX 75901
//5 CLS:FORA=0TO8:READA$:POKE1024+A,VAL("&H"+A$):NEXTA:EXEC1024:DATAC6,1,96,BC,1F,2,7E,96,A3
//10 CLEAR21000:DIMNM$(200),MS$(19,10),A$,F$,S$,T$,BR$,CL$,NM$,PS$,PW$,A,B,C,CL,LN,LV,MS,NM,KY,UC

// All variables in BASIC are global, so we are declaring them outside the
// functions to make them global in C as well. Arrays in BASIC are "0 to X",
// and in C they are "0 to X-1", so we add one to them in C to get the same
// number of elements.
char nmArray[MAX_USERS+1][ULOG_SIZE];             // NM$(200)
char msArray[MAX_MSGS+1][MAX_LINE+1][INPUT_SIZE+1];// MS$(19,10) 1.4
char aStr[INPUT_SIZE+1];                          // A$ 1.4
char fStr[FR_SIZE];                               // F$ - From
char sStr[SB_SIZE];                               // S$ - Subj
char tStr[TO_SIZE];                               // T$ - To
char nmStr[NAME_SIZE];                            // NM$ - Name
char psStr[PSWD_SIZE];                            // PS$ - Pswd
char pwStr[PSWD_SIZE];                            // PW$ - Pswd

// To save RAM, these two strings will exist in Flash memory. It will
// require a bit of work later to use them (__FlashStringHelper*).
prog_char brStr[] PROGMEM = "*==============*==============*"; // BR$ - border
prog_char clStr[] PROGMEM = "\x0c\x0e";                        // CL$ - clear

int a, b, c, cl, ln, lv, ms, nm, ky, uc;
// A, B, C - misc.
// CL - Calls
// LN - Line Number
// LV - Level
// MS - Messages
// NM - Names (users)
// KY - Keys (commands entered)
// UC - Uppercase input (1=Yes, 0=No)

void allram()
{
  // HACK - create adefault Sysop account.
  nm = 0;
  strncpy_P(nmArray[0], PSTR("SYSOP\\TEST9"), ULOG_SIZE);

  cls(); // From line 5  

  //15 CL$=CHR$(12)+CHR$(14):BR$="*==============*==============*":GOSUB555
  //char cl[] = "\0xC\0xE";
  //char br[] = "*==============*==============*";
  gosub555();

//20 CLS:PRINTTAB(6)"*ALL RAM* BBS SYSTEM":PRINT"USERS:"NM,"CALLS:"CL:PRINTTAB(5)"SYSTEM AWAITING CALLER";:go:SOUND200,10
line20:
nmStr[0] = 0; // reset user.
  cls();
  printTab(6);
  print(F("*ALL RAM* BBS SYSTEM"));
  printSemi(F("USERS:"));
  printSemi(nm);
  printComma();
  printSemi(F("CALLS:"));
  print(cl);
  printTab(5);
  printSemi(F("SYSTEM AWAITING CALLER"));
  //gosub1005();
  if (gosub1005()==255) goto line20;
  sound(200,10);

  //25 A$="Welcome To *ALL RAM* BBS!":GOSUB1055:KY=0:CL=CL+1
  strncpy_P(aStr, PSTR("Welcome To *ALL RAM* BBS!"), INPUT_SIZE);
  gosub1055();
  ky = 0;
  cl = cl + 1;

  showLoginMessage();

  //30 PRINT:PRINT"Password or 'NEW' :";:UC=1:GOSUB1005:PS$=A$:IFA$=""ORA$="NEW"THEN55ELSEPRINT"Checking: ";:A=0
line30:
  print();
  printSemi(F("Password or 'NEW' :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  strncpy(psStr, aStr, PSWD_SIZE);
  if (aStr[0]=='\0' || strcmp(aStr, "NEW")==0)
  {
    goto line55;
  }
  else
  {
    printSemi(F("Checking: "));
    a = 0;
  }

line35:
  //35 A$=NM$(A):B=INSTR(A$,"\"):NM$=LEFT$(A$,B-1):PW$=MID$(A$,B+1,LEN(A$)-B-1):LV=VAL(RIGHT$(A$,1)):IFPW$=PS$THEN45ELSEA=A+1:IFA<=NM THEN35
  strncpy(aStr, nmArray[a], ULOG_SIZE);
  b = instr(aStr, "\\");
  strncpy(nmStr, aStr, b-1);
  nmStr[b-1] = '\0';  
  strncpy(pwStr, &aStr[b], strlen(aStr)-b-1);
  pwStr[strlen(aStr)-b-1] = '\0';
  lv = atoi(&aStr[strlen(aStr)-1]);
  if (strncmp(pwStr, psStr, PSWD_SIZE)==0)
  {
    goto line45;
  }
  else
  {
    a = a + 1;
    if (a<=nm) goto line35;
  }

line40: // for empty userlog bug
  //40 PRINT"*INVALID*":KY=KY+1:IFKY<3THEN30ELSE215
  print(F("*INVALID*"));
  ky = ky + 1;
  if (ky<3) goto line30;
  goto line215;

line45:
  //45 PRINT"*ACCEPTED*":PRINTBR$:PRINT"On-Line: "NM$:PRINT"Access :"LV:PRINT"Caller :"CL:KY=0:GOTO115
  print(F("*ACCEPTED*"));
  print((__FlashStringHelper*)brStr);
  printSemi(F("On-Line: "));
  print(nmStr);
  printSemi(F("Access :"));
  print(lv);
  printSemi(F("Caller :"));
  print(cl);
  ky = 0;
  goto line115;

  //50 'New User
line55:
  //55 A$="Password Application Form":GOSUB1055
  strncpy_P(aStr, PSTR("Password Application Form"), INPUT_SIZE);
  gosub1055();

  //60 IFNM=200THENPRINT"Sorry, the userlog is full now.":GOTO215ELSEPRINT"Name=20 chars, Password=8 chars"
  if (nm==MAX_USERS)
  {
    print(F("Sorry, the userlog is full now."));
    goto line215;
  }
  else
  {
    printSemi(F("Name="));
    printNumSemi(NAME_SIZE);
    printSemi(F(" chars, Password="));
    printNumSemi(PSWD_SIZE); 
    printSemi(F(" chars"));
  }

line65:
  //65 PRINT:PRINT"Full Name :";:UC=1:GOSUB1005:NM$=A$:IFA$=""ORLEN(A$)>20THEN30
  print();
  printSemi(F("Full Name :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  strncpy(nmStr, aStr, NAME_SIZE);
  if (aStr[0]=='\0' || strlen(aStr)>20) goto line30;

  //70 PRINT"Password  :";:UC=1:GOSUB1005:PW$=A$:IFA$=""ORLEN(A$)>8THEN30
  printSemi(F("Password  :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  strncpy(pwStr, aStr, PSWD_SIZE);
  if (aStr[0]=='\0' || strlen(aStr)>8) goto line30;

  //75 PRINT:PRINT"Name :"NM$:PRINT"Pswd :"PW$:PRINT"Is this correct? ";:UC=1:GOSUB1005:IFLEFT$(A$,1)="Y"THEN80ELSE65
  print();
  printSemi(F("Name :"));
  print(nmStr);
  printSemi(F("Pswd :"));
  print(pwStr);
  printSemi(F("Is this correct? "));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  if (aStr[0]=='Y')
  {
    goto line80;
  }
  else
  {
    goto line65;
  }

line80:
  //80 NM=NM+1:NM$(NM)=NM$+"\"+PW$+"0":LV=0:KY=0
  nm = nm + 1;
  strncpy(nmArray[nm], nmStr, NAME_SIZE);
  strcat_P(nmArray[nm], PSTR("\\"));
  strncat(nmArray[nm], pwStr, PSWD_SIZE);
  //strcat_P(nmArray[nm], PSTR("0"));
  strcat_P(nmArray[nm], PSTR("1")); // AUTO VALIDATED
  //lv = 0;
  lv = 1;
  ky = 0;
  //85 PRINT"Your password will be validated as soon as time permits.  Press":PRINT"[ENTER] to continue :";:GOSUB1005
  print(F("Your password will be validated as soon as time permits.  Press"));
  printSemi(F("[ENTER] to continue :"));
  //gosub1005();
  if (gosub1005()==255) goto line20;

  //100 'Main Menu
line105:
  //105 A$="*ALL RAM* BBS Master Menu":GOSUB1055
  strncpy_P(aStr, PSTR("*ALL RAM* BBS Master Menu"), INPUT_SIZE);
  gosub1055();

  //110 PRINT"C-all Sysop","P-ost Msg":PRINT"G-oodbye","R-ead Msg":PRINT"U-serlog","S-can Titles"
  printSemi(F("C-all Sysop"));
  printComma();
  print(F("P-ost Msg"));
  printSemi(F("G-oodbye"));
  printComma();
  print(F("R-ead Msg"));
  printSemi(F("U-serlog"));
  printComma();
  print(F("S-can Titles"));

line115:
  //115 PRINTBR$
  print((__FlashStringHelper*)brStr);

line120:
  //120 KY=KY+1:IFKY>200THENPRINT"Sorry, your time on-line is up.":GOTO210ELSEIFKY>180THENPRINT"Please complete your call soon."
  ky = ky + 1;
  if (ky>200)
  {
    print(F("Sorry, your time on-line is up."));
    goto line210;
  }
  else if (ky>180)
  {
    print(F("Please complete your call soon."));
  }

line125:
  showFreeRam();
  //125 PRINTTAB(7)"?=Menu/Command :";:UC=1:GOSUB1005:A$=LEFT$(A$,1)
  printTab(7);
  printSemi(F("?=Menu/Command :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[1] = '\0';

line130:
  //130 LN=INSTR("?CGRSPU%",A$):IFLN=0THENPRINT"*Invalid Command*":GOTO120
  ln = instr("?CGRSPU%", aStr);
  if (ln==0)
  {
    print(F("*Invalid Command*"));
    goto line120;
  }

  //135 IFLV<1ANDLN>5THENPRINT" Sorry, you are not validated.":GOTO125
  if (lv<1 && ln>5)
  {
    print(F(" Sorry, you are not validated."));
    goto line125;
  }

  //140 ONLN GOTO105,155,205,405,455,305,255,505
  if (ln==1) goto line105;
  if (ln==2) goto line155;
  if (ln==3) goto line205;
  if (ln==4) goto line405;
  if (ln==5) goto line455;
  if (ln==6) goto line305;
  if (ln==7) goto line255;
  if (ln==8) goto line505;

  //150 'Call Sysop
line155:
  //155 A$="Calling the Sysop":GOSUB1055:A=0
  strncpy_P(aStr, PSTR("Calling the Sysop"), INPUT_SIZE);
  gosub1055();
  a = 0;

  //165 PRINT" BEEP!";:SOUND150,5:IFINKEY$=CHR$(12)THEN175ELSEprintING$(5,8);:A=A+1:IFA<25THEN165
line165:
  printSemi(F(" BEEP!"));
  sound(150, 5);
  if (inkey()==12)
  {
    goto line175;
  }
  else
  {
    string(5, 8);
    a = a + 1;
    if (a<25) goto line165;
  }

  //170 PRINT:PRINT" The Sysop is unavaliable now.":GOTO115
  print(F(""));
  print(F(" The Sysop is unavaliable now."));
  goto line115;

line175:
  //175 PRINT:PRINTTAB(6)"*Chat mode engaged*"
  print();
  printTab(6);
  print(F("*Chat mode engaged*"));

line180:
  //180 GOSUB1005:IFLEFT$(A$,3)="BYE"THEN185ELSE180
  //gosub1005();
  if (gosub1005()==255) goto line20;
  if (strncmp(aStr, "BYE", 3)==0)
  {
    goto line185;
  }
  else
  {
    goto line180;
  }

line185:
  //185 PRINTTAB(5)"*Chat mode terminated*":GOTO115
  goto line115;

  //200 'Goodbye
line205:
  //205 A$="Thank you for calling":GOSUB1055
  strncpy_P(aStr, PSTR("Thank you for calling"), INPUT_SIZE);
  gosub1055();

line210:
  //210 PRINT:PRINT"Goodbye, "NM$"!":PRINT:PRINT"Please call again."
  print(F(""));
  printSemi(F("Goodbye, "));
  printSemi(nmStr);
  print(F("!"));
  print();
  print(F("Please call again."));

line215:
  //215 PRINT:PRINT:PRINT"*ALL RAM* BBS disconnecting..."
  print();
  print();
  print(F("*ALL RAM* BBS disconnecting..."));

  //220 FORA=1TO1000:NEXTA
  delay(1000);

#if defined(ENET_PORT)
  telnetDisconnect();
#endif

  //225 GOTO20
  goto line20;

  //250 'Userlog
line255:
  //255 A$="List of Users":GOSUB1055:PRINT"Users on system:"NM:IFNM=0THEN115ELSEA=1
  strncpy_P(aStr, PSTR("List of Users"), INPUT_SIZE);
  gosub1055();
  printSemi(F("Users on system:"));
  print(nm);
  if (nm==0)
  {
    goto line115;
  }
  else
  {
    a = 1;
  }
line260:
  //260 A$=NM$(A):PRINTLEFT$(A$,INSTR(A$,"\")-1)TAB(29)RIGHT$(A$,1)
  strncpy(aStr, nmArray[a], INPUT_SIZE);
  {
    char tempStr[NAME_SIZE+1];         // Add room for NULL.
    strncpy(tempStr, aStr, NAME_SIZE+1);
    tempStr[instr(tempStr, "\\")-1] = '\0';
    printSemi(tempStr);
  }
  printTab(29);
  print(right(aStr,1));

  //265 IF(A/10)=INT(A/10)THENPRINT"C-ontinue or S-top :";:UC=1:GOSUB1005:IFLEFT$(A$,1)="S"THEN275
  if (a % 10 == 9)
  {
    printSemi(F("C-ontinue or S-top :"));
    uc = 1;
    //gosub1005();
    if (gosub1005()==255) goto line20;
    if (aStr[0]=='S') goto line275;
  }

  //270 A=A+1:IFA<=NM THEN260
  a = a + 1;
  if (a<=nm) goto line260;

line275:
  //275 PRINT"*End of Userlog*":GOTO115
  print(F("*End of Userlog*"));
  goto line115;

  //300 'Post Msg
line305:
  //305 IFMS=20THENPRINT"One moment, making room...":FORA=0TO18:FORB=0TO10:MS$(A,B)=MS$(A+1,B):NEXTB:NEXTA:MS=19
  if (ms==MAX_MSGS+1)
  {
    print(F("One moment, making room..."));
    for(a=0; a<=MAX_MSGS-1; a++)
    {
      for(b=0; b<=MAX_LINE; b++)
      {
        strncpy(msArray[a][b], msArray[a+1][b], INPUT_SIZE);
      }
    }
    ms = MAX_MSGS;
  }
  //310 CLS:PRINTCL$"This will be message #"MS+1:FORA=0TO10:MS$(MS,A)="":NEXTA:F$=NM$
  cls();
  print((__FlashStringHelper*)clStr);
  printSemi(F("This will be message #"));
  print(ms+1);
  for (a=0; a<=MAX_LINE; a++)
  {
    msArray[ms][a][0] = '\0';
  }
  strncpy(fStr, nmStr, NAME_SIZE);

  //315 PRINT"From :"F$:PRINT"To   :";:UC=1:GOSUB1005:A$=LEFT$(A$,20):T$=A$:IFA$=""THEN115
  printSemi(F("From :"));
  print(fStr);
  printSemi(F("To   :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[NAME_SIZE] = '\0';
  strncpy(tStr, aStr, TO_SIZE);
  if (aStr[0]=='\0') goto line115;

  //320 PRINT"Is this message private? ";:UC=1:GOSUB1005:IFLEFT$(A$,1)="Y"THENS$="*E-Mail*":GOTO330
  printSemi(F("Is this message private? "));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  if (aStr[0]=='Y')
  {
    strncpy_P(sStr, PSTR("*E-Mail*"), SB_SIZE);
    goto line330;
  }

  //325 PRINT"Subj :";:UC=1:GOSUB1005:A$=LEFT$(A$,18):S$=A$:IFA$=""THEN115
  printSemi(F("Subj :"));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[SB_SIZE-1] = '\0';
  strncpy(sStr, aStr, SB_SIZE);
  if (aStr[0]=='\0') goto line115;

line330:
  //330 PRINT"Enter up to 10 lines, 64 chars. [ENTER] on a blank line to end.":A=0
  printSemi(F("Enter up to"));
  printSemi(MAX_LINE);
  printSemi(F("lines,"));
  printSemi(INPUT_SIZE);
  print(F("chars. [ENTER] on a blank line to end."));
  a = 0;

line335:
  //335 A=A+1:PRINTUSING"##>";A;:GOSUB1005:MS$(MS,A)=A$:IFA$=""THENA=A-1:GOTO345ELSEIFA<10THEN335
  a = a + 1;
  printUsingSemi("##>", a);
  //gosub1005();
  if (gosub1005()==255) goto line20;
  strncpy(msArray[ms][a], aStr, INPUT_SIZE);
  if (aStr[0]=='\0')
  {
    a = a - 1;
    goto line345;
  }
  else if (a<MAX_LINE) goto line335;

line340:
  //340 PRINT"*Message Buffer Full*"
  print(F("*Message Buffer Full*"));

line345:
  //345 PRINT"A-bort, C-ont, E-dit, L-ist, or S-ave Message? ";:UC=1:GOSUB1005:A$=LEFT$(A$,1):IFA$=""THEN345
  printSemi(F("A-bort, C-ont, E-dit, L-ist, or S-ave Message? "));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[1] = '\0';
  if (aStr[0]=='\0') goto line345;

  //350 LN=INSTR("ACELS",A$):IFLN=0THEN345ELSEONLN GOTO385,355,360,375,380
  ln = instr("ACELS", aStr);
  if (ln==0) goto line345;
  if (ln==1) goto line385;
  if (ln==2) goto line355;
  if (ln==3) goto line360;
  if (ln==4) goto line375;
  if (ln==5) goto line380;

line355:
  //355 IFA<10THENPRINT"Continue your message:":GOTO335ELSE340
  if (a<MAX_LINE)
  {
    printSemi(F("Continue your message:"));
    goto line335;
  } 
  else goto line340;

line360:  
  //360 PRINT"Edit line 1 -"A":";:GOSUB1005:LN=VAL(LEFT$(A$,2)):IFLN<1ORLN>A THEN345
  printSemi(F("Edit line 1 -"));
  printSemi(a);
  printSemi(F(":"));
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[2] = '\0';
  ln = atoi(aStr);
  if (ln<1 || ln>a) goto line345;

  //365 PRINT"Line currently reads:":PRINTMS$(MS,LN):PRINT"Enter new line:":GOSUB1005:A$=LEFT$(A$,64):IFA$=""THENPRINT"*Unchanged*"ELSEMS$(MS,LN)=A$:PRINT"*Corrected*"
  print(F("Line currently reads:"));
  print(msArray[ms][ln]);
  print(F("Enter new line:"));
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[INPUT_SIZE] = '\0'; // 1.4
  if (aStr[0]=='\0')
  {
    print(F("*Unchanged*"));
  }
  else
  {
    strncpy(msArray[ms][ln], aStr, INPUT_SIZE);
    print(F("*Corrected*"));
  }

  //370 GOTO360
  goto line360;

line375:
  //375 CLS:PRINTCL$"Message Reads:":FORB=1TOA:PRINTUSING"##>";B;:PRINTMS$(MS,B):NEXTB:GOTO345
  cls();
  print((__FlashStringHelper*)clStr);
  print(F("Message Reads:"));
  for (b=1; b<=a; b++)
  {
    printUsingSemi("##>", b);
    print(msArray[ms][b]);
  }
  goto line345;

line380:
  //380 MS$(MS,0)=T$+"\"+F$+"\"+S$:MS=MS+1:PRINT"*Message"MS"stored*":GOTO115
  strcpy(msArray[ms][0], tStr);
  strcat_P(msArray[ms][0], PSTR("\\"));
  strcat(msArray[ms][0], fStr);
  strcat_P(msArray[ms][0], PSTR("\\"));
  strcat(msArray[ms][0], sStr);
  ms = ms + 1;
  printSemi(F("*Message"));
  printSemi(ms);
  print(F("stored*"));
  goto line115;

line385:
  //385 PRINT"*Message Aborted*":GOTO115
  print(F("*Message Aborted*"));
  goto line115;

  //400 'Read Msg
line405:
  //405 IFMS=0THENPRINT"The message base is empty.":GOTO115
  if (ms==0)
  {
    print(F("The message base is empty."));
    goto line115;
  }

  //410 CLS:PRINTCL$
  cls();
  print((__FlashStringHelper*)clStr);

line415:
  //415 PRINT"Read Message 1 -"MS":";:GOSUB1005:A=VAL(LEFT$(A$,2)):IFA<1ORA>MS THEN115
  printSemi(F("Read Message 1 -"));
  printSemi(ms);
  printSemi(F(":"));
  //gosub1005();
  if (gosub1005()==255) goto line20;
  aStr[2] = '\0';
  a = atoi(aStr);
  if (a<1 || a>ms) goto line115;

  //420 A$=MS$(A-1,0):B=INSTR(A$,"\"):C=INSTR(B+1,A$,"\"):T$=LEFT$(A$,B-1):F$=MID$(A$,B+1,C-B-1):S$=RIGHT$(A$,LEN(A$)-C)
  strncpy(aStr, msArray[a-1][0], INPUT_SIZE);
  b = instr(aStr, "\\");
  c = instr(b+1, aStr, "\\");
  strncpy(tStr, aStr, b-1);
  tStr[b-1] = '\0';
  strncpy(fStr, (aStr-1)+b+1, c-b-1);
  fStr[c-b-1] = '\0'; // FIXTHIS - max copy sizes here?
  strncpy(sStr, right(aStr, strlen(aStr)-c), SB_SIZE);

  //425 IFS$="*E-Mail*"ANDLV<8THENIFNM$<>T$ANDNM$<>F$THENPRINT"That message is private.":GOTO415
  if (strcmp(sStr, "*E-Mail*")==0 && lv<8)
  {
    if (strcmp(nmStr, tStr)!=0 && strcmp(nmStr, fStr)!=0)
    {
      print(F("That message is private."));
      goto line415;
    }
  }

  //430 CLS:PRINTCL$"Message #"A:PRINT"From :"F$:PRINT"To   :"T$:PRINT"Subj :"S$:PRINT:B=0
  cls();
  print((__FlashStringHelper*)clStr);
  printSemi(F("Message #"));
  print(a);
  printSemi(F("From :"));
  print(fStr);
  printSemi(F("To   :"));
  print(tStr);
  printSemi(F("Subj :"));
  print(sStr);
  print();
  b = 0;

line435:
  //435 B=B+1:PRINTMS$(A-1,B):IFMS$(A-1,B)=""THEN440ELSEIFB<10THEN435
  b = b + 1;
  print(msArray[a-1][b]);
  if (msArray[a-1][b][0]=='\0')
  {
    goto line440;
  }
  else if (b<MAX_LINE) goto line435;

line440:
  //440 PRINT"*End of Message*":GOTO415
  print(F("*End of Message*"));
  goto line415;

  //450 'Scan Titles
line455:
  //455 IFMS=0THENPRINT"The message base is empty.":GOTO115
  if (ms==0)
  {
    print(F("The message base is empty."));
    goto line115;
  }

  //460 CLS:PRINTCL$"Message Titles:":A=0
  cls();
  print((__FlashStringHelper*)clStr);
  print(F("Message Titles:"));
  a = 0;

line465:
  //465 A$=MS$(A,0):PRINTUSING"[##] SB: ";A+1;:B=INSTR(A$,"\"):C=INSTR(B+1,A$,"\"):PRINTRIGHT$(A$,LEN(A$)-C):PRINTTAB(5)"TO: "LEFT$(A$,B-1):A=A+1:IFA<MS THEN465
  strncpy(aStr, msArray[a][0], INPUT_SIZE);
  printUsingSemi("[##] SB: ", a+1);
  b = instr(aStr, "\\");
  c = instr(b+1, aStr, "\\");
  print(right(aStr, strlen(aStr)-c));
  printTab(5);
  printSemi(F("TO: "));
  {
    char tempStr[TO_SIZE];
    strncpy(tempStr, aStr, b-1);
    tempStr[b-1] = '\0';
    print(tempStr);
  }
  a = a + 1;
  if (a<ms) goto line465;

  //470 PRINT"*End of Messages*":GOTO115
  print(F("*End of Messages*"));
  goto line115;

  //500 '%SYSOP MENU%
line505:
  //505 IFLV<9THENA$="Z":GOTO130
  if (lv<9)
  {
    strncpy_P(aStr, PSTR("Z"), INPUT_SIZE);
    goto line130;
  }

  //510 PRINT"PASSWORD?";:GOSUB1005:IFA$<>"?DROWSSAP"THENPRINT"Thank You!":GOTO115
  printSemi(F("PASSWORD?"));
  //gosub1005();
  if (gosub1005()==255) goto line20;
  if (strcmp(aStr, "?DROWSSAP")!=0)
  {
    print(F("Thank You!"));
    goto line115;
  }

  //515 PRINT"Abort BBS? YES or NO? ";:UC=1:GOSUB1005:IFA$<>"YES"THEN115
  printSemi(F("Abort BBS? YES or NO? "));
  uc = 1;
  //gosub1005();
  if (gosub1005()==255) goto line20;
  if (strcmp(aStr, "YES")!=0) goto line115;

  //520 GOSUB605:STOP
  gosub605();
  return;
} // end of allram()

/*---------------------------------------------------------------------------*/
// Subroutines (formerly GOSUBs)
//
//550 '%LOAD%
void gosub555()
{
  //555 PRINT"%LOAD% [ENTER] WHEN READY";:GOSUB1005
  printSemi(F("%LOAD% [ENTER] WHEN READY"));
  //gosub1005();
  print();
  if (aStr[0]=='!') return;

  //560 OPEN"I",#-1,"USERLOG":INPUT#-1,CL,NM:FORA=0TONM:INPUT#-1,NM$(A):NEXTA:CLOSE
  loadUserlog();

  //565 OPEN"I",#-1,"MSG BASE":INPUT#-1,MS:FORA=0TOMS-1:FORB=0TO10:INPUT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:RETURN
  loadMsgBase();
}

//600 '%SAVE%
void gosub605()
{
  //605 PRINT"%SAVE% [ENTER] WHEN READY";:GOSUB1005:MOTORON:FORA=0TO999:NEXTA
  printSemi(F("%SAVE% [ENTER] WHEN READY"));
  gosub1005();
  
  //610 OPEN"O",#-1,"USERLOG":PRINT#-1,CL,NM:FORA=0TONM:PRINT#-1,NM$(A):NEXTA:CLOSE
  saveUserlog();

  //615 OPEN"O",#-1,"MSG BASE":PRINT#-1,MS:FORA=0TOMS-1:FORB=0TO10:PRINT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:RETURN
  saveMsgBase();
}

//1000 'User Input
#define CR         13
#define INBUF_SIZE 64
byte gosub1005()
{
  byte ch; // Used only here, so we can make it local.
  byte count;

  //1005 LINEINPUTA$:A$=LEFT$(A$,64):IFUC=0ORA$=""THENRETURN
  count = lineinput(aStr, INPUT_SIZE);
  aStr[INPUT_SIZE] = '\0';
  if ((uc==0) || (aStr[0]=='\0')) return count;

  //1010 FORC=1TOLEN(A$):CH=ASC(MID$(A$,C,1)):IFCH>96THENMID$(A$,C,1)=CHR$(CH-32)
  for (c=0; c<strlen(aStr); c++)
  {
    ch = aStr[c];
    if (ch>96) aStr[c] = ch-32;    
    //1015 IFCH=92THENMID$(A$,C,1)="/"
    if (ch==92) aStr[c] = '/';
    //1020 NEXTC:UC=0:RETURN
  }
  uc = 0;
  
  return count;
}

//1050 'Function Border
void gosub1055()
{
  //1055 CLS:PRINTCL$BR$:PRINTTAB((32-LEN(A$))/2)A$:PRINTBR$:RETURN
  cls();
  print((__FlashStringHelper*)clStr);
  print((__FlashStringHelper*)brStr);
  printTab((32-strlen(aStr))/2);
  print(aStr);
  print((__FlashStringHelper*)brStr);
}

/*---------------------------------------------------------------------------*/
// The following functions mimic some of the Extended Color BASIC commands.

// CLS
// Clear the screen.
void cls()
{
  print();
  print(F("--------------CLS--------------"));
}

// SOUND tone, duration
// On the CoCo, tone (1-255), duration (1-255; 15=1 second).
void sound(byte tone, byte duration)
{
  Serial.write(0x07);   // BEL
  delay(duration*66.6); // Estimated delay.
}

/*---------------------------------------------------------------------------*/
// String functions.

// STRING$(length, charcode)
// Generate a string of length charcode chracters.
void string(byte length, byte charcode)
{
  int i;

  for (i=0; i<length; i++) printCharSemi(charcode);
}

// RIGHT$(str, length)
// Note: Modifies the passed in string, which is okay for our purpose but
// would not be suitable as a generic replacement for RIGHT$.
char *right(char *str, byte length)
{
  return &str[strlen(str)-length];
}

// INSTR(first, str, substr)
// Starting at pos, return position of substr in str, or 0 if not found.
int instr(byte pos, char *str, char *substr)
{
  if (pos<1) return 0;
  return instr(aStr+pos, substr) + pos;
}
int instr(char *str, char *substr)
{
  char *ptr;

  ptr = strstr(str, substr);
  if (ptr==NULL) return 0; // No match?
  if (ptr==&str[strlen(str)]) return 0; // Matched the \0 at end of line?
  return ptr-str+1;
}

/*---------------------------------------------------------------------------*/
// Input functions.

#ifdef ENET_PORT
byte lineinput(char *cmdLine, byte len)
{
  return telnetInput(telnetClient, cmdLine, len);
}
#else
// LINE INPUT str
// Read string up to len bytes. This code comes from my Hayes AT Command
// parser, so the variables are named differently.
#define CR           13
#define BEL          7
#define BS           8
#define CAN          24
byte lineinput(char *cmdLine, byte len)
{
  int     ch;
  byte    cmdLen = 0;
  boolean done;

  done = false;
  while(!done)
  {
    //ledBlink();

    ch = -1; // -1 is no data available

    if (Serial.available()>0)
    {
      ch = Serial.read();
    }
    else
    {
      continue; // No data. Go back to the while()...
    }
    switch(ch)
    {
    case -1: // No data available.
      break;

    case CR:
      print();
      cmdLine[cmdLen] = '\0';
      done = true;
      break;

      /*case CAN:
       print(F("[CAN]"));
       cmdLen = 0;
       break;*/

    case BS:
      if (cmdLen>0)
      {
        printCharSemi(BS);
        printSemi(F(" "));
        printCharSemi(BS);
        cmdLen--;
      }
      break;

    default:
      // If there is room, store any printable characters in the cmdline.
      if (cmdLen<len)
      {
        if ((ch>31) && (ch<127)) // isprint(ch) does not work.
        {
          printCharSemi(ch);
          cmdLine[cmdLen] = ch; //toupper(ch);
          cmdLen++;
        }
      }
      else
      {
        printCharSemi(BEL); // Overflow. Ring 'dat bell.
      }
      break;
    } // end of switch(ch)
  } // end of while(!done)

  return cmdLen;
}
#endif

// INKEY$
// Return character waiting (if any) from standard input (not ethernet).
char inkey()
{
  if (Serial.available()==0) return 0;
  return Serial.read();
}

/*---------------------------------------------------------------------------*/

// File I/O
// Ideally, I would have created wrappers for the OPEN, READ, CLOSE commands,
// but I was in a hurry, so...

//SD Card routines
#if defined(SD_PIN)
#define TEMP_SIZE 4
#define FNAME_MAX (8+1+3+1)
boolean initSD()
{
  static bool sdInit = false;

  if (sdInit==true) return true;

  printSemi(F("Initializing SD card..."));
  pinMode(SD_PIN, OUTPUT);
  if (!SD.begin(SD_PIN))
  {
    print(F("initialization failed."));
    return false;
  }
  print(F("initialization done."));
  sdInit = true;

  return true;
}
#endif

//560 OPEN"I",#-1,"USERLOG":INPUT#-1,CL,NM:FORA=0TONM:INPUT#-1,NM$(A):NEXTA:CLOSE
void loadUserlog()
{
#if defined(SD_PIN)
  File myFile;
  char tempStr[TEMP_SIZE];
  char filename[FNAME_MAX];

  if (!initSD()) return;

  strncpy_P(filename, PSTR("USERLOG"), FNAME_MAX);

  myFile = SD.open(filename, FILE_READ);

  if (myFile)
  {
    printSemi(filename);
    print(F(" opened."));
    fileReadln(myFile, tempStr, TEMP_SIZE);
    cl = atoi(tempStr);
    Serial.print(F("cl = "));
    Serial.println(cl);
    fileReadln(myFile, tempStr, TEMP_SIZE);
    nm = atoi(tempStr);
    Serial.print(F("nm = "));
    Serial.println(nm);
    for (a=0; a<=nm; a++)
    {
      fileReadln(myFile, nmArray[a], ULOG_SIZE);
      Serial.print(a);
      Serial.print(F(". "));
      Serial.println(nmArray[a]);
    }  
    myFile.close();
  }
  else
  {
    printSemi(F("Error opening "));
    print(filename);
  }
#else  
  print(F("(USERLOG would be loaded from tape here.)"));
#endif
}

//565 OPEN"I",#-1,"MSG BASE":INPUT#-1,MS:FORA=0TOMS-1:FORB=0TO10:INPUT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:RETURN
void loadMsgBase()
{
#if defined(SD_PIN)
  File myFile;
  char tempStr[TEMP_SIZE];
  char filename[FNAME_MAX];

  if (!initSD()) return;

  strncpy_P(filename, PSTR("MSGBASE"), FNAME_MAX);

  myFile = SD.open(filename, FILE_READ);
  if (myFile)
  {
    printSemi(filename);
    print(F(" opened."));
    fileReadln(myFile, tempStr, TEMP_SIZE);
    ms = atoi(tempStr);
    Serial.print("ms = ");
    Serial.println(ms);
    for (a=0; a<=ms-1; a++)
    {
      for (b=0; b<=MAX_LINE; b++)
      {
        fileReadln(myFile, msArray[a][b], INPUT_SIZE);
        Serial.print(F("msArray["));
        Serial.print(a);
        Serial.print(F("]["));
        Serial.print(b);
        Serial.print(F("] = "));
        Serial.println(msArray[a][b]);
      }
    }  
    myFile.close();
  }
  else
  {
    printSemi(F("Error opening "));
    print(filename);
  }
#else
  print(F("(MSGBASE would be loaded from tape here.)"));
#endif
}

#if defined(SD_PIN)
//byte fileReadln(File myFile, char *buffer, byte count)
{
  char ch;
  int  pos;

  pos = 0;
  while(myFile.available() && pos<count)
  {
    ch = myFile.read();
    if (ch==CR)
    {
      buffer[pos] = '\0';
      break;
    }
    if (ch>=32)
    {
      //Serial.print(ch);
      buffer[pos] = ch;
      pos++;
    }
  }
  if (pos>=count) buffer[pos] = '\0';
  //Serial.println();
  return pos;
}
#endif

//610 OPEN"O",#-1,"USERLOG":PRINT#-1,CL,NM:FORA=0TONM:PRINT#-1,NM$(A):NEXTA:CLOSE
void saveUserlog()
{
#if defined(SD_PIN)
  File myFile;
  char filename[FNAME_MAX];

  if (!initSD()) return;

  strncpy_P(filename, PSTR("USERLOG"), FNAME_MAX);

  if (SD.exists(filename)==true) SD.remove(filename);

  myFile = SD.open(filename, FILE_WRITE);
  if (myFile)
  {
    printSemi(filename);
    print(F(" created."));
    myFile.println(cl);
    Serial.print(F("cl = "));
    Serial.println(cl);
    myFile.println(nm);
    Serial.print(F("nm = "));
    Serial.println(nm);
    for (a=0; a<=nm; a++)
    {
      myFile.println(nmArray[a]);
      Serial.print(a);
      Serial.print(F(". "));
      Serial.println(nmArray[a]);
    }
    myFile.close();
  }
  else
  {
    print(F("Error creating file."));
  }
#else
  print(F("save USERLOG"));
#endif
}

//615 OPEN"O",#-1,"MSG BASE":PRINT#-1,MS:FORA=0TOMS-1:FORB=0TO10:PRINT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:RETURN
void saveMsgBase()
{
#if defined(SD_PIN)
  File myFile;
  char filename[FNAME_MAX];

  if (!initSD()) return;

  strncpy_P(filename, PSTR("MSGBASE"), FNAME_MAX);

  if (SD.exists(filename)==true) SD.remove(filename);

  myFile = SD.open(filename, FILE_WRITE);
  if (myFile)
  {
    printSemi(filename);
    print(F(" created."));
    myFile.println(ms);
    for (a=0; a<=ms-1; a++)
    {
      for (b=0; b<=MAX_LINE; b++)
      {
        myFile.println(msArray[a][b]);
        Serial.print(F("msArray["));
        Serial.print(a);
        Serial.print(F("]["));
        Serial.print(b);
        Serial.print(F("] = "));
        Serial.println(msArray[a][b]);
      }
    }
    myFile.close();
  }
  else
  {
    print(F("Error creating file."));
  }
#else
  print(F("save MSGBASE"));
#endif
}

/*---------------------------------------------------------------------------*/
// Print (output) routines.

// For TAB to work, we need to track where we think we are on the line.
byte tabPos = 0;

// We want to simulate the following:
// PRINT "HELLO"  -- string, with carraige return at end of line
// PRINT A        -- number (space before and after), with carraige return
// PRINT "HELLO"; -- no carraige return
// PRINT TAB(5);  -- tab to position 5
// PRINT "A","B"  -- tab to column 16 (on 32-column screen)

// Due to various types of strings on Arduino (in memory or Flash), we will
// have to duplicate some functions to have versions that take the other
// types of strings.

// printTypeSemi() routines will not put a carraige return at the end.

// PRINT TAB(column);
void printTab(byte column)
{
  while(tabPos<column)
  {
    printSemi(F(" ")); // Print, and increment tab position.
  }
}

// PRINT,
// NOTE: DECB doesn't add a carraige return after a comma.
void printComma()
{
  printTab(tabPos + (16-(tabPos % 16)));
}

// PRINT "STRING";
// For normal strings in RAM.
void printSemi(const char *string)
{
  tabPos = tabPos + Serial.print(string);
#if defined(ENET_PORT)
  telnetClient.print(string);
#endif
}
// For strings in Flash.
void printSemi(const __FlashStringHelper *string)
{
  tabPos = tabPos + Serial.print(string);
#if defined(ENET_PORT)
  telnetClient.print(string);
#endif
}
// For printing a byte as a number (0-255).
void printSemi(byte num)
{
  Serial.print(F(" "));
  tabPos = tabPos + Serial.print(num);
  Serial.print(F(" "));
  tabPos = tabPos + 2;
#if defined(ENET_PORT)
  telnetClient.print(F(" "));
  telnetClient.print(num);
  telnetClient.print(F(" "));
#endif
}
// For printing a single character.
void printCharSemi(char ch)
{
  tabPos = tabPos + Serial.print(ch);
#if defined(ENET_PORT)
  telnetClient.print(ch);
#endif
}
// For printing a byte as a number with no spaces.
void printNumSemi(byte num)
{
  tabPos = tabPos + Serial.print(num);
#if defined(ENET_PORT)
  telnetClient.print(num);
#endif
}

// PRINT
void print(void)
{
  Serial.println();
#if defined(ENET_PORT)
  telnetClient.println();
#endif
}

// PRINT "STRING"
void print(const char *string)
{
  Serial.println(string);
  tabPos = 0;
#if defined(ENET_PORT)
  telnetClient.println(string);
#endif
}
void print(const __FlashStringHelper *string)
{
  Serial.println(string);
  tabPos = 0;
#if defined(ENET_PORT)
  telnetClient.println(string);
#endif
}

// PRINT I
void print(int num)
{
  Serial.print(F(" "));
  Serial.println(num);
  tabPos = 0;
#if defined(ENET_PORT)
  telnetClient.print(F(" "));
  telnetClient.println(num);
#endif
}

// PRINT USING(format, number);
// NOTE: This only emulates printing positive integers, which is the
// only way it is used by this program.
void printUsingSemi(char *format, byte num)
{
  byte i;
  byte fmtDigits;
  byte numDigits;
  byte tempNum;

  i = 0;
  while(format[i]!='\0')
  {
    if (format[i]!='#') {
      printCharSemi(format[i]);
      i++;
      continue;
    }
    else
    {
      // Start counting the run of #s.
      // Find end of #'s to know how many to use.
      fmtDigits = 0;
      while(format[i]=='#' && format[i]!='\0')
      {
        fmtDigits++;
        i++;
      }
      // Now we know how many # (digits).
      tempNum = num;
      numDigits = 1;
      while(tempNum>10)
      {
        tempNum = tempNum/10;
        numDigits++;
      }
      while(numDigits<fmtDigits)
      {
        printSemi(F(" "));
        fmtDigits--;
      }
      printNumSemi(num);
    }
  }
}

/*---------------------------------------------------------------------------*/
// Show some stuff functions.

// Emit some configuration information.
void showConfig()
{
  print((__FlashStringHelper*)brStr);
  print(F("*ALL RAM* Configuration:"));
  printSemi(F("Userlog size :"));
  print(MAX_USERS+1);
  printSemi(F("Input size   :"));
  print(INPUT_SIZE);
  printSemi(F("Username size:"));
  print(NAME_SIZE);
  printSemi(F("Password size:"));
  print(PSWD_SIZE);
  printSemi(F("Msg base size:"));
  print(MAX_MSGS+1);
  printSemi(F("Message lines:"));
  print(MAX_LINE+1);
#if defined(SD_PIN)
  print(F("SD card      : Enabled"));
#endif
#if defined(ENET_PORT)
  print(F("Ethernet     : Enabled"));
#endif
  printSemi(F("ESTIMATED MEM:"));
  printSemi(ULOG_MEM + MBASE_MEM);
  print(F("bytes."));
  printSemi(F("Free RAM     :"));
  print(freeRam());
  print((__FlashStringHelper*)brStr);
}

void showLoginMessage()
{
  print(F("\nYou are connected to an Arduino UNO R3, a small computer thing you can buy\n"
    "from Radio Shack for $29.99 (or around $22 online). It has 2K of RAM and\n"
    "32K of Flash for program storage. It has a SainSmart Ethernet Shield ($17.99)\n"
    "attached to it. The software running here is a line-by-line port of my\n"
    "*ALL RAM* BBS program written in 1983. The original Extended Color BASIC\n"
    "code was converted as literally as possible to Arduino C. It's a travesty.\n"
    "\n"
    "This BBS program was designed to run on a cassette based TRS-80 Color\n"
    "Computer with 32K of RAM. All messages and users were stored in memory.\n"
    "Obviously, with only 2K of RAM, this is not possible, so this version has\n"
    "been configured to allow only a few users and a teensy tiny message base\n"
    "(smaller than Twitter posts!). Enjoy the experiment!\n"
    "\n"
    "(If userlog is full, use the Sysop password 'TEST'.)"));
}

void showCoCoHeader()
{
  cls();
  print(F("EXTENDED COLOR BASIC 1.1\n"
    "COPYRIGHT (C) 1982 BY TANDY\n"
    "UNDER LICENSE FROM MICROSOFT\n"
    "\n"
    "OK\n"
    "CLOAD\"ALLRAM\"\n"
    "RUN"));
}

/*---------------------------------------------------------------------------*/

// Debug and utility functions.
//

unsigned int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void showFreeRam()
{
  printSemi(F("Free RAM:"));
  print(freeRam());
}

/*---------------------------------------------------------------------------*/

void showUserlog()
{
  int i;
  printSemi(F("Users: "));
  print(nm);
  for (i=0; i<=nm; i++) {
    printSemi(i);
    printSemi(F(". "));
    print(nmArray[i]);
  }
}

void showMessageBase()
{
  int i,j;

  for (int i=0; i<ms; i++)
  {
    for (int j=0; j<=MAX_LINE; j++)
    {
      printSemi(F("msArray["));
      printSemi(i);
      printSemi(F(","));
      printSemi(j);
      printSemi(F("] = "));
      print(msArray[i][j]);
    }
  }
}


/*---------------------------------------------------------------------------*/

//0 REM *ALL RAM* BBS Editor 1.0
//1 REM   Shareware / (C) 1983
//2 REM     By Allen Huffman
//3 REM  110 Champions Dr, #811
//4 REM     Lufkin, TX 75901
//5 FORA=0TO8:READA$:POKE1024+A,VAL("&H"+A$):NEXTA:EXEC1024:DATAC6,1,96,BC,1F,2,7E,96,A3
//10 CLEAR21000:DIMNM$(200),MS$(19,10),A$,F$,S$,T$,NM$,PW$,A,B,C,LN,LV,MS,NM,PR:PR=80
//15 CLS:PRINTTAB(3)"*ALL RAM* EDITOR COMMANDS:":printING$(32,45)
//20 PRINTTAB(4)"1. CREATE USERLOG",TAB(4)"2. LOAD USERLOG/MSG BASE",TAB(4)"3. SAVE USERLOG/MSG BASE",TAB(4)"4. PRINT USERLOG",TAB(4)"5. PRINT MESSAGES",TAB(4)"6. EDIT USERS",TAB(4)"7. KILL MESSAGES",TAB(4)"8. QUIT"
//25 PRINT@392,"ENTER FUNCTION :"
//30 A$=INKEY$:IFA$=""THEN30ELSEPRINT@408,A$:LN=VAL(A$):IFLN<1ORLN>8THENSOUND50,1:GOTO25
//35 SOUND200,1:ONLN GOTO55,105,155,205,255,305,405,40
//40 STOP
//50 'Create Userlog
//55 CLS:PRINTTAB(7)"SYSOP INFORMATION:":printING$(32,45)
//60 PRINT@128,"SYSOP'S NAME:    (20 CHARACTERS)>";:LINEINPUTA$:IFA$=""ORLEN(A$)>20THENSOUND50,1:GOTO15ELSENM$=A$
//65 PRINT@192,"PASSWORD    :     (8 CHARACTERS)>";:LINEINPUTA$:IFA$=""ORLEN(A$)>8THENSOUND50,1:GOTO15ELSEPW$=A$
//70 PRINT@297,"*VERIFY ENTRY*":PRINT:PRINT"NAME :"NM$:PRINT"PSWD :"PW$:PRINT@456,"IS THIS CORRECT?";
//75 LINEINPUTA$:IFLEFT$(A$,1)<>"Y"THENSOUND50,1:GOTO55
//80 NM$(0)=NM$+"\"+PW$+"9":GOTO15
//100 'Load Userlog/Msg Base
//105 CLS:PRINTTAB(5)"LOAD USERLOG/MSG BASE:":printING$(32,45)
//110 LINEINPUT" READY TAPE, THEN PRESS ENTER:";A$:PRINT@168,"...ONE MOMENT..."
//115 OPEN"I",#-1,"USERLOG":PRINT@232,"LOADING  USERLOG":INPUT#-1,CL,NM:FORA=0TONM:INPUT#-1,NM$(A):NEXTA:CLOSE
//120 OPEN"I",#-1,"MSG BASE":PRINT@240,"MSG BASE":INPUT#-1,MS:FORA=0TOMS-1:FORB=0TO10:INPUT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:GOTO15
//150 'Save Userlog/Msg Base
//155 CLS:PRINTTAB(5)"SAVE USERLOG/MSG BASE:":printING$(32,45)
//160 LINEINPUT" READY TAPE, THEN PRESS ENTER:";A$:PRINT@168,"...ONE MOMENT...":MOTORON:FORA=1TO1000:NEXTA
//165 PRINT@232,"SAVING   USERLOG":OPEN"O",#-1,"USERLOG":PRINT#-1,CL,NM:FORA=0TONM:PRINT#-1,NM$(A):NEXTA:CLOSE
//170 PRINT@240,"MSG BASE":OPEN"O",#-1,"MSG BASE":PRINT#-1,MS:FORA=0TOMS-1:FORB=0TO10:PRINT#-1,MS$(A,B):NEXTB:NEXTA:CLOSE:GOTO15
//200 'Print Userlog
//205 IFNM$(0)=""THENPRINT@454,"*USERLOG NOT LOADED*":SOUND50,1:GOTO25
//210 CLS:PRINTTAB(9)"PRINT USERLOG:":printING$(32,45)
//215 LINEINPUT"    PRESS ENTER WHEN READY:";A$:PRINT@169,"...PRINTING..."
//220 PRINT#-2,TAB((PR-30)/2)"[*ALL RAM* BBS System Userlog]":PRINT#-2,"":PRINT#-2,TAB((PR-46)/2)"[###]  [        NAME        ]  [PASSWORD]  [L]"
//225 FORA=0TONM:A$=NM$(A):B=INSTR(A$,"\"):NM$=LEFT$(A$,B-1):PW$=MID$(A$,B+1,LEN(A$)-B-1):LV=VAL(RIGHT$(A$,1))
//230 A$="000........................................0":B=LEN(STR$(A))-1:MID$(A$,4-B,B)=RIGHT$(STR$(A),B):MID$(A$,8,LEN(NM$))=NM$:MID$(A$,32,LEN(PW$))=PW$:MID$(A$,44,1)=RIGHT$(STR$(LV),1)
//235 PRINT@238,A:PRINT#-2,TAB((PR-44)/2)A$:NEXTA:GOTO15
//250 'Print Messages
//255 IFMS$(0,0)=""THENPRINT@454,"*MSG BASE NOT LOADED*":SOUND50,1:GOTO25
//260 CLS:PRINTTAB(8)"PRINT  MESSAGES:":printING$(32,45)
//265 LINEINPUT"    PRESS ENTER WHEN READY:";A$:PRINT@169,"...PRINTING..."
//270 PRINT#-2,TAB((PR-30)/2)"[*ALL RAM* BBS System Messages]":PRINT#-2,""
//275 FORA=0TOMS-1:A$=MS$(A,0):B=INSTR(A$,"\"):C=INSTR(B+1,A$,"\"):T$=LEFT$(A$,B-1):F$=MID$(A$,B+1,C-B-1):S$=RIGHT$(A$,LEN(A$)-C)
//280 PRINT@238,A+1:B=(PR-64)/2:PRINT#-2,TAB(B)"Message #"A:PRINT#-2,TAB(B)"TO :"T$:PRINT#-2,TAB(B)"FR :"F$:PRINT#-2,TAB(B)"SB :"S$:PRINT#-2,STRING$(64,45):C=0
//285 C=C+1:PRINT#-2,TAB(B)MS$(A,C):IFMS$(A,C)=""THEN290ELSEIFC<10THEN285
//290 PRINT#-2,"":NEXTA:GOTO15
//300 'Edit Users
//305 IFNM$(0)=""THENPRINT@454,"*USERLOG NOT LOADED*":SOUND50,1:GOTO25
//310 CLS:PRINTTAB(10)"EDIT  USERS:":printING$(32,45):A=0
//315 PRINT@70,"USERS ON SYSTEM:"NM
//320 A$=NM$(A):B=INSTR(A$,"\"):NM$=LEFT$(A$,B-1):PW$=MID$(A$,B+1,LEN(A$)-B-1):LV=VAL(RIGHT$(A$,1))
//325 PRINT@128,"USER #"A:PRINT:PRINT"NAME: "NM$:PRINT"PSWD: "PW$:PRINT"LVL :"LV
//330 PRINT@320,STRING$(32,45)TAB(4)"D-LET   UP-BACK   J-UMP",TAB(4)"E-DIT   DN-NEXT   M-ENU"
//335 PRINT@456,"ENTER FUNCTION :"
//340 A$=INKEY$:IFA$=""THEN340ELSEPRINT@472,A$;:LN=INSTR("DEJM"+CHR$(94)+CHR$(10),A$):IFLN=0THENSOUND50,1:GOTO335
//345 SOUND200,1:ONLN GOTO350,365,385,15,390,395
//350 IFA=0THENSOUND1,5:GOTO335
//355 IFA=NM THENNM=NM-1:A=A-1:GOTO315
//360 FORB=A TONM:NM$(B)=NM$(B+1):NEXTB:NM=NM-1:GOTO315
//365 PRINT@198,;:LINEINPUTA$:IFA$=""ORLEN(A$)>20THENPRINT@198,NM$ELSENM$=A$
//370 PRINT@230,;:LINEINPUTA$:IFA$=""ORLEN(A$)>8THENPRINT@230,PW$ELSEPW$=A$
//375 PRINT@262,;:LINEINPUTA$:B=VAL(A$):IFB<1ORB>9THENPRINT@261,LV ELSELV=B
//380 NM$(A)=NM$+"\"+PW$+RIGHT$(STR$(LV),1):GOTO335
//385 PRINT@456," JUMP TO USER # ";:LINEINPUTA$:B=VAL(A$):IFB<0ORB>NM THENSOUND1,5:GOTO335ELSEA=B:GOTO320
//390 A=A-1:IFA<0THENA=NM
//391 GOTO315
//395 A=A+1:IFA>NM THENA=0
//396 GOTO315
//400 'Kill Messages
//405 IFMS$(0,0)=""THENPRINT@454,"*MSG BASE NOT LOADED*":SOUND50,1:GOTO25
//410 CLS:PRINTTAB(9)"KILL MESSAGES:":ING$(32,45)
//415 PRINT@96,"DELETE MESSAGE # 1 -"MS":";:LINEINPUTA$:A=VAL(A$):IFA<1ORA>MS THEN15
//420 A$=MS$(A-1,0):B=INSTR(A$,"\"):C=INSTR(B+1,A$,"\"):T$=LEFT$(A$,B-1):F$=MID$(A$,B+1,C-B-1):S$=RIGHT$(A$,LEN(A$)-C)
//425 PRINT:PRINT"TO: "T$:PRINT"FR: "F$:PRINT"SB: "S$
//430 PRINT:LINEINPUT"DELETE THIS?";A$:IFLEFT$(A$,1)<>"Y"THEN410
//435 IFA=MS THENMS=MS-1:GOTO410
//440 FORB=A-1 TOMS-2:FORC=0TO10:MS$(B,C)=MS$(B+1,C):NEXTC:NEXTB:MS=MS-1:GOTO410

// End of file.

