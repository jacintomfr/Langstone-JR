// ══════════════════════════════════════════════════════════════════════════════
//  Langstone V3H — PlutoSDR GUI  (LangstoneGUI_Pluto.c)
//  Hardware: ADALM-PLUTO, RPi5, LCD 800×480 táctil, GNU Radio 3.10
//  Build: cc LangstoneGUI_Pluto.c -o GUI_Pluto -liio -llgpio -lz -lm
// ══════════════════════════════════════════════════════════════════════════════
//
//  ── SPECTRUM STRETCH ────────────────────────────────────────────────────────
//  Soft-knee non-linear curve applied to spectrum display levels.
//  Purpose: visually stretch signal peaks upward without raising noise floor.
//
//  Implementation (search "Soft-knee stretch" in waterfall()):
//    floor_px = spectrum_rows * 15 / 100   ← noise threshold (15% of height)
//    powf(norm, 0.65f)                     ← stretch exponent
//
//  To adjust — edit and recompile:
//
//  EXPONENT (powf second argument):
//    1.0f  → linear, no stretch (original behaviour)
//    0.8f  → gentle stretch — subtle improvement
//    0.65f → moderate stretch (current default) — signals ~10px higher
//    0.5f  → strong stretch (sqrt) — signals very visible, noise still low
//    0.3f  → aggressive — most signals near top, loses dynamic range
//
//  FLOOR (noise threshold):
//    spectrum_rows * 10 / 100  → lower floor — more signals get stretched
//    spectrum_rows * 15 / 100  → current default (15%)
//    spectrum_rows * 25 / 100  → higher floor — only strong signals stretched
//    Increase if your band noise floor is high (busy HF bands)
//    Decrease for quiet bands (VHF/UHF)
//
//  NOISE COMPRESSION (below floor):
//    lv * 7 / 10  → current (30% compression of noise pixels)
//    lv * 5 / 10  → stronger compression — noise even lower visually
//    lv * 9 / 10  → lighter compression — more natural noise floor
//
//  Example — more aggressive stretch for weak signal work:
//    floor_px = spectrum_rows * 10 / 100;   // lower threshold
//    lv = floor_px + (int)((float)max_ab * powf(norm, 0.5f));  // sqrt stretch
//
//  Example — subtle, almost linear:
//    floor_px = spectrum_rows * 20 / 100;
//    lv = floor_px + (int)((float)max_ab * powf(norm, 0.8f));
//
// ──────────────────────────────────────────────────────────────────────────────
//  ── WATERFALL AUTO NOISE FLOOR ──────────────────────────────────────────────
//  Histogram O(n) + IIR smoothing. Waterfall and spectrum are INDEPENDENT:
//    Spectrum: baselevel = fftref - 45  (fixed 45dB window from FFT Ref)
//    Waterfall: wf_low = wf_low_smooth + WFFloor  (auto + manual offset)
//  WFFloor adjustable in SET menu "WF Level=" (-20 to +20 dB)
//  FFT Ref adjustable in SET menu "FFT Ref=" (spectrum top reference)
//
// ──────────────────────────────────────────────────────────────────────────────
//  ── SETTINGS MENU REFERENCE ─────────────────────────────────────────────────
//  FFT Ref=        Spectrum top reference level (dBFS)
//  WF Level=       Waterfall brightness offset (-20 to +20 dB)
//  AGC Adj=        AGC output level (-20 to +20 dB)
//  Callsign=       Operator callsign display (top-right, textSize=2)
//  CW Ident=       CW ID string (scroll chars, left/right cursor)
//
// ══════════════════════════════════════════════════════════════════════════════

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <iio/iio.h>
#include <unistd.h>
#include <stdlib.h>
#include <lgpio.h>
#include <stdio.h>
#include <fcntl.h>
#include "version.h"
#include <string.h>
#include <math.h>
#include <zlib.h>
#include <stdint.h>
#include <dirent.h>
#include "Graphics.h"
#include "Touch.h"
#include "Mouse.h"
#include "hmi.h"
#include "Morse.c"
#include <netdb.h>
#include <netinet/in.h>                                                                                                      
#include <arpa/inet.h>


//#define PLUTOIP "ip:pluto.local"

char plutoip[30];


void setFreq(double fr);
void displayFreq(double fr);
void setFreqInc();
void setTx(int ptt);
void setPtts(int p);
void setMode(int mode);
void setVolume(int vol);
void setMute(int m);
void setSquelch(int sql);
void setSSBMic(int mic);
void setFMMic(int mic);
void setAMMic(int mic);
void setRxFilter(int low,int high);
void setTxFilter(int low,int high);
void setBandBits(int b);
void processTouch();
void processMouse(int mbut);
void processHmi(int hm);
void setRotation(int rot);
void initGUI();
void sendFifo(char * s);
void initFifos();
void initUDP(void);
void initPluto(void);
void setPlutoRxFreq(long long rxfreq);
void setPlutoTxFreq(long long rxfreq);
void setHwRxFreq(double fr);
void setHwTxFreq(double fr);
void PlutoTxEnable(int txon);
void PlutoRxEnable(int rxon);
void detectHw();
int buttonTouched(int bx,int by);
void setKey(int k);
void displayMenu(void);
void showSettingsMenu(void);
void displaySetting(int se);
void changeSetting(void);
void drawRoundBox(int x1,int y1,int x2,int y2,int r,int g,int b);
void drawButtonIC7300(int x,int y,const char*label,int state);
void drawScaleLabel(int x,int y,const char*label);
void takeSnapshot(void);
char* findUpgradePen(void);
void doUpgrade(char* script);
void setAGC(int mode);
void fb_open(void);
void fb_close(void);
void doGitUpgrade(void);
void drawCallsignDisplay(void);

void processGPIO(void);
void initGPIO(void);
int readConfig(void);
int writeConfig(void);
int satMode(void);
int splitMode(void);
int txvtrMode(void);
int duplexMode(void);
int multMode(void);
void setMoni(int m);
void initSDR(void);
void waterfall(void);
void clearWaterfall(void);
void P_Meter(void);
void S_Meter(void);
void TempC(void);
void setRit(int rit);
void setInputMode(int n);
void gen_palette(char colours[][3],int num_grads);
void setPlutoTxAtt(int att);
void setPlutoRxGain(int gain);
int readPlutoRxGain(void);
void setLimessbGeqH(int gain);
void setLimessbGeqM2(int gain);
void setLimessbGeqM1(int gain);
void setLimessbGeqL(int gain);
void setBand(int b);
void setPlutoGpo(int p);
void setTxPin(int v);
long long runTimeMs(void);                                                    
void clearPopUp(void);
void displayPopupMode(void);
void displayPopupBand(void);
void send1750(void);
void setCTCSS(int t);
void displayError(char*st);
void flushUDP(void);
void setFFTBW(int bw);

int wr_ch_lli(struct iio_channel *chn, const char* what, long long val);
int wr_ch_bool(struct iio_channel *chn, const char* what, bool val);
int wr_ch_dbl(struct iio_channel *chn, const char* what, double val);
int wr_ch_str(struct iio_channel *chn, const char* what, const char* str);
int wr_db_str(struct iio_device *dev, const char* what, const char* str);

int minGain(double freq);
int maxGain(double freq);
void setDialLock(int d);
void setBeacon(int b);
int firstpass=1;
double freq;
double freqInc=0.001;
#define numband 24
int band=3;
#define nummode 6
double bandFreq[numband] = {70.200,144.200,432.200,1296.200,2320.200,2400.100,3400.100,5760.100,10368.200,24048.200,47088.2,10489.55,433.2,433.2,433.2,433.2,433.2,433.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2};
double bandTxOffset[numband]={0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-9936.0,-23616.0,-46656.0,-10069.5,0,0,0,0,0,0,0,0,0,0,0,0};
double bandRxOffset[numband]={0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-9936.0,-23616.0,-46656.0,-10345.0,0,0,0,0,0,0,0,0,0,0,0,0};
double bandRepShift[numband]={0,-0.6,1.6,-6.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int bandTxHarmonic[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int bandRxHarmonic[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int bandMode[numband]={0};
int bandBitsRx[numband]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
int bandBitsTx[numband]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
int bandSquelch[numband][nummode]={0};
int bandFFTRef[numband]={-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10};
int bandTxAtt[numband]={0};
int bandRxGain[numband]={100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100};              //100 is automatic gain
int bandssbGeqH[numband]={52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52,52};
int bandssbGeqM2[numband]={40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40};
int bandssbGeqM1[numband]={20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20};
int bandssbGeqL[numband]={10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10};
int bandDuplex[numband]={0};
int bandCTCSS[numband]={0};
float bandSmeterZero[numband]={-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80,-80};
int bandSSBFiltLow[numband]={300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300,300};
int bandSSBFiltHigh[numband]={3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000,3000};
int bandFFTBW[numband]={0};

#define minFreq 0.0
#define maxFreq 99999.99999
#define minHwFreq 69.9
#define maxHwFreq 5999.99999



int mode=0;
int lastmode=0;
char * modename[nummode]={"USB","LSB","CW ","CWN","FM ","AM "};
enum {USB,LSB,CW,CWN,FM,AM};

#define numSettings 31

#define txX       620
#define txY        15
#define modeX     196
#define modeY      15
#define txvtrX    421
#define txvtrY     15
#define moniX     525
#define moniY      15
#define diallockX 354
#define diallockY  15



// Button states
#define BTN_OFF    0
#define BTN_ON     1
#define BTN_DANGER 2
#define BTN_WARN   3
#define BTN_AMBER  4  // amber style button

char * settingText[numSettings]={"S-Meter Zero= ","FFT Ref= ","Rx Baseband= ","WF Level= ","AGC Adj= ","Spec Stretch= ","SSB Rx Filter High= ","SSB Rx Filter Low= ","SSB Gain EQ-H= ","SSB Gain EQ-M2= ","SSB Gain EQ-M1= ","SSB Gain EQ-L= ","SSB Mic Gain= ","FM Mic Gain= ","AM Mic Gain= ","Tx Att= ","Rx Gain= "," Rx Offset= ","Rx Harmonic Mixing= "," Tx Offset= ","Tx Harmonic Mixing= ","Repeater Shift= ","CTCSS= ","Band Bits (Rx)= ","Band Bits (Tx)= ","24 Bands= ","CW Ident= ","Callsign= ","CWID Carrier= ","CW Break-In Hang Time= ","Rotate Screen = "};
enum {S_ZERO,FFT_REF,RX_BASE,WF_FLOOR,AGC_ADJ,SPEC_STRETCH,SSB_FILT_HIGH,SSB_FILT_LOW,SSB_GEQH,SSB_GEQM2,SSB_GEQM1,SSB_GEQL,SSB_MIC,FM_MIC,AM_MIC,TX_ATT,RX_GAIN,RX_OFFSET,RX_HARMONIC,TX_OFFSET,TX_HARMONIC,REP_SHIFT,CTCSS,BAND_BITS_RX,BAND_BITS_TX,BANDS24,CWID,CALLSIGN,CW_CARRIER,BREAK_IN_TIME,ROTATE};

int settingNo=RX_GAIN;
int setIndex=0;
int maxSetIndex=10;

enum {FREQ,SETTINGS,VOLUME,SQUELCH,RIT};
int inputMode=FREQ;

#define NUMCTCSS 52
int CTCSSTone[NUMCTCSS] = {0,670,693,719,744,770,797,825,854,885,915,948,974,1000,1035,1072,1109,1148,1188,1230,1273,1318,1365,1413,1462,1500,1514,1567,1598,1622,1655,1679,1713,1738,1773,1799,1835,1862,1899,1928,1966,1995,2035,2065,2107,2181,2257,2291,2336,2418,2503,2541};


//GUI Layout values X and Y coordinates for each group of buttons.

#define volButtonX 660
#define volButtonY 300
#define sqlButtonX 30
#define sqlButtonY 300
#define agcButtonX sqlButtonX
#define agcButtonY 210  // AGC button above SQL — centred between MicGain(181) and SQL indicator(275)
#define ritButtonX 660
#define ritButtonY 150
#define funcButtonsY 429
#define funcButtonsX 30
#define buttonHeight 50
#define buttonSpaceY 55
#define buttonWidth 100
#define buttonSpaceX 105
#define freqDisplayX 150
#define freqDisplayY 65
#define freqDisplayCharWidth 40 
#define freqDisplayCharHeight 45
#define versionX 670
#define versionY 15
#define settingX 200
#define settingY 390


// ── doGitUpgrade() — upgrade from GitHub via langstone_upgrade_git.sh ──────
void doGitUpgrade(void)
{
  #define PROGRESS_FILE "/tmp/langstone_upgrade_progress"
  int lineY = settingY - 29;
  char lineBuf[120];
  char lastLine[120] = "";

  // Clear status area
  for(int cy = lineY; cy < settingY + 20; cy++)
    drawLine(0, cy, 799, cy, 0,0,0);
  gotoXY(0, lineY); setForeColour(255,220,0); textSize=1;
  displayStr("A verificar versao...");

  remove(PROGRESS_FILE);
  system("/home/pi/Langstone/langstone_upgrade_git.sh &");

  int timeout = 200 * 10;  // 200s timeout
  int done = 0;
  int success = 0;

  while(timeout-- > 0 && !done)
    {
    usleep(100000);
    FILE *pf = fopen(PROGRESS_FILE, "rt");
    if(!pf) continue;
    if(!fgets(lineBuf, sizeof(lineBuf), pf)) { fclose(pf); continue; }
    fclose(pf);
    int len = strlen(lineBuf);
    if(len > 0 && lineBuf[len-1] == '\n') lineBuf[len-1] = 0;
    if(strcmp(lineBuf, lastLine) == 0) continue;
    strncpy(lastLine, lineBuf, 119);

    // Terminal states
    if(strncmp(lineBuf,"SUCCESS:",8)==0)    { success=1; done=1; continue; }
    if(strncmp(lineBuf,"UP_TO_DATE:",11)==0){ success=2; done=1; continue; }

    // Parse prefix
    char *text = lineBuf; int isErr = 0;
    if(strncmp(lineBuf,"MSG:",4)==0)       text = lineBuf+4;
    else if(strncmp(lineBuf,"OK:",3)==0)   text = lineBuf+3;
    else if(strncmp(lineBuf,"ERR:",4)==0){ text = lineBuf+4; isErr=1; done=1; }

    for(int cy=lineY; cy<lineY+10; cy++) drawLine(0,cy,799,cy,0,0,0);
    gotoXY(0, lineY);
    setForeColour(isErr ? 255 : 255, isErr ? 50 : 220, 0);
    textSize=1;
    char disp[80]; strncpy(disp,text,79); disp[79]=0;
    displayStr(disp);
    }

  // Final message
  for(int cy=lineY; cy<lineY+10; cy++) drawLine(0,cy,799,cy,0,0,0);
  gotoXY(0, lineY);
  if(success == 1)
    {
    // Extract new version from SUCCESS:Vxx-yyy
    char newver[16]=""; sscanf(lastLine,"SUCCESS:%15s",newver);
    char msg[80]; sprintf(msg,"Upgrade OK %s - reinicio em 3s",newver);
    setForeColour(0,220,60); displayStr(msg);
    sleep(3);
    system("/home/pi/Langstone/stop");
    sleep(1);
    system("/home/pi/Langstone/run &");
    exit(0);
    }
  else if(success == 2)
    {
    char curver[16]=""; sscanf(lastLine,"UP_TO_DATE:%15s",curver);
    char msg[80]; sprintf(msg,"Ja actualizado: %s",curver);
    setForeColour(255,220,0); displayStr(msg);
    sleep(2);
    }
  else if(timeout <= 0)
    {
    setForeColour(255,50,0); displayStr("Timeout - ver upgrade.log  ");
    sleep(3);
    }
  else
    {
    setForeColour(255,50,0); displayStr("Falhou - backup restaurado ");
    sleep(3);
    }

  remove(PROGRESS_FILE);
  drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "UPGRDE", BTN_OFF);
}

#define popupX 30
#define popupY 374
#define FFTX 140
#define FFTY 216
#define sMeterX 2
#define sMeterY 5
#define sMeterWidth 170
#define sMeterHeight 56
#define errorX 200
#define errorY 240
#define version "V3Pluto-V1.2"


int gpiohandle;
int ptt=0;
int ptts=0;
int moni=0;
int fifofd;
int sendBeacon=0;
int dotCount=0;
int transmitting=0;
int dialLock=0;
int squelchGate=0;
int lastSquelchGate=0;

int rxFilterLow;
int rxFilterHigh;

int bands24 = 0;
int screenrotate = 0;

// ── Port V3H globals ─────────────────────────────────────────────
// ── Direct framebuffer (waterfall optimisation) ──────────────────────────
#define BAND_BITS_RX BAND_BITS_RX  // Pluto alias
#define FB_DEV     "/dev/fb0"
#define FB_STRIDE  3200        // 800px × 4 bytes 32bpp
static uint32_t wfRowBuf[520];
int bandWFFloor[numband]={0};    // waterfall brightness offset per band
#define WFFloor bandWFFloor[band]
int bandSpecStretch[numband];    // spectrum stretch per band
#define specStretch bandSpecStretch[band]
char callSign[12] = "CU2ED      ";  // operator callsign display (max 11 chars + null)
int  callSignLen  = 5;               // number of active chars in callSign
float fftRowBuf[512];    // single-read FFT row buffer
int   wfHead = 0;        // waterfall ring buffer head
float wf_low_smooth = -999.0f;
int smNeedsFullRedraw = 0;
int statusForceRedraw = 0;  // force S-Meter full redraw
int pmNeedsReset = 0;
int upgradeConfirm = 0;
int agcMode = 2;
int bandAGCAdj[numband]={0};     // AGC level adjust per band
#define AGCAdj bandAGCAdj[band]
int AGCAdjByMode[6] = {0,0,0,0,0,0};  // AGC level memory per mode (USB,LSB,CW,CWN,FM,AM)


int keyDownTimer=0;
int CWIDkeyDownTime=1000;                     //time to put key down between CW Idents (100 per second)

#define configDelay 500                              //delay before config is written after tuning (5 Seconds)
int configCounter=configDelay;

int twoButTimer=0;
int lastBut=0;

int breakInTimer=0;
int breakInTime=100;

long long lastLOhz;

long long lastClock;
long progStartTime=0;

int lastKey=1;

int sMeterType = 0;

int volume=20;
#define maxvol 100

int squelch=20;
#define maxsql 100

int rit=0;
#define minrit -9990
#define maxrit 9990

int SSBMic=50;
#define maxSSBMic 99

int FMMic=50;
#define maxFMMic 99

int AMMic=20;
#define maxAMMic 99

int TxAtt=0;

int tuneDigit=8;
#define maxTuneDigit 11

#define TXDELAY 10000      //10ms delay between setting Tx output bit and sending tx command to SDR
#define RXDELAY 10000       //10ms delay between sending rx command to SDR and setting Tx output bit low. 

#define BurstLength 500000     //length of 1750Hz Burst   500ms

char mousePath[30];
char touchPath[30];
char hmiPath[30];
int mousePresent;
int touchPresent;
int plutoPresent;
int hmiPresent;
int portsdownPresent;



int bandBitsToPluto=0;      //copy low 3 band bits to pluto IO1-IO3

int popupSel=0;
int popupFirstBand;
enum {NONE,MODE,BAND,BEACON};

#define pttPin 17       // Physical pin is 11
#define keyPin 18       // Physical pin is 12
#define txPin 21        // Physical pin is 40      
#define bandPin1 1      //Physical pin is 28
#define bandPin1alt 12  //Physical pin is 32     bandPin1 is copied to both of these pins to retain compatibility with Portsdown.
#define bandPin2 19     //Physical pin is 35
#define bandPin3 4      //Physical pin is 7
#define bandPin4 25     //Physical pin is 22
#define bandPin5 22     //Physical pin is 16
#define bandPin6 24     //Physical pin is 18
#define bandPin7 10     //Physical pin is 19
#define bandPin8 9      //Physical pin is 21


int plutoGpo=0;

//robs Waterfall

float inbuf[2];
FILE *fftstream;
FILE *txfftstream;
float buf[512][130];
int points=512;
int rows=130;
int FFTRef = -30;
int spectrum_rows=80;
unsigned char * palette;
int HzPerBin=94;                        //calculated from FFT width and number of samples. Width=48000 number of samples =512
int bwBarStart=3;
int bwBarEnd=34;
float sMeter;                             //peak reading S meter.
float sMeterPeak;

struct iio_context *plutoctx;
struct iio_device *plutophy;

//UDP server to receive FFT data from GNU RAdio

#define RXPORT 7373
#define TXPORT 7474



int main(int argc, char* argv[])
{
  strcpy(plutoip,"ip:");
  char * penv = getenv("PLUTO_IP");
  if(penv==NULL)
    {
      strcpy(penv,"pluto.local");
    }
  strcat(plutoip,penv);

  printf("plutoip = %s\n",plutoip);
   
  initUDP();
  
  
  lastClock=0;
  readConfig();
  // Init bandSpecStretch defaults (5) — overwritten by readConfig if saved
  for(int _i=0;_i<numband;_i++) if(bandSpecStretch[_i]==0) bandSpecStretch[_i]=5;
  setRotation(screenrotate);
  detectHw();
  initPluto();
  initFifos();
  initScreen();
  initGPIO();
  printf("Initialising Touch at %s\n",touchPath);
  if(touchPresent) initTouch(touchPath);
  printf("Initialising Mouse at %s\n",mousePath);
  if(mousePresent) initMouse(mousePath);
  if(hmiPresent) initHmi(hmiPath);
  initGUI(); 
  initSDR(); 
  //              RGB Vals   Black >  Blue  >  Green  >  Yellow   >   Red     4 gradients    //number of gradients is varaible
  gen_palette((char [][3]){ {0,0,0},{0,0,255},{0,255,0},{255,255,0},{255,0,0}},4);

  
  while(1)
  {
  
    processGPIO();
                                                                                                                    
   if(touchPresent)
     {
       if(getTouch()==1)
        {
         processTouch();
        }
     }
    
    if(mousePresent)
      {
        int but=getMouse();
        if(but>0)
          {
             processMouse(but);
          }
       if(twoButTimer>0) 
       {
        twoButTimer--;
        if(twoButTimer==0)
          {
            lastBut=0;
          }
       }
           
      }
      
    if(hmiPresent)
      {
        int hr=getHmi();
        if(hr > 0)
          {
          processHmi(hr);
          }
      }
            
   
    if(sendBeacon==2)
      {
        dotCount=dotCount+1;
        if(dotCount==1)
          {
            setKey(1);
          }
        if(dotCount==12)
          {
            setKey(0);
          }
        if(dotCount==25)
          {
          dotCount=0;
          }
      } 
      


    if(sendBeacon==1)                                   //sending CWID
    {
      if(keyDownTimer>0)
        {    
          if((keyDownTimer>100) &&( keyDownTimer < CWIDkeyDownTime-100))                                //Key down between Idents
           {
            setKey(1);
           }
           else
           {
            setKey(0);
           }
        keyDownTimer--;
        }    
     else
      {
       int ret=morseKey();                              //get the next key from morse string
       if(ret==-1)                                      // Ident finished
        {
        keyDownTimer=CWIDkeyDownTime + 100;             //key down for this time between idents. Add 1 second to force a minimum key up gap between idents. 
        }
       else
        {  
          setKey(ret);
        }
      }
  
    }




    waterfall();
    TempC();    drawCallsignDisplay();
                                          // status panel — Temp CPU, Tx Att, Rx Gain, Mic Gain
    if(configCounter > 0)
      {
      configCounter--;
      if(configCounter == 0)
        writeConfig();
      }
    
   
   if(firstpass==1)
   {
   setTx(1);                                              //seems to be needed to initialise Pluto
   setTx(0);
   firstpass=0;
   }
    
    while(runTimeMs() < (lastClock + 10))                //delay until the next iteration at 100 per second (10ms)
    {
    usleep(100);
    }
    lastClock=runTimeMs();
  }
}

long long runTimeMs(void)
{
struct timeval tt;
gettimeofday(&tt,NULL);
if(progStartTime==0)
  {
    progStartTime=tt.tv_sec;
  }
tt.tv_sec=tt.tv_sec - progStartTime;
return ((tt.tv_sec*1000) + (tt.tv_usec/1000));
}

void gen_palette(char colours[][3], int num_grads){
  //allocate some memory, size of palette
  palette = malloc(sizeof(char)*256*3);

  int diff[3];
  float scale=256/(num_grads);
  int pos=0;

  for(int i=0;i<num_grads;i++){
      //get differences in colours for current gradient
      diff[0]=(colours[i+1][0]-colours[i][0])/(scale-1);
      diff[1]=(colours[i+1][1]-colours[i][1])/(scale-1);
      diff[2]=(colours[i+1][2]-colours[i][2])/(scale-1);

      //create the palette built up of multiple gradients
      for(int n=0;n<scale;n++){
          palette[pos*3+2]=colours[i][0]+(n*diff[0]);
          palette[pos*3+1]=colours[i][1]+(n*diff[1]);
          palette[pos*3]=colours[i][2]+(n*diff[2]);
          pos++;
      }

  }
}

void flushUDP(void)
{
int ret;
  do
  {
  ret = fread(&inbuf,sizeof(float),1,txfftstream);
  }
  while (ret>0);
  
  do
  {
  ret = fread(&inbuf,sizeof(float),1,fftstream);
  }
  while (ret>0); 
}



// ── Framebuffer direct access helpers ────────────────────────────────────────
void fb_open(void)
{
  // fbfd is declared and opened by Graphics.h (lgpio/hmi library)
  // We just use it directly — no need to reopen
  // If for any reason it's not open, try opening ourselves
  if(fbfd > 0) return;
  fbfd = open(FB_DEV, O_RDWR);
  if(fbfd <= 0)
    fprintf(stderr, "fb_open: cannot open %s\n", FB_DEV);
}

void fb_close(void)
{
  // Do not close fbfd — owned by Graphics.h / lgpio
}

// Write one horizontal row directly to framebuffer
// x,y = top-left pixel, n = number of pixels, buf = BGRA8888 pixel array
static inline void fb_write_row(int x, int y, int n, uint32_t *buf)
{
  if(fbfd < 0) return;
  off_t offset = (off_t)y * FB_STRIDE + (off_t)x * 4;
  lseek(fbfd, offset, SEEK_SET);
  write(fbfd, buf, (size_t)n * 4);
}

// Pack R,G,B into BGRA8888 uint32_t (RPi /dev/fb0 default format)
// If colours appear wrong: swap R and B → use (R<<16)|(G<<8)|B
static inline uint32_t fb_pixel(int R, int G, int B)
{
  // BGRA8888: byte order in memory = B,G,R,A
  // As uint32_t on little-endian: A<<24 | R<<16 | G<<8 | B
  return (uint32_t)((255<<24) | (R<<16) | (G<<8) | B);
}

void waterfall()
{
  int level,level2;
  int ret;
  int baselevel;
  int bwbaroffset;
  float scaling;
  int fftref;
  int centreShift=0;


     // ── 1. Single fread() — one syscall for full 512-float frame ──────
      // (original used 512 individual freads = 51200 syscalls/second)
      if((transmitting==1) && (satMode()==0))
        {
        ret = fread(fftRowBuf, sizeof(float), points, txfftstream);
        fftref=10;
        }
      else
        {
        ret = fread(fftRowBuf, sizeof(float), points, fftstream);
        fftref=FFTRef;
        }

      if(ret >= points)
    {

      // ── 2. Ring buffer advance — zero float copies ────────────────────
      // (original copied 512×130 = 66560 floats per frame)
      wfHead = (wfHead + rows - 1) % rows;

      // ── 3. Store new row, recentring FFT ─────────────────────────────
      for(int p = 0; p < points; p++)
        {
        if(p < points/2)
          buf[p + points/2][wfHead] = fftRowBuf[p];
        else
          buf[p - points/2][wfHead] = fftRowBuf[p];
        }
 
 
        if(((mode==CW)||(mode==CWN)) && (transmitting==1) && (satMode()==0))
          {
          bwbaroffset=800/HzPerBin;
          }
        else
          {
           bwbaroffset=0;
          }
        
 
        //use raw values to calculate S meter value
        //use highest reading within the receiver bandwidth
        
      // ── S-meter peak from current row ──────────────────────────────
      int smShift = transmitting ? bwbaroffset : centreShift;
      sMeterPeak = -200;
      for(int p = points/2 + bwBarStart + smShift;
              p < points/2 + bwBarEnd   + smShift; p++)
        {
        if(p >= 0 && p < points)
          if(buf[p][wfHead] > sMeterPeak)
            sMeterPeak = buf[p][wfHead];
        }
 
    
        //RF level adjustment
    
  if(satMode()==1)
  {
    baselevel=fftref-45;
    scaling=255.0/(float)(fftref-baselevel);
    wf_low_smooth=-999.0f;
  }
  else if(transmitting==1)
  {
    baselevel=fftref-80;
    scaling=255.0/(float)(fftref-baselevel);
  }
  else
  {
    // ── Spectrum baselevel — fixed window from FFTRef, independent of WFFloor
    baselevel = fftref - 45;   // spectrum always uses 45dB window
    scaling = 255.0f / (float)(fftref - baselevel);
    // ── Waterfall noise floor — auto IIR, WFFloor only affects waterfall
    float row_min=buf[0][wfHead],row_max=buf[0][wfHead];
    for(int p=1;p<points;p++){if(buf[p][wfHead]<row_min)row_min=buf[p][wfHead];if(buf[p][wfHead]>row_max)row_max=buf[p][wfHead];}
    float range=row_max-row_min;if(range<1.0f)range=1.0f;
    int hist[32]={0};
    for(int p=0;p<points;p++){int b=(int)((buf[p][wfHead]-row_min)*31.0f/range);if(b<0)b=0;if(b>31)b=31;hist[b]++;}
    int target=points/5,cum=0;float noise_floor=row_min;
    for(int b=0;b<32;b++){cum+=hist[b];if(cum>=target){noise_floor=row_min+(b*range/31.0f);break;}}
    if(wf_low_smooth<-900.0f)wf_low_smooth=noise_floor;
    else if(noise_floor<wf_low_smooth)wf_low_smooth=0.7f*wf_low_smooth+0.3f*noise_floor;
    else wf_low_smooth=0.97f*wf_low_smooth+0.03f*noise_floor;
    // wf_low/wf_high calculated below in waterfall draw — uses WFFloor
  }
        
        
    
        // ── Draw waterfall — piHPSDR hardcoded palette ────────────────
        {
        float wf_low, wf_high;
        if(transmitting==1 && satMode()==0)
          {
          wf_low  = (float)(fftref - 80) + (float)WFFloor;
          wf_high = (float)fftref;
          }
        else
          {
          wf_low  = wf_low_smooth - 5.0f + (float)WFFloor;
          wf_high = wf_low + 55.0f;
          }
        float wf_range = 1.0f / (wf_high - wf_low);
        for(int r=0;r<rows-20;r++)
          {
          int ridx = (wfHead + r) % rows;
          int screen_y = FFTY + 3 + r;
          for(int p=0;p<points;p++)
            {
            // ── TX: black except inside TX filter BW ─────────────────
            if(transmitting==1 && satMode()==0)
              {
              int prel = p - points/2 - bwbaroffset;
              int inBW = (prel >= bwBarStart && prel <= bwBarEnd);
              float v2 = buf[p][ridx];
              if(!inBW || v2 <= wf_low)
                { wfRowBuf[p] = 0; continue; }
              }
            float v = buf[p][ridx];
            int pr,pg,pb;
            if(v <= wf_low)       { pr=0;   pg=0;   pb=64;  }
            else if(v >= wf_high) { pr=200; pg=0;   pb=0;   }
            else
              {
              float pct = (v - wf_low) * wf_range;
              if(pct < 0.222f)      { float t=pct/0.222f;          pr=0;              pg=0;              pb=(int)(64+t*191); }
              else if(pct < 0.333f) { float t=(pct-0.222f)/0.111f; pr=0;              pg=(int)(t*255);   pb=255;             }
              else if(pct < 0.444f) { float t=(pct-0.333f)/0.111f; pr=0;              pg=255;            pb=(int)(255-t*255);}
              else if(pct < 0.555f) { float t=(pct-0.444f)/0.111f; pr=(int)(t*255);   pg=255;            pb=0;               }
              else if(pct < 0.777f) { float t=(pct-0.555f)/0.222f; pr=255;            pg=(int)(255-t*255);pb=0;              }
              else                  { float t=(pct-0.777f)/0.223f; pr=255;            pg=0;              pb=(int)(t*100);    }
              }
            wfRowBuf[p] = fb_pixel(pr, pg, pb);
            }
          // Write full row in one call
          if(fbfd >= 0)
            fb_write_row(FFTX, screen_y, points, wfRowBuf);
          else
            for(int _p=0;_p<points;_p++){
              int _c=wfRowBuf[_p]; setPixel(_p+FFTX,screen_y,(_c>>16)&0xFF,(_c>>8)&0xFF,_c&0xFF);}
          }
        }
    
        // ── Clear spectrum area (drawLine per row) ───────────────────
        for(int r=0;r<spectrum_rows+1;r++)
          drawLine(FFTX, FFTY-r, FFTX+points-1, FFTY-r, 0,0,0);
    
        // ── Outer border + divider ───────────────────────────────────
        {
        int wf_bot = FFTY + 3 + rows;
        drawRoundBox(137, 129, 654, wf_bot+1, 160,160,160);
        drawLine(138, 216, 653, 216, 160,160,160);  // spectrum/waterfall divider
        }
        // ── Spectrum — fill + curve IC-7300 style ────────────────────
        {
        int specLevel[520];
        static float specSmooth[520] = {0};  // IIR smoothed spectrum
        scaling = (float)spectrum_rows / (float)(fftref - baselevel);
        for(int p=0;p<points;p++)
          {
          float v=buf[p][wfHead];
          if(v<baselevel) v=baselevel;
          if(v>fftref) v=fftref;
          float raw = (v - baselevel) * scaling;
          // IIR: α=0.35 → ~3 frame average (~30ms) — removes random FFT spikes
          // Asymmetric attack/decay: fast rise, slow fall
          if(raw > specSmooth[p])
            specSmooth[p] = 0.6f  * raw + 0.4f  * specSmooth[p];  // fast attack
          else
            specSmooth[p] = 0.08f * raw + 0.92f * specSmooth[p];  // slow decay
          int lv = (int)specSmooth[p];
          // ── Soft-knee stretch: noise compressed, signals expanded ─
          {
          // specStretch: 0=linear, 5=default(0.65), 10=max(0.30)
          if(specStretch == 0)
            {
            specLevel[p] = lv;  // linear, no stretch
            }
          else
            {
            float sexp = 1.0f - (float)specStretch * 0.07f;
            int floor_px = spectrum_rows * 15 / 100;
            if(lv <= floor_px)
              lv = lv * 7 / 10;
            else
              {
              int above  = lv - floor_px;
              int max_ab = spectrum_rows - floor_px;
              float norm = (float)above / (float)max_ab;
              lv = floor_px + (int)((float)max_ab * powf(norm, sexp));
              }
            specLevel[p] = lv;
            }
          }
          }
        // ── Spectrum fill — blue gradient, single loop (dark blue→cyan) ──
        for(int p=0;p<points-1;p++)
          {
          int lv=specLevel[p];
          if(lv>2)
            {
            for(int y=0; y<=lv; y++)
              {
              int t=y*255/lv;
              drawLine(p+FFTX,FFTY-y,p+FFTX,FFTY-y,
                       0, t*180>>8, 64+(t*191>>8));
              }
            }
          }
        // Spectrum curve — bright cyan
        for(int p=0;p<points-1;p++)
          drawLine(p+FFTX, FFTY-specLevel[p], p+1+FFTX, FFTY-specLevel[p+1], 180,180,180);


        }
          
          //draw Bandwidth indicator
          int p=points/2;
          
          bwBarStart=rxFilterLow/HzPerBin;
          bwBarEnd=rxFilterHigh/HzPerBin;
          
          if(bwBarStart < -255) bwBarStart = -255;
          if(bwBarEnd  > 255) bwBarEnd = 255;
          
        
          
          if (((mode==CW) || (mode==CWN)) && (transmitting==0 && satMode()== 0))
          {
           centreShift=800/HzPerBin;
          }
          else
          {
           centreShift=0;          
          }

          if(bwBarStart > -255) drawLine(p+FFTX+bwBarStart-bwbaroffset, FFTY-spectrum_rows+5, p+FFTX+bwBarStart-bwbaroffset, FFTY-spectrum_rows,255,140,0);
          drawLine(p+FFTX+bwBarStart-bwbaroffset, FFTY-spectrum_rows, p+FFTX+bwBarEnd-bwbaroffset, FFTY-spectrum_rows,255,140,0);
          
          if(bwBarEnd < 255) drawLine(p+FFTX+bwBarEnd-bwbaroffset, FFTY-spectrum_rows+5, p+FFTX+bwBarEnd-bwbaroffset, FFTY-spectrum_rows,255,140,0);  
          //draw centre line (displayed frequency)
          drawLine(p+FFTX+centreShift, FFTY-10, p+FFTX+centreShift, FFTY-spectrum_rows,255,0,0);  
          
          


		  		            // ── X axis scale — fixed strip, redrawn only on BW change ────
          {
          static int lastScaleBW = -1;
          int p2 = points/2;
          int ticks2[11] = {0,21,43,64,85,107,128,149,171,192,213};
          int scaleY2 = FFTY + 3 + rows - 20;
          if(lastScaleBW != bandFFTBW[band])
            {
            lastScaleBW = bandFFTBW[band];
            for(int r=scaleY2;r<scaleY2+20;r++)
              drawLine(FFTX,r,FFTX+points-1,r,0,0,0);
            drawLine(FFTX,scaleY2,FFTX+points,scaleY2,0,180,220);
            for(int tick=0;tick<11;tick++)
              {
              drawLine(FFTX+p2+ticks2[tick],scaleY2,FFTX+p2+ticks2[tick],scaleY2+3,0,180,220);
              drawLine(FFTX+p2-ticks2[tick],scaleY2,FFTX+p2-ticks2[tick],scaleY2+3,0,180,220);
              }
            int sy2 = scaleY2 + 4;
            drawScaleLabel(p2+FFTX-6, sy2, "0");
            switch(bandFFTBW[band])
              {
              case 0:
                drawScaleLabel(p2+FFTX-ticks2[5]-24,  sy2, "-10k");
                drawScaleLabel(p2+FFTX-ticks2[10]-24, sy2, "-20k");
                drawScaleLabel(p2+FFTX+ticks2[5]-24,  sy2, "+10k");
                drawScaleLabel(p2+FFTX+ticks2[10]-24, sy2, "+20k");
                break;
              case 1:
                drawScaleLabel(p2+FFTX-ticks2[5]-18,  sy2, "-5k");
                drawScaleLabel(p2+FFTX-ticks2[10]-24, sy2, "-10k");
                drawScaleLabel(p2+FFTX+ticks2[5]-18,  sy2, "+5k");
                drawScaleLabel(p2+FFTX+ticks2[10]-24, sy2, "+10k");
                break;
              case 2:
                drawScaleLabel(p2+FFTX-ticks2[5]-30,  sy2, "-2.5k");
                drawScaleLabel(p2+FFTX-ticks2[10]-18, sy2, "-5k");
                drawScaleLabel(p2+FFTX+ticks2[5]-30,  sy2, "+2.5k");
                drawScaleLabel(p2+FFTX+ticks2[10]-18, sy2, "+5k");
                break;
              case 3:
                drawScaleLabel(p2+FFTX-ticks2[5]-36,  sy2, "-1.25k");
                drawScaleLabel(p2+FFTX-ticks2[10]-30, sy2, "-2.5k");
                drawScaleLabel(p2+FFTX+ticks2[5]-36,  sy2, "+1.25k");
                drawScaleLabel(p2+FFTX+ticks2[10]-30, sy2, "+2.5k");
                break;
              }
            }
          }
		  		  
          if((transmitting==0) || (satMode()==1))
          {
          S_Meter();
          }
          
          else
          {
          P_Meter();
          }
   }       
}


// ── S-Meter geometry ─────────────────────────────────────────────
#define SM_NUM_SEG   20
#define SM_SEG_W     7
#define SM_SEG_GAP   1
#define SM_BAR_H     10
#define SM_TRI_H     6
#define SM_GAP       2
#define SM_S9_SEG    14
#define SM_MARGIN    3
#define SM_TXT_H     14
#define SM_BAR_X     (sMeterX + 4)
#define SM_TRI_Y     (sMeterY + SM_MARGIN)
#define SM_BAR_Y     (SM_TRI_Y + SM_TRI_H + SM_GAP)
#define SM_TXT_Y     (SM_BAR_Y + SM_BAR_H + SM_GAP + SM_TXT_H - 2)

static inline void smDrawSeg(int seg, int lit)
{
  int sx=SM_BAR_X+seg*(SM_SEG_W+SM_SEG_GAP);
  int r,g,b;
  if(seg<SM_S9_SEG){r=0;g=210;b=40;}
  else if(seg<SM_S9_SEG+2){r=230;g=185;b=0;}
  else{r=240;g=30;b=0;}
  if(!lit){r/=4;g/=4;b/=4;}
  for(int ln=0;ln<SM_BAR_H;ln++)
    drawLine(sx,SM_BAR_Y+ln,sx+SM_SEG_W-1,SM_BAR_Y+ln,r,g,b);
}

static inline void smDrawTriangle(int sqx,int visible)
{
  drawLine(sqx-3,SM_TRI_Y+0,sqx+3,SM_TRI_Y+0,0,visible?200:0,visible?200:0);
  drawLine(sqx-2,SM_TRI_Y+1,sqx+2,SM_TRI_Y+1,0,visible?210:0,visible?210:0);
  drawLine(sqx-1,SM_TRI_Y+2,sqx+1,SM_TRI_Y+2,0,visible?220:0,visible?220:0);
  drawLine(sqx,  SM_TRI_Y+3,sqx,  SM_TRI_Y+3,0,visible?230:0,visible?230:0);
  drawLine(sqx-2,SM_TRI_Y+4,sqx+2,SM_TRI_Y+4,0,visible?210:0,visible?210:0);
  drawLine(sqx,  SM_TRI_Y+5,sqx,  SM_TRI_Y+5,0,visible?200:0,visible?200:0);
}

void P_Meter(void)
{
          //  -------------------------------------------------------
          //  P-Meter TX — LED bargraph segmentado (mesmo estilo S-Meter)
          //  Sem squelch, sem peak-hold. Decay diferenciado por modo.
          //  -------------------------------------------------------

          char smStr[10];
          static int sMeterCount = 0;

          sMeterPeak = (sMeterPeak+50)*2.1;

          if(sMeterPeak < 0)   sMeterPeak = 0;
          if(sMeterPeak > 100) sMeterPeak = 100;    // escala 0-100

          if(sMeterPeak >= sMeter)
            {
            sMeter = sMeterPeak;                    // fast attack
            }
          else
            {
            if(sMeter > 0)
              {
              if((mode==CW) || (mode==CWN))
                {
                sMeter = sMeter - 10;               // fast decay CW
                }
              else if((mode==USB) || (mode==LSB))
                {
                sMeter = sMeter - 3;                // slow decay SSB
                }
              else
                {
                sMeter = sMeter - 0.5;              // very slow decay FM/AM
                }
              }
            }

          if(sMeter < 0) sMeter = 0;

          // segmentos acesos (sMeter 0..100 → 0..20 segs)
          int litSegs = (int)(sMeter * SM_NUM_SEG / 100);
          if(litSegs > SM_NUM_SEG) litSegs = SM_NUM_SEG;

          // ── caixa — só na primeira vez ────────────────────────────
          static int pmBorderDrawn = 0;
          static int pmLastLitSegs = -1;
          static int pmTextDrawn = 0;
          if(pmNeedsReset)
            {
            pmBorderDrawn = 0;
            pmLastLitSegs = -1;
            pmTextDrawn   = 0;
            pmNeedsReset  = 0;
            }
          if(!pmBorderDrawn || pmLastLitSegs == -1)
            {
            drawRoundBox(sMeterX, sMeterY, sMeterX+sMeterWidth, sMeterY+sMeterHeight, 160,160,160);
            for(int r = sMeterY+1; r < sMeterY+sMeterHeight; r++)
              drawLine(sMeterX+1, r, sMeterX+sMeterWidth-1, r, 0,0,0);
            // draw all segments
            for(int s=0; s<SM_NUM_SEG; s++)
              {
              int sx = SM_BAR_X + s*(SM_SEG_W+SM_SEG_GAP);
              int r,g,b;
              if(s < SM_S9_SEG)        { r=0;   g=150; b=255; }
              else if(s < SM_S9_SEG+2) { r=0;   g=210; b=170; }
              else                      { r=255; g=50;  b=0;   }
              int lit = s < litSegs;
              for(int ln=0; ln<SM_BAR_H; ln++)
                drawLine(sx, SM_BAR_Y+ln, sx+SM_SEG_W-1, SM_BAR_Y+ln,
                         lit?r:r/4, lit?g:g/4, lit?b:b/4);
              }
            pmBorderDrawn = 1;
            pmLastLitSegs = litSegs;
            }

          // ── segmentos — redesenho diferencial ─────────────────────
          if(litSegs != pmLastLitSegs)
            {
            if(litSegs > pmLastLitSegs)
              {
              for(int s=pmLastLitSegs; s<litSegs; s++)
                {
                int sx = SM_BAR_X + s*(SM_SEG_W+SM_SEG_GAP);
                int r,g,b;
                if(s < SM_S9_SEG)        { r=0;   g=150; b=255; }
                else if(s < SM_S9_SEG+2) { r=0;   g=210; b=170; }
                else                      { r=255; g=50;  b=0;   }
                for(int ln=0; ln<SM_BAR_H; ln++)
                  drawLine(sx, SM_BAR_Y+ln, sx+SM_SEG_W-1, SM_BAR_Y+ln, r,g,b);
                }
              }
            else
              {
              for(int s=litSegs; s<pmLastLitSegs; s++)
                {
                int sx = SM_BAR_X + s*(SM_SEG_W+SM_SEG_GAP);
                int r,g,b;
                if(s < SM_S9_SEG)        { r=0;   g=150; b=255; }
                else if(s < SM_S9_SEG+2) { r=0;   g=210; b=170; }
                else                      { r=255; g=50;  b=0;   }
                for(int ln=0; ln<SM_BAR_H; ln++)
                  drawLine(sx, SM_BAR_Y+ln, sx+SM_SEG_W-1, SM_BAR_Y+ln, r/4,g/4,b/4);
                }
              }
            pmLastLitSegs = litSegs;
            }

          // ── texto — só uma vez (não pisca) ────────────────────────
          if(!pmTextDrawn)
            {
            textSize = 2;
            setForeColour(0,238,68);
            gotoXY(sMeterX+10, SM_TXT_Y);
            displayStr("Tx Level");
            pmTextDrawn = 1;
            }

          // Reset P_Meter state when TX ends (next RX call to S_Meter)
          smNeedsFullRedraw = 1;
          pmBorderDrawn = 1;   // keep border, only reset on next TX start
}


void drawCallsignDisplay(void)
{
  // Display operator callsign top-right in textSize=2
  // Only show chars up to first _ (space placeholder) — no _ in display
  static char lastDisplayed[12] = "";
  if(strcmp(callSign, lastDisplayed) == 0) return;  // only redraw on change
  strncpy(lastDisplayed, callSign, 11);
  // Build clean string: stop at first _ or null
  char clean[12] = "           ";  // 11 spaces to clear old chars
  int len = 0;
  for(int i=0; i<11 && callSign[i] != 0; i++)
    {
    if(callSign[i] == 95) break;  // stop at _ (space placeholder)
    clean[i] = callSign[i];
    len = i+1;
    }
  gotoXY(670,14);
  setForeColour(255,220,0);
  textSize=2;
  displayStr(clean);            // always 11 chars to clear leftovers
  textSize=1;
}

void S_Meter(void)
{

          char smStr[10];
          static int sMeterCount=0;
         
          sMeterPeak=sMeterPeak-bandSmeterZero[band];                   //adjust offset to give positive values for s-meter
          int dbOver=0;
          int sValue=0;
          
          if(bandRxGain[band]==100)                  //if we are in RF AGC mode
            {
             sMeterPeak=sMeterPeak + maxGain(freq) - readPlutoRxGain();       //compensate for reduced gain due to AGC action
            }       
            
          if(sMeterPeak < 0) sMeterPeak=0;
          if(sMeterPeak >= sMeter)
            {
            sMeter=sMeterPeak;                                    //fast attack
            } 
          else
            {
            if(sMeter > 0) sMeter=sMeter-2;                    //slow decay
            }
            
 
          if(sMeter<55)
            {
            sValue=sMeter/6;
            }
          else
            {
            sValue=9;
            dbOver=sMeter-54;
            }
 
          
          
          // ── LED bargraph ─────────────────────────────────────────
          static int lastLitSegs=-1,lastSqSeg=-1,lastSquelch=-1,borderDrawn=0;
          static int lastGated=-1,lastSValue=-1,lastDbOver=-1,lastSmType=-1;
          int litSegs=(int)(sMeter*SM_NUM_SEG/80); if(litSegs>SM_NUM_SEG)litSegs=SM_NUM_SEG;
          int sqSegs=(int)(squelch*SM_NUM_SEG/80); if(sqSegs>SM_NUM_SEG-1)sqSegs=SM_NUM_SEG-1;
          if(statusForceRedraw||smNeedsFullRedraw)
            {lastLitSegs=-1;smNeedsFullRedraw=0;
             for(int r=sMeterY+1;r<sMeterY+sMeterHeight;r++)
               drawLine(sMeterX+1,r,sMeterX+sMeterWidth-1,r,0,0,0);}
          if(!borderDrawn||lastLitSegs==-1)
            {drawRoundBox(sMeterX,sMeterY,sMeterX+sMeterWidth,sMeterY+sMeterHeight,160,160,160);
             for(int r=sMeterY+1;r<sMeterY+sMeterHeight;r++)
               drawLine(sMeterX+1,r,sMeterX+sMeterWidth-1,r,0,0,0);
             for(int s=0;s<SM_NUM_SEG;s++)smDrawSeg(s,s<litSegs);
             borderDrawn=1;lastLitSegs=litSegs;lastSquelch=-1;}
          if(litSegs!=lastLitSegs)
            {if(litSegs>lastLitSegs)for(int s=lastLitSegs;s<litSegs;s++)smDrawSeg(s,1);
             else for(int s=litSegs;s<lastLitSegs;s++)smDrawSeg(s,0);
             lastLitSegs=litSegs;}
          if(sqSegs!=lastSqSeg||squelch!=lastSquelch)
            {if(lastSqSeg>=0&&lastSquelch>0){int ox=SM_BAR_X+lastSqSeg*(SM_SEG_W+SM_SEG_GAP)+SM_SEG_W/2;smDrawTriangle(ox,0);}
             if(squelch>0){int sqx=SM_BAR_X+sqSegs*(SM_SEG_W+SM_SEG_GAP)+SM_SEG_W/2;smDrawTriangle(sqx,1);}
             lastSqSeg=sqSegs;lastSquelch=squelch;}
 
  // ── Texto — a cada 6 frames (~60ms), só se valor mudou ────────
  sMeterCount++;
  if(sMeterCount > 5)
    {
    sMeterCount = 0;
    int gated = ((sMeter < squelch) && (squelch > 0)) ? 1 : 0;
    int textChanged = (sValue != lastSValue || dbOver != lastDbOver ||
                       gated  != lastGated  || sMeterType != lastSmType);
    if(textChanged)
      {
      textSize = 2;
      if(gated)
        setForeColour(0,80,0);
      else
        setForeColour(0,238,68);

      gotoXY(sMeterX+4, SM_TXT_Y);

      if(sMeterType == 0)
        {
        sprintf(smStr,"S%d",sValue);
        displayStr(smStr);
        if(dbOver > 0)
          {
          setForeColour(255,85,0);
          sprintf(smStr,"+%ddB ",dbOver);
          displayStr(smStr);
          }
        else
          {
          displayStr("      ");
          }
        }
      else
        {
        int dbm = (int)(sMeter + bandSmeterZero[band]);
        sprintf(smStr,"%ddBm   ", dbm);
        displayStr(smStr);
        }

      // SQL label — só quando squelch activo
      textSize = 1;
      setForeColour(255,255,255);
      gotoXY(sMeterX+sMeterWidth-40, SM_TXT_Y);
      if(squelch > 0)
        displayStr("SQL");
      else
        displayStr("   ");

      lastSValue  = sValue;
      lastDbOver  = dbOver;
      lastGated   = gated;
      lastSmType  = sMeterType;
      }
    }
 

  // ── Squelch gate ─────────────────────────────────────────────
  int gated2 = ((sMeter < squelch) && (squelch > 0)) ? 1 : 0;
  if(gated2)
    {
    squelchGate = 0;
    if(squelchGate != lastSquelchGate)
      { setMute(1); lastSquelchGate = squelchGate; }
    }
  else
    {
    squelchGate = 1;
    if(squelchGate != lastSquelchGate)
      { setMute(0); lastSquelchGate = squelchGate; }
    }



}


void TempC(void)
{
  #define TEMPC_INTERVAL 50   // 50 × 10ms = 500ms
  static int  tempCounter  = 0;
  static long lastTempRaw  = -1;
  static int  last_TxAtt   = -99;
  static int  last_RxGain  = -99;
  static int  last_Mic     = -99;
  static int  last_mode    = -1;
  // CPU% via /proc/stat delta
  static long long last_cpu_total = 0;
  static long long last_cpu_idle  = 0;
  static int  last_cpu_pct   = -1;

  char vStr[16];
  int  tempTick = 0;
  tempCounter++;
  if(tempCounter >= TEMPC_INTERVAL)
    {
    tempCounter = 0;
    tempTick = 1;
    }

  // ── Temperatura + CPU% — só a cada 500ms ────────────────────────
  if(tempTick || lastTempRaw == -1)
    {
    // Temperature
    char vtemp[12]; char *end;
    FILE *tmp = fopen("/sys/class/thermal/thermal_zone0/temp","rt");
    if(tmp != NULL)
      {
      fread(vtemp, 10, 1, tmp);
      fclose(tmp);
      long raw = strtol(vtemp, &end, 10);
      if(raw != lastTempRaw || lastTempRaw == -1)
        {
        lastTempRaw = raw;
        textSize=1; setForeColour(255,255,0);
        gotoXY(5,110);  displayStr("Temp CPU");
        sprintf(vStr,"=%5.1fC  ", raw/1000.0);
        gotoXY(70,110); displayStr(vStr);
        }
      }
    // CPU% — read /proc/stat aggregate line
    FILE *stat = fopen("/proc/stat","rt");
    if(stat != NULL)
      {
      char line[128];
      fgets(line, sizeof(line), stat);
      fclose(stat);
      // cpu user nice system idle iowait irq softirq steal guest guest_nice
      long long u,n,s,id,iow,irq,sirq,st;
      if(sscanf(line,"cpu %lld %lld %lld %lld %lld %lld %lld %lld",
                &u,&n,&s,&id,&iow,&irq,&sirq,&st) == 8)
        {
        long long total = u+n+s+id+iow+irq+sirq+st;
        long long idle  = id+iow;
        long long dtotal = total - last_cpu_total;
        long long didle  = idle  - last_cpu_idle;
        int pct = 0;
        if(dtotal > 0)
          pct = (int)(100LL * (dtotal - didle) / dtotal);
        if(pct < 0)   pct = 0;
        if(pct > 100) pct = 100;
        last_cpu_total = total;
        last_cpu_idle  = idle;
        if(pct != last_cpu_pct || last_cpu_pct == -1)
          {
          last_cpu_pct = pct;
          textSize=1;
          setForeColour(255,220,0);
          gotoXY(5,123);  displayStr("CPU%=");
          sprintf(vStr,"%3d%%  ", pct);
          gotoXY(50,123); displayStr(vStr);
          }
        }
      }
    }
      
//>>> STATUS de TX GAIN,RX GAIN,MIC_Gain e FM DEV

  // ── Tx Att — só redesenha quando muda ───────────────────────────
  if(bandTxAtt[band] != last_TxAtt)
    {
    last_TxAtt = bandTxAtt[band];
    textSize=1; setForeColour(255,255,0);
    gotoXY(5,136);  displayStr("Tx Att= ");
    sprintf(vStr,"%d  ", last_TxAtt);
    gotoXY(70,136); displayStr(vStr);
    }

  // ── Rx Gain — só redesenha quando muda ───────────────────────────
  {
  int curRxGain = bandRxGain[band];
  if(curRxGain != last_RxGain)
    {
    last_RxGain = curRxGain;
    textSize=1; setForeColour(255,255,0);
    gotoXY(5,149); displayStr("Rx Gain= ");
    if(curRxGain > maxGain(freq))
      sprintf(vStr,"Auto  ");
    else
      sprintf(vStr,"%d dB  ", curRxGain);
    gotoXY(75,149); displayStr(vStr);
    }
  }

  // ── Mic Gain — só redesenha quando muda ou modo muda ─────────────
  {
  int curMic = (mode==FM) ? FMMic : (mode==AM) ? AMMic : SSBMic;
  if(curMic != last_Mic || mode != last_mode)
    {
    last_Mic  = curMic;
    last_mode = mode;
    textSize=1;
    gotoXY(5,162);
    if((mode==USB)||(mode==LSB)||(mode==AM)||(mode==FM))
      {
      setForeColour(255,220,0);
      displayStr("Mic Gain= ");
      sprintf(vStr,"%d  ", curMic);
      gotoXY(80,162); displayStr(vStr);
      }
    else
      {
      setForeColour(0,0,0);
      displayStr("            ");
      }
    }
  }
}

void detectHw()
{
  FILE * fp;
  char * ln=NULL;
  size_t len=0;
  ssize_t rd;
  int p;
  char handler[3][20];
  char * found;
  p=0;
  mousePresent=0;
  touchPresent=0;
  hmiPresent=0;
  portsdownPresent=0;
  fp=fopen("/proc/bus/input/devices","r");
   while ((rd=getline(&ln,&len,fp)!=-1))
    {
      if(ln[0]=='N')        //name of device
      {
        p=0;
        if((strstr(ln,"FT5406")!=NULL) || (strstr(ln,"pi-ts")!=NULL) || (strstr(ln,"ft5x06")!=NULL))         //Found Raspberry Pi TouchScreen entry
          {
           p=1;                                //we have found the touchscreen
          }  
          
        if(strstr(ln,"C-Media")!=NULL)         //Found HMI device on the CM108 sound card
          {
           p=2;                                //hmi device on the sound card
          }                                                                 
      }
      
      if(ln[0]=='H')        //handlers
      {
         if(strstr(ln,"mouse")!=NULL)
         {
           found=strstr(ln,"event");        
           strcpy(handler[p],found); 
           handler[p][strlen(found)-2] = 0;         
           if(p==0)                                 //not the touch screen so assume it is a normal mouse
            {
              sprintf(mousePath,"/dev/input/%s",handler[0]);
              mousePresent=1;
              printf("Found Mouse at %s\n",mousePath); 
            }
           if(p==1)                                 //touch screen 
           {
             sprintf(touchPath,"/dev/input/%s",handler[1]);
             touchPresent=1;
             printf("Found Touch at %s\n",touchPath);
           }
         }
         if((strstr(ln,"kbd")!=NULL) && (p==2))             //found the HMI Entry for the CM108 Sound card
          {
           found=strstr(ln,"event");        
           strcpy(handler[p],found); 
           handler[p][strlen(found)-2] = 0;         
           sprintf(hmiPath,"/dev/input/%s",handler[2]);
           hmiPresent=1;
           printf("Found HMI Device at %s\n",hmiPath); 
          }       
      }   
    }
  fclose(fp);
  if(ln)  free(ln);
  
  if ((fp = fopen("/home/pi/rpidatv/bin/rpidatvgui", "r")))                      //test to see if Portsdown file is present. If so we change the exit behaviour. 
  {
    fclose(fp);
    portsdownPresent=1;
  }
    
    plutoPresent=1;      //this will be reset by setPlutoFreq if Pluto is not present.  
  
}

void displayError(char*st)
{
  gotoXY(errorX,errorY);
  setForeColour(255,0,0);
  textSize=2;
  displayStr(st);
}

void initPluto(void)
{
    plutoctx = iio_create_context(NULL,plutoip);
      if(iio_err(plutoctx)!=0)
      {
        plutoPresent=0;
        displayError("Pluto not responding");
        return;
      }
      else
      {
      plutophy = iio_context_find_device(plutoctx, "ad9361-phy"); 
      }
}


/* write Pluto Channel attribute: long long int */
int wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	const struct iio_attr *attr = iio_channel_find_attr(chn, what);
  
    if((iio_attr_write_longlong(attr, val)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}


/* write Pluto Channel attribute: String */
int wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	const struct iio_attr *attr = iio_channel_find_attr(chn, what);
  
    if((iio_attr_write_string(attr, str)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}


/* write Pluto Debug attribute: String */
int wr_db_str(struct iio_device *dev, const char* what, const char* str)
{
	const struct iio_attr *attr = iio_device_find_debug_attr(dev, what);
  
    if((iio_attr_write_string(attr, str)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}


/* write Pluto Channel attribute: Double */
int wr_ch_dbl(struct iio_channel *chn, const char* what, double val)
{
	const struct iio_attr *attr = iio_channel_find_attr(chn, what);
  
    if((iio_attr_write_double(attr, val)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}




/* write Pluto Channel attribute: bool */
int wr_ch_bool(struct iio_channel *chn, const char* what, bool val)
{
	const struct iio_attr *attr = iio_channel_find_attr(chn, what);
  
    if((iio_attr_write_bool(attr, val)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}

/* Read Pluto Channel attribute: Double */
int rd_ch_dbl(struct iio_channel *chn, const char* what, double* val)
{
	const struct iio_attr *attr = iio_channel_find_attr(chn, what);
  
    if((iio_attr_read_double(attr, val)) !=0)
      {
        return -1;
      }        
    else
      {
        return 0;
      }  
}

void setPlutoRxFreq(long long rxfreq)
{
int ret;
   if(plutoPresent)
    { 
      ret=wr_ch_lli(iio_device_find_channel(plutophy, "altvoltage0", true),"frequency", rxfreq); //Rx LO Freq
      if(ret<0)
      {
      displayError("Pluto not responding");
      }
    }
  
}

void setPlutoTxFreq(long long txfreq)
{
int ret;
   if(plutoPresent)
    { 
      ret=wr_ch_lli(iio_device_find_channel(plutophy, "altvoltage1", true),"frequency", txfreq); //Tx LO Freq
   if(ret<0)
      {
      displayError("Pluto not responding");
      }
    }
  
}

void setPlutoTxAtt(int att)
{ 
  if(plutoPresent)
    {
      wr_ch_dbl(iio_device_find_channel(plutophy, "voltage0", true),"hardwaregain", (double)att); //set Tx Attenuator     
    }
}

void setPlutoRxGain(int gain)
{ 
  if(plutoPresent)
    {
     if(gain>maxGain(freq))
        {
          wr_ch_str(iio_device_find_channel(plutophy, "voltage0", false),"gain_control_mode", "slow_attack");  //set Auto Gain
        }
        else
        {
        wr_ch_str(iio_device_find_channel(plutophy, "voltage0", false),"gain_control_mode", "manual");  //set Manual  Gain control
        wr_ch_dbl(iio_device_find_channel(plutophy, "voltage0", false),"hardwaregain", (double)gain); //set Rx Gain 
        }
    } 
}


int readPlutoRxGain(void)
{
double ret;
      if(plutoPresent)
      {
        rd_ch_dbl(iio_device_find_channel(plutophy, "voltage0", false),"hardwaregain", &ret); //Read current Rx Gain
        return (int) ret; 
      }
      else
      {
      return 73;
      }

}



void PlutoTxEnable(int txon)
{
  if(plutoPresent)
    { 
      if(txon==0)
        {
        wr_ch_bool(iio_device_find_channel(plutophy, "altvoltage1", true),"powerdown", true); //turn off TX LO
        }
      else
        {
        wr_ch_bool(iio_device_find_channel(plutophy, "altvoltage1", true),"powerdown", false); //turn on TX LO
        }
    }

}

void PlutoRxEnable(int rxon)
{
  if(plutoPresent)
    {
      if(rxon==0)
        {
        wr_ch_bool(iio_device_find_channel(plutophy, "altvoltage0", true),"powerdown", true); //turn off RX LO
        }
      else
        {
        wr_ch_bool(iio_device_find_channel(plutophy, "altvoltage0", true),"powerdown", false); //turn on RX LO
        }
    }

}

void setPlutoGpo(int p)
{
  char pins[10]; 
   
  sprintf(pins,"0x27 0x%x0",p);
  pins[9]=0;

  if(plutoPresent)
    {
      wr_db_str(plutophy,"direct_reg_access",pins);
    }
}


void initUDP(void)
{
   struct sockaddr_in myaddr;
   int fdr; 
   int fdt; 
   
//initialise Receive UDP receiver for FFT Receiver stream
   fdr=socket(AF_INET,SOCK_DGRAM,0);
   memset((char *)&myaddr,0,sizeof(myaddr));                      //Set any valid address for receiving UDP packets
   myaddr.sin_family = AF_INET;                                  //Network Connection
   myaddr.sin_addr.s_addr = htonl(INADDR_ANY);                   //Any Address
   myaddr.sin_port = htons(RXPORT);                              //set UDP POrt to listen on
   bind(fdr,(struct sockaddr *)&myaddr,sizeof(myaddr));          //bind the socket to the address  
   fftstream=fdopen(fdr,"r");                                    //open as a stream
   fcntl(fileno(fftstream), F_SETFL, O_RDONLY | O_NONBLOCK);    //set it as nonblocking
   
//initialise Receive UDP receiver for FFT Transmitter stream
   fdr=socket(AF_INET,SOCK_DGRAM,0);
   memset((char *)&myaddr,0,sizeof(myaddr));                      //Set any valid address for receiving UDP packets
   myaddr.sin_family = AF_INET;                                   //Network Connection
   myaddr.sin_addr.s_addr = htonl(INADDR_ANY);                   //Any Address
   myaddr.sin_port = htons(TXPORT);                               //set UDP POrt to listen on
   bind(fdr,(struct sockaddr *)&myaddr,sizeof(myaddr));          //bind the socket to the address  
   txfftstream=fdopen(fdr,"r");                                  //open as a stream
   fcntl(fileno(txfftstream), F_SETFL, O_RDONLY | O_NONBLOCK);   //set it as nonblocking
      
}



void initFifos()
{
 if(access("/tmp/langstoneTRx",F_OK)==-1)   //does fifo exist already?
    {
        mkfifo("/tmp/langstoneTRx", 0666);
    }
}


void sendFifo(char * s)
{
  char fs[50];
  int ret;
  int retry;
  int success;
  strcpy(fs,s);
  strcat(fs,"\n");
  
  success = 0;
  do
  {
  fifofd=open("/tmp/langstoneTRx",O_WRONLY|O_NONBLOCK);
  retry=0;
    do
     {
       ret=write(fifofd,fs,strlen(fs));
       usleep(5000);
       retry++;
     }
   while((ret==-1)&(retry<10));   
  if(ret==-1)
    {
      displayError("Waiting for Lang_TRX.py");
      success = 0;
     }
  else
     {
     success = 1;
     }
  close(fifofd);
  usleep(1000);
  }
  while(success == 0);
}

void initGPIO(void)
{
  gpiohandle = lgGpiochipOpen(4);
  lgGpioClaimInput(gpiohandle,LG_SET_PULL_UP,pttPin); 
  lgGpioClaimInput(gpiohandle,LG_SET_PULL_UP,keyPin); 
  lgGpioClaimOutput(gpiohandle,0,txPin,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin1,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin1alt,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin2,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin3,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin4,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin5,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin6,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin7,0); 
  lgGpioClaimOutput(gpiohandle,0,bandPin8,0); 
  lastKey=1;
}

void processGPIO(void)
{
int p1=1;
int k1=1;

  p1=lgGpioRead(gpiohandle,pttPin);
  k1=lgGpioRead(gpiohandle,keyPin);

  
    if(p1==0)             //if the hardware PTT has been pressed
        {
          if(ptt==0)
            {
              ptt=1;
              if(ptts==1) setPtts(0);                       //turn off the touchscreen PTT.
              setTx(ptt|ptts);
            }
        }
     else
        {
          if(ptt==1)
            {
              ptt=0;
              setTx(ptt|ptts);
            }
        }

      if(k1!=lastKey)
    {    
    setKey(!k1);
    lastKey=k1;
    } 

    if((mode==CW) ||(mode==CWN))
      {
        if(k1==0)          //key down
          {
            if((ptt|ptts)==0)   //not transmitting
              { 
                setTx(1);    
              }
            breakInTimer=breakInTime;
          }
        else
          {
            if((breakInTimer>0) & ((ptt|ptts)==0))
            {
            breakInTimer--;
            if(breakInTimer==0)
              {
              setTx(0);
              }
            }
          }
       }
     
}



void sqlButton(int show)
{
  char sqlStr[5];
  drawButtonIC7300(sqlButtonX, sqlButtonY, "SQL", show==1 ? BTN_OFF : BTN_OFF);
  // value readout above button (original position, original style)
  textSize=2;
  setForeColour(255,220,0);
  gotoXY(sqlButtonX+30,sqlButtonY-25);
  displayStr("   ");
  gotoXY(sqlButtonX+30,sqlButtonY-25);
  sprintf(sqlStr,"%d",squelch);
  displayStr(sqlStr);
}

void ritButton(int show)
{
  char ritStr[5];
  int to;
  if(show==1)
    {
    // Show RIT button — active (cyan) only if inputMode==RIT, else grey
    int ritState = (inputMode==RIT) ? BTN_ON : BTN_OFF;
    drawButtonIC7300(ritButtonX, ritButtonY, "RIT", ritState);
    // Zero button — only visible and active when RIT is active
    if(inputMode==RIT)
      drawButtonIC7300(ritButtonX, ritButtonY+buttonSpaceY, "Zero", BTN_WARN);
    else
      for(int r=ritButtonY+buttonSpaceY; r<ritButtonY+buttonSpaceY+buttonHeight; r++)
        drawLine(ritButtonX, r, ritButtonX+buttonWidth-1, r, 0,0,0);
    }
  else
    {
    drawButtonIC7300(ritButtonX, ritButtonY, "RIT", BTN_OFF);
    // erase Zero button
    for(int r=ritButtonY+buttonSpaceY; r<ritButtonY+buttonSpaceY+buttonHeight; r++)
      drawLine(ritButtonX, r, ritButtonX+buttonWidth-1, r, 0,0,0);
    }
  // value readout (original position, original style)
  textSize=2;
  setForeColour(255,220,0);
  to=0;
  if(abs(rit)>0)   to=8;
  if(abs(rit)>9)   to=16;
  if(abs(rit)>99)  to=24;
  if(abs(rit)>999) to=32;
  gotoXY(ritButtonX,ritButtonY-25);
  displayStr("         ");
  gotoXY(ritButtonX+38-to,ritButtonY-25);
  if(rit==0)
    sprintf(ritStr,"0");
  else
    sprintf(ritStr,"%+d",rit);
  displayStr(ritStr);
}

void initGUI()
{
  clearScreen();


// Volume Button
  drawButtonIC7300(volButtonX, volButtonY, "Vol", BTN_OFF);
  // AGC button above SQL
  {
  const char* agcLabels[] = {"AGC OFF","AGC F","AGC M","AGC S","AGC L"};
  int agcState = (agcMode == 0) ? BTN_WARN : BTN_ON;
  drawButtonIC7300(agcButtonX, agcButtonY, agcLabels[agcMode], agcState);
  }
 

 //bottom row of buttons
  displayMenu();
  setBand(band);
  
//Squelch button now visible in all modes. 
    sqlButton(1);
    clearWaterfall();
    setAGC(agcMode);
    { char adj[8]; sprintf(adj,"Y%d",AGCAdj); sendFifo(adj); }  // restore AGC level
    drawCallsignDisplay();
}

void setRotation(int rot)
{
  rotateTouch(rot);
  rotateScreen(rot);
}

void clearWaterfall(void)
{
  for(int r=0;r<rows;r++)
   {  
   for(int p=0;p<points;p++)
    {  
    buf[p][r]=-100;
    }
   }
   flushUDP();
}


void initSDR(void)
{
  setBand(band);
  setMode(mode); 
  setVolume(volume);
  setSquelch(squelch);
  setRit(0);
  setSSBMic(SSBMic);
  setFMMic(FMMic);
  setAMMic(AMMic);
  setFreqInc();
  lastLOhz=0;
  setFreq(freq);
  setTx(0);
}

void displayMenu()
{
  // AGC button above SQL — redrawn here to stay consistent with menu state
  {
  const char* agcLabels[] = {"AGC OFF","AGC F","AGC M","AGC S","AGC L"};
  int agcState = (agcMode == 0) ? BTN_WARN : BTN_ON;
  drawButtonIC7300(agcButtonX, agcButtonY, agcLabels[agcMode], agcState);
  }

  // BAND — cyan when band popup is open
  drawButtonIC7300(funcButtonsX,              funcButtonsY, "BAND",
                   (popupSel==BAND) ? BTN_ON : BTN_OFF);
  // MODE — cyan when mode popup is open
  drawButtonIC7300(funcButtonsX+buttonSpaceX, funcButtonsY, "MODE",
                   (popupSel==MODE) ? BTN_ON : BTN_OFF);

  // Button 3 — DUP / 1750 / ATK
  if((mode==FM) && (bandRepShift[band]!=0))
    {
    if((ptt|ptts) && (bandDuplex[band]>0))
      drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "1750", BTN_ON);
    else
      drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "DUP",  BTN_ON);
    }
  else
    drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, " ", BTN_OFF);

  // SET
  drawButtonIC7300(funcButtonsX+buttonSpaceX*3, funcButtonsY, "SET",    BTN_WARN);

  // Button 5 — SNAP (AGC moved above SQL button)
  drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);

  // BEACON / CWID / DOTS
  if(sendBeacon > 0)
    {
    if(sendBeacon==1)
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "CWID",   BTN_AMBER);
    else
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "DOTS",   BTN_AMBER);
    }
  else
    drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON",
                     (popupSel==BEACON) ? BTN_ON : BTN_OFF);

  // PTT
  drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "PTT",
                   (ptt|ptts) ? BTN_DANGER : BTN_OFF);
}


void displayPopupMode(void)
{
  clearPopUp();
  // MODE button ON (popup is open)
  drawButtonIC7300(funcButtonsX+buttonSpaceX, funcButtonsY, "MODE", BTN_ON);
  for(int n=0;n<nummode;n++)
    {
    // Highlight currently active mode
    int mstate = (n == mode) ? BTN_ON : BTN_OFF;
    drawButtonIC7300(popupX+n*buttonSpaceX, popupY, modename[n], mstate);
    }
  popupSel=MODE;
}

void displayPopupBand(void)
{
  char bstr[6];
  int b;
  clearPopUp();
  // More.. button — ON when on second page (popupFirstBand > 0)
  drawButtonIC7300(popupX, popupY, "More..", popupFirstBand > 0 ? BTN_ON : BTN_OFF);
  // BAND button ON (popup is open)
  drawButtonIC7300(funcButtonsX, funcButtonsY, "BAND", BTN_ON);
  for(int n=0;n<6;n++)
    {
    b=bandFreq[n+popupFirstBand];
    sprintf(bstr,"%d", b);
    // Highlight currently selected band
    int bstate = (n+popupFirstBand == band) ? BTN_ON : BTN_OFF;
    drawButtonIC7300(popupX+(n+1)*buttonSpaceX, popupY, bstr, bstate);
    }
  popupSel=BAND;
}

void displayPopupBeacon(void)
{
  clearPopUp();
  // BEACON button ON (popup is open)
  drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON", BTN_ON);
  // Highlight active beacon mode
  drawButtonIC7300(popupX+buttonSpaceX*5, popupY, "DOTS",
                   sendBeacon==2 ? BTN_AMBER : BTN_OFF);
  drawButtonIC7300(popupX+buttonSpaceX*6, popupY, "CWID",
                   sendBeacon==1 ? BTN_AMBER : BTN_OFF);
  popupSel=BEACON;
}


void clearPopUp(void)
{
for(int py=popupY;py<popupY+buttonHeight+1;py++)
{
  for(int px=0;px<800;px++)
  {
  setPixel(px,py,0,0,0);
  }
}
popupSel=NONE;
displayMenu();
}

void processHmi(int hm)
{
  if(hm==1)         //volume down button released
    {
      setPtts(0);
    }
   if(hm == 2)     //volume down button pressed
    {
       setPtts(1);
    }
}
                                                          
void processMouse(int mbut)
{
  if(mbut==128)       //scroll whell turned 
    {
      if((inputMode==FREQ) && (dialLock==0))
      {
        freq=freq+(mouseScroll*freqInc);
        mouseScroll=0;
        if(((freq + bandRxOffset[band])/bandRxHarmonic[band]) < minHwFreq) freq=(minHwFreq - bandRxOffset[band])/bandRxHarmonic[band];
        if(((freq + bandRxOffset[band])/bandRxHarmonic[band]) > maxHwFreq) freq=(maxHwFreq - bandRxOffset[band])/bandRxHarmonic[band];
        setFreq(freq);
        return;      
      }
      
      if(mouseScroll>0) mouseScroll=1;                     //prevent large changes when adjusting. 
      if(mouseScroll<0) mouseScroll=-1;
      
      if(inputMode==SETTINGS)
      {
        changeSetting();
        return;
      }
      if(inputMode==VOLUME)
      {
        volume=volume+mouseScroll;
        mouseScroll=0;
        if(volume < 0) volume=0;
        if(volume > maxvol) volume=maxvol;
        setVolume(volume);
        return;      
      }    
      if(inputMode==SQUELCH)
      {
        squelch=squelch+mouseScroll;
        mouseScroll=0;
        if(squelch < 0) squelch=0;
        if(squelch > maxsql) squelch=maxsql;
        bandSquelch[band][mode]=squelch;
        setSquelch(squelch);
        return;      
      }   
      if(inputMode==RIT)
      {
        rit=rit+mouseScroll*10;
        mouseScroll=0;
        if(rit < minrit) rit=minrit;
        if(rit > maxrit) rit=maxrit;
        setRit(rit);
        return;      
      }     
    }
    
  if(mbut==1+128)      //Left Mouse Button down
    {
      if((inputMode==SETTINGS)&&((settingNo==CWID)||(settingNo==CALLSIGN)||(settingNo==BAND_BITS_RX)||(settingNo==BAND_BITS_TX)))
       {
         setIndex=setIndex-1;
         if(setIndex<0) setIndex=0;
         displaySetting(settingNo);
       }
      else
       {
        tuneDigit=tuneDigit-1;
        if(tuneDigit<0) tuneDigit=0;
        if(tuneDigit==5) tuneDigit=4;
        if(tuneDigit==9) tuneDigit=8;
        setFreqInc();
        setFreq(freq);
        twoButTimer=20;
        lastBut=lastBut | 1;
        if((inputMode==SETTINGS) && (settingNo==BAND_BITS_RX))
          {
          displaySetting(BAND_BITS_RX);
          }
        if((inputMode==SETTINGS) && (settingNo==BAND_BITS_TX))
          {
          displaySetting(BAND_BITS_TX);
          }
       }  
    }
    
  if(mbut==2+128)      //Right Mouse Button down
    {
      if((inputMode==SETTINGS)&&((settingNo==CWID)||(settingNo==CALLSIGN)||(settingNo==BAND_BITS_RX)||(settingNo==BAND_BITS_TX)))
       {
         setIndex=setIndex+1;
         if(setIndex>maxSetIndex) setIndex=maxSetIndex;
         displaySetting(settingNo);
       }
      else  
       {
         tuneDigit=tuneDigit+1;
         if(tuneDigit > maxTuneDigit) tuneDigit=maxTuneDigit;
         if(tuneDigit==5) tuneDigit=6;
         if(tuneDigit==9) tuneDigit=10;
         setFreqInc();
         setFreq(freq); 
         twoButTimer=20;
         lastBut=lastBut | 2;
         if((inputMode==SETTINGS) && (settingNo==BAND_BITS_RX))
           {
             displaySetting(BAND_BITS_RX);
           }
         if((inputMode==SETTINGS) && (settingNo==BAND_BITS_TX))
           {
             displaySetting(BAND_BITS_TX);
           }
        }          
    }
 
  if((mbut==3+128) | (lastBut==3))       //Middle button down or both buttons within 100ms
     {
      if(dialLock==0)
        {
          setDialLock(1);
        }
      else
        {
         setDialLock(0); 
        }
       lastBut=0;
     }
    
  if(mbut==4+128)      //Extra Button down
    {
 
    }   
    
  if(mbut==5+128)      //Side Button down
    {
 
    }
  
    
}

void setFreqInc()
{
  if(tuneDigit==0) freqInc=10000.0;
  if(tuneDigit==1) freqInc=1000.0;
  if(tuneDigit==2) freqInc=100.0;
  if(tuneDigit==3) freqInc=10.0;
  if(tuneDigit==4) freqInc=1.0;
  if(tuneDigit==5) tuneDigit=6;
  if(tuneDigit==6) freqInc=0.1;
  if(tuneDigit==7) freqInc=0.01;
  if(tuneDigit==8) freqInc=0.001;                                    
  if(tuneDigit==9) tuneDigit=10;
  if(tuneDigit==10) freqInc=0.0001;
  if(tuneDigit==11) freqInc=0.00001;   
}
       


void processTouch()
{ 


// Volume Button   


if(buttonTouched(volButtonX,volButtonY))    //Vol
    {
      if(inputMode==VOLUME)
        {
        setInputMode(FREQ);
        }
      else
        {
        setInputMode(VOLUME);
        }
      return;
    }
                                                        


// AGC Button (above SQL)
if(buttonTouched(agcButtonX,agcButtonY))    //AGC mode cycle
    {
      if(inputMode==FREQ)
        {
        agcMode=(agcMode+1)%5;
        setAGC(agcMode);
        }
      return;
    }

// Squelch Button   


if(buttonTouched(sqlButtonX,sqlButtonY))    //sql
    {
      if(inputMode==SQUELCH)
        {
        setInputMode(FREQ);
        }
      else
        {
        setInputMode(SQUELCH);
        }
      return;
    }


//RIT Button

if(buttonTouched(ritButtonX,ritButtonY))    //rit
    {
     if(mode!=4)
     {
      if(inputMode==RIT)
        {
        setInputMode(FREQ);
        }
      else
        {
        setInputMode(RIT);
        }
      return;
      }
    }

//RIT Zero Button

if(buttonTouched(ritButtonX,ritButtonY+buttonSpaceY))    //rit zero
    {
     setRit(0);
     setInputMode(FREQ);
    }
    
if(buttonTouched(sMeterX,sMeterY))                        //touch on s-Meter
{
   if(sMeterType == 0)
   {
     sMeterType=1;
   }
   else
   {
     sMeterType=0;
   }
}

//Function Buttons


if(buttonTouched(funcButtonsX,funcButtonsY))    //Button 1 = BAND or MENU
    {
    if((inputMode==FREQ) && (popupSel!=BAND))
    {
      writeConfig();
      displayPopupBand();
      return;
    }
    else
    {
      setInputMode(FREQ);
      clearPopUp();
      return; 
    }
      
        
    }      
if(buttonTouched(funcButtonsX+buttonSpaceX,funcButtonsY))    //Button 2 = MODE (FREQ) / SNAP (SETTINGS)
    {
     if((inputMode==FREQ) && (popupSel!=MODE))
      {displayPopupMode(); return;}
     else if(inputMode==SETTINGS)
      {takeSnapshot(); return;}
     else
      {setInputMode(FREQ); clearPopUp(); return;}
    }
      
if(buttonTouched(funcButtonsX+buttonSpaceX*2,funcButtonsY))  // Button 3 =Blank or DUP or 1750 or NEXT
    {
     if(inputMode==FREQ)
      {
      if((mode==FM) && (bandRepShift[band]!=0))
      {
        if((ptt | ptts) && (bandDuplex[band]>0))
        {  
        send1750(); 
        }
        else
        {
        if(bandDuplex[band]==0) 
          {
          bandDuplex[band]=1;
          setMode(mode);
          }
         else 
          {
          bandDuplex[band]=0;
          setMode(mode);
          }

        }
       
      }      
      return;
      }
      else if(inputMode==SETTINGS)
      {
      settingNo=settingNo+1;
      if(settingNo==numSettings) settingNo=0;
      displaySetting(settingNo);
      return;
      }
      else
      {
      setInputMode(FREQ);
      }
    }
      
if(buttonTouched(funcButtonsX+buttonSpaceX*3,funcButtonsY))    // Button4 =SET or PREV
    {
     if(inputMode==FREQ)
      {
      setInputMode(SETTINGS);
      return;
      }
    else  if (inputMode==SETTINGS)
      {
      settingNo=settingNo-1;
      if(settingNo<0) settingNo=numSettings-1;
      displaySetting(settingNo);
      return;
      }
    else
      {
      setInputMode(FREQ);
      return;
      }
    }
       
if(buttonTouched(funcButtonsX+buttonSpaceX*4,funcButtonsY))    //Button 5 = SNAP (FREQ) / UPGRADE (SETTINGS)
    {
    if(inputMode==FREQ)
      {
      takeSnapshot();
      return;
      }
    else if(inputMode==SETTINGS)
      {
      if(upgradeConfirm==0)
        {upgradeConfirm=1;
         drawButtonIC7300(funcButtonsX+buttonSpaceX*4,funcButtonsY,"SURE?",BTN_WARN);
         displayError("  Touch UPGRDE again to upgrade from GitHub  ");return;}
      upgradeConfirm=0;doGitUpgrade();return;
      }
    else
      {setInputMode(FREQ);}
    }

if(buttonTouched(funcButtonsX+buttonSpaceX*5,funcButtonsY))    //Button 6 = BEACON  or Exit to Portsdown
    {
    if(inputMode==FREQ)
      {
      if(sendBeacon==0)
        {
         displayPopupBeacon();
        }
      else
        {
        setBeacon(0);
        }
 
      return;
      }
      else if (inputMode==SETTINGS)
      {
         setBandBits(0);
         sendFifo("H0");        //unlock the flowgraph so that it can exit
         sendFifo("Q");       //kill the SDR
         clearScreen();
         writeConfig();
         iio_context_destroy(plutoctx);
         sleep(2);                  
         system("sudo reboot"); 
      }
      else
      {
      setInputMode(FREQ);
      }
    } 
         
if(buttonTouched(funcButtonsX+buttonSpaceX*6,funcButtonsY))   //Button 7 = PTT  or OFF
    {
    if(inputMode==FREQ)
      {
      if(ptts==0)
        {
          setPtts(1);
        }
      else
        {
          setPtts(0);
        }
      return;
      }
      else if (inputMode==SETTINGS)
      {
      setBandBits(0);
      sendFifo("H");        //unlock the flowgraph so that it can exit
      sendFifo("Q");       //kill the SDR 
      writeConfig();
      iio_context_destroy(plutoctx);
      if(screenrotate)
      {
        system("sudo cp /home/pi/Langstone/splash_inv.bgra /dev/fb0");
      }
      else
      {
        system("sudo cp /home/pi/Langstone/splash.bgra /dev/fb0");
      }
      sleep(2);
      system("sudo poweroff");                          
      return;      
      }
      else
      {
      setInputMode(FREQ);
      }
      

    }      


   
//Touch on Frequency Digits moves cursor to digit and sets tuning step. Removes dial lock if it is set.  

if((touchY>freqDisplayY) & (touchY < freqDisplayY+freqDisplayCharHeight) & (touchX>freqDisplayX) & (touchX < freqDisplayX+12*freqDisplayCharWidth))   
  {
    if(inputMode==FREQ) setDialLock(0);
    int tx=touchX-freqDisplayX;
    tx=tx/freqDisplayCharWidth;
    tuneDigit=tx;
    setFreqInc();
    setFreq(freq);
    return;
  }


//touch on spectrum display increments the FFT Bandwidth
if((touchY < FFTY) & (touchY > FFTY - spectrum_rows) & (touchX > FFTX) & (touchX < FFTX + points))   
  {
    bandFFTBW[band]++;
    if(bandFFTBW[band] > 3)  bandFFTBW[band] = 0;
    setFFTBW(bandFFTBW[band]);
    return;
  }



if(popupSel==MODE)
{
  for(int n=0;n<nummode;n++)
  {
     if(buttonTouched(popupX+(n*buttonSpaceX),popupY))                                
       {
       mode=n;
       setMode(mode);
       clearPopUp();
       }
  }
}

if(popupSel==BAND)
{
  if(buttonTouched(popupX,popupY))
  {
  popupFirstBand=popupFirstBand+6;
  if(bands24)
  {
    if(popupFirstBand>23) popupFirstBand=0;
  }
  else
  {
    if(popupFirstBand>11) popupFirstBand=0;
  }
  displayPopupBand();
  }
  
  for(int n=1;n<7;n++)
  {
     if(buttonTouched(popupX+(n*buttonSpaceX),popupY))                                
       {
       band=n-1+popupFirstBand;
       setBand(band);
       clearPopUp();
       }
  }

 
}


if(popupSel==BEACON)
  {
    if(buttonTouched(popupX+ 5* buttonSpaceX,popupY))       //DOTS
      {
      setBeacon(2);
      clearPopUp();
      }
     if(buttonTouched(popupX+ 6* buttonSpaceX,popupY))       //CWID
      {
      setBeacon(1);
      clearPopUp();
      }     
  }







}


int buttonTouched(int bx,int by)
{
  return ((touchX > bx) & (touchX < bx+buttonWidth) & (touchY > by) & (touchY < by+buttonHeight));
}

void setBand(int b)
{
  // Save current band-specific settings before switching
  if(band >= 0 && band < numband) {
    // AGCAdj, WFFloor, specStretch already use #define → auto-saved
  }
  freq=bandFreq[band];
  setFreq(freq);
  mode=bandMode[band];
  setMode(mode);
  setFFTBW(bandFFTBW[band]);
  setBandBits(bandBitsRx[band]);
  squelch=bandSquelch[band][mode];
  setSquelch(squelch);
  setCTCSS(bandCTCSS[band]);
  FFTRef=bandFFTRef[band];
  TxAtt=bandTxAtt[band];
  setPlutoTxAtt(TxAtt);
  setPlutoRxGain(bandRxGain[band]);
  setLimessbGeqH(bandssbGeqH[band]);
  setLimessbGeqM2(bandssbGeqM2[band]);
  setLimessbGeqM1(bandssbGeqM1[band]);
  setLimessbGeqL(bandssbGeqL[band]);
  configCounter=configDelay;
}

void setPtts(int p)
{
 if(p==1)
 {
  ptts=1;
  drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "PTT", BTN_DANGER);
  setTx(ptt|ptts);
 }
  else
 {
  ptts=0;
  if(sendBeacon>0)
   {
    setBeacon(0);
    setMode(mode);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON", BTN_OFF);
    }
  setTx(ptt|ptts);
  drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "PTT",    BTN_OFF);
  drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON", BTN_OFF);
 }
}




void setBeacon(int b)
{
 if(b > 0)
   {
      sendBeacon=b;
      morseReset();
      keyDownTimer=300;
      lastmode=mode;
      mode=CW;
      setMode(mode);
      if(lastmode==USB) freq=freq+0.0008;         //If we are working SSb add an offset of 800Hz to bring pips into Rx bandwidth.
      if(lastmode==LSB) freq=freq-0.0008;
      setFreq(freq);                  
      if(!(ptt|ptts))                     //if not already transmitting
        {
          setTx(1);                          //goto transmit
        }
      ptts=1;                             //latch the transmit on
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON", BTN_AMBER);
      drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "PTT",    BTN_DANGER);
   }
  else
    {
      sendBeacon=0;                                                                                           
      ptts=0;
      setTx(0);
      setKey(0);
      usleep(100000);
      mode=lastmode;
      setMode(mode);
      if(mode==USB) freq=freq-0.0008;
      if(mode==LSB) freq=freq+0.0008;
      setFreq(freq);
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "BEACON", BTN_OFF);
      drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "PTT",    BTN_OFF);
    }

}

 
void setVolume(int vol)
{
  char volStr[10];
  sprintf(volStr,"V%d",vol);
  sendFifo(volStr);
  setForeColour(255,220,0);
  textSize=2;
  gotoXY(volButtonX+30,volButtonY-25);
  displayStr("   ");
  gotoXY(volButtonX+30,volButtonY-25);
  sprintf(volStr,"%d",vol);
  displayStr(volStr);
  
  configCounter=configDelay;
}

void setSquelch(int sql)
{
  char sqlStr[10];
  sprintf(sqlStr,"S%d",sql);
  setForeColour(255,220,0);
  textSize=2;
  gotoXY(sqlButtonX+30,sqlButtonY-25);
  displayStr("   ");
  gotoXY(sqlButtonX+30,sqlButtonY-25);
  sprintf(sqlStr,"%d",sql);
  displayStr(sqlStr);
  configCounter=configDelay;
}

void setInputMode(int m)

{
if(inputMode==SETTINGS)
  {
  gotoXY(0,settingY);
  setForeColour(255,255,255);
  textSize=2;
  displayStr("                                                ");
  gotoXY(0,settingY+8);
  displayStr("                                                ");
  // Clear snap message strip above settings line
  for(int cy = settingY-29; cy < settingY; cy++)
    drawLine(0, cy, 799, cy, 0,0,0);
  writeConfig();
  displayMenu();
  }
if(inputMode==VOLUME)
  {
    drawButtonIC7300(volButtonX, volButtonY, "Vol", BTN_OFF);
  }
if(inputMode==SQUELCH)
  {
    drawButtonIC7300(sqlButtonX, sqlButtonY, "SQL", BTN_OFF);
  }
if(inputMode==RIT)
  {
    drawButtonIC7300(ritButtonX, ritButtonY, "RIT", BTN_OFF);
    // Erase Zero button completely (black) when leaving RIT
    for(int r=ritButtonY+buttonSpaceY; r<ritButtonY+buttonSpaceY+buttonHeight; r++)
      drawLine(ritButtonX, r, ritButtonX+buttonWidth-1, r, 0,0,0);
  }
  
inputMode=m;

if(inputMode==FREQ)
  {
  setFreq(freq);
  }

if(inputMode==SETTINGS)
  {
    clearPopUp();
    showSettingsMenu();
    mouseScroll=0;
    displaySetting(settingNo);
    upgradeConfirm = 0;       // reset upgrade confirm state when entering settings
  }
if(inputMode==VOLUME)
  {
    drawButtonIC7300(volButtonX, volButtonY, "Vol", BTN_ON);
  }
if(inputMode==SQUELCH)
  {
    drawButtonIC7300(sqlButtonX, sqlButtonY, "SQL", BTN_ON);
  }
if(inputMode==RIT)
  {
    drawButtonIC7300(ritButtonX, ritButtonY,              "RIT",  BTN_ON);
    drawButtonIC7300(ritButtonX, ritButtonY+buttonSpaceY, "Zero", BTN_WARN);
  }

}

void setRit(int ri)
{
  char ritStr[10];
  int to;
  if(!((mode==FM) || (mode==AM)))
  {
  rit=ri;
  setForeColour(255,220,0);
  textSize=2;
  to=28; 
  gotoXY(ritButtonX,ritButtonY-25);
  displayStr("         ");
  gotoXY(ritButtonX+38-to,ritButtonY-25);
  if (rit==0)
  {
  sprintf(ritStr," 0.00");
  }
  else
  {
    sprintf(ritStr,"%+3.2f",rit/1000.0);
  }
  displayStr(ritStr);
  setFreq(freq);
  }
}


void setSSBMic(int mic)
{
  char micStr[10];
  sprintf(micStr,"G%d",mic);
  sendFifo(micStr);
}

void setLimessbGeqH(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"b%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqM2(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"p%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqM1(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"P%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqL(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"e%d",gain);
  sendFifo(gainStr);
}

void setFMMic(int mic)
{
  char micStr[10];
  sprintf(micStr,"g%d",mic);
  sendFifo(micStr);
}

void setAMMic(int mic)
{
  char micStr[10];
  sprintf(micStr,"d%d",mic);
  sendFifo(micStr);
}

void setCTCSS(int t)
{
  char ctStr[10];
  sprintf(ctStr,"C%d",CTCSSTone[t]);
  sendFifo(ctStr);
}

void setKey(int k)
{
  char kStr[5];
  sprintf(kStr,"K%d",k);
  sendFifo(kStr);
}

void setMute(int m)
{
  if(squelchGate == 0) m=1;
  char mStr[5];
  sprintf(mStr,"U%d",m);
  sendFifo(mStr);
}

void setRxFilter(int low,int high)
{
  char filtStr[10];
  sprintf(filtStr,"I%d",low);
  sendFifo(filtStr);                                             
  sprintf(filtStr,"F%d",high);
  sendFifo(filtStr);
  
  rxFilterLow = low;
  rxFilterHigh = high;  
}

void setTxFilter(int low,int high)
{
  char filtStr[10];
  sprintf(filtStr,"i%d",low);
  sendFifo(filtStr);
  sprintf(filtStr,"f%d",high);
  sendFifo(filtStr);  
}


void setFFTBW(int bw)
{
  char BWStr[10];
  sprintf(BWStr,"W%d",bw);
  sendFifo(BWStr);
  
  switch(bw)
  {
  case 0:             //48KHz   Sample rate
  HzPerBin = 94;
  break;
  case 1:             //24KHz   Sample rate
  HzPerBin = 47;
  break; 
  case 2:             //12KHz   Sample rate
  HzPerBin = 23;
  break;
  case 3:             //6KHz   Sample rate
  HzPerBin = 12;
  break;
  }
}


void setMode(int md)
{
  // ── Save AGCAdj for current mode, restore for new mode ───────────
  if(mode >= 0 && mode < 6) AGCAdjByMode[mode] = AGCAdj;
  AGCAdj = AGCAdjByMode[md];
  { char adj[8]; sprintf(adj,"Y%d",AGCAdj); sendFifo(adj); }

  bandMode[band]=md;
  gotoXY(modeX,modeY);
  setForeColour(255,255,0);
  textSize=2;
  displayStr(modename[md]);
  if((md==FM)&&(bandDuplex[band]==1))
    {
    displayStr("DUP");
    }
  else
    {
    displayStr("   ");
    }      
  if(md==USB)
    {
    sendFifo("M0");    //USB
    setTxFilter(100,3000);    //USB Filter Setting
    setRxFilter(bandSSBFiltLow[band],bandSSBFiltHigh[band]);    //USB Filter Setting    configured in settings.
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(1);
    setRit(0);
    } 
  
  if(md==LSB)
    {
    sendFifo("M1");    //LSB
    setTxFilter(-3000,-100); // LSB Filter Setting
    setRxFilter(-1*bandSSBFiltHigh[band],-1*bandSSBFiltLow[band]); // LSB Filter Setting
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(1);
    setRit(0);
    } 
  
  if(md==CW)
    {
    sendFifo("M2");    //CW
    setRxFilter(bandSSBFiltLow[band],bandSSBFiltHigh[band]);   // USB filter settings used for CW Wide Filter
    setTxFilter(-100,100); // CW Filter Setting
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(1);
    setRit(0);
    } 
    
  if(md==CWN)
    {
    sendFifo("M3");    //CWN
    setRxFilter(600,1000);    //CW Narrow Filter
    setTxFilter(-100,100); // CW Filter Setting
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(1);
    setRit(0);
    } 
  if(md==FM)
    {
    sendFifo("M4");    //FM
    setRxFilter(-7500,7500);    //FM Filter
    setTxFilter(-7500,7500);    //FM Filter  
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(0);
    setRit(0);
    } 
  if(md==AM)
    {
    sendFifo("M5");    //AM
    setRxFilter(-5000,5000);    //AM Filter
    setTxFilter(-5000,5000);    //AM Filter 
    setFreq(freq);    //set the frequency to adjust for CW offset.
    ritButton(0);
    setRit(0);
    } 

    squelch=bandSquelch[band][mode];
    setSquelch(squelch);
                                                                 
configCounter=configDelay;
}

void setTxPin(int v)
{
  if(v==1) lgGpioWrite(gpiohandle,txPin,1); else lgGpioWrite(gpiohandle,txPin,0);  
}

void setTx(int pt)
{
  if((pt==1)&&(transmitting==0))
    {
    if(firstpass == 0)                                      //don't set the Output pins if we are still initialising
        { 
         setTxPin(1);
         setBandBits(bandBitsTx[band]);
         plutoGpo=plutoGpo | 0x10;
         setPlutoGpo(plutoGpo);                               //set the Pluto GPO Pin 
        } 
      usleep(TXDELAY);
      setHwTxFreq(freq);
      if((mode==FM)&&(bandDuplex[band]==1))
        {
        displayFreq(freq+bandRepShift[band]);
        displayMenu();
        }
      PlutoTxEnable(1);
      if (moni==0) setMute(1);                        //mute the receiver
      if(satMode()==0)
      {
        sMeter=0;
        setHwRxFreq(freq+10.0);               //offset the Rx frequency to prevent unwanted mixing. (happens even if disabled!) 
        PlutoRxEnable(0);
      }
      if(satMode()==0)
      {
        clearWaterfall();
      }
      sendFifo("T");
      gotoXY(txX,txY);
      setForeColour(255,0,0);
      textSize=2;
      displayStr("Tx");
      transmitting=1;  
    }
  else if((pt==0)&&(transmitting==1))
    {
      if(satMode()==0)
      {
      sMeter=0;
      clearWaterfall();
      }
      
      sendFifo("R");
      setMute(0);                  //unmute the receiver 
      setHwTxFreq(freq+10.0);           //offset the Tx freq to prevent unwanted spurious
      PlutoTxEnable(0);
      PlutoRxEnable(1);
      setHwRxFreq(freq);
      if((mode==FM)&&(bandDuplex[band]==1))
        {
        displayFreq(freq);
        displayMenu();
        }
      gotoXY(txX,txY);
      setForeColour(0,255,0);
      textSize=2;
      displayStr("Rx");                                                                           
      transmitting=0;
      usleep(RXDELAY);
      setTxPin(0);
      setBandBits(bandBitsRx[band]);
      plutoGpo=plutoGpo & 0xEF;
      setPlutoGpo(plutoGpo);                               //clear the Pluto GPO Pin 
    }
}

void setHwRxFreq(double fr)
{
  long long rxoffsethz;
  long long LOrxfreqhz;
  long long rxfreqhz;
  double frRx;
  double frTx;
  
  frRx=fr+bandRxOffset[band];
  
  
  if(bandRxHarmonic[band] < 2)
  {
    if(frRx > maxHwFreq)
    {
    frRx = maxHwFreq;
    }
    if(frRx < minHwFreq)
    {
    frRx = minHwFreq;
    }
  }
  
  rxfreqhz=frRx*1000000;
  
  if (rxfreqhz<69900000) rxfreqhz=69900000;         //this is the lowest frequency we can receive with a pluto 
  
  if(rxfreqhz<70100000)
  {
  rxoffsethz=(rxfreqhz-70000000);        //Special case for receiving below 70.100     Use the offset of +-100KHz
  LOrxfreqhz=70000000;
  }
  else
  {
  rxoffsethz=(rxfreqhz % 100000)+50000;        //use just the +50Khz to +150Khz positive side of the sampled spectrum. This avoids seeing the DC hump .
  LOrxfreqhz=rxfreqhz-rxoffsethz;
  }

  if( bandRxHarmonic[band]>1)                                //allow for harmonic mixing for higher bands (10GHz)
    {
      LOrxfreqhz=LOrxfreqhz/bandRxHarmonic[band];
    }
  

  rxoffsethz=rxoffsethz+rit;
  if((mode==CW)|(mode==CWN))
    {
     rxoffsethz=rxoffsethz-800;         //offset  for CW tone of 800 Hz    
    }
  if(LOrxfreqhz!=lastLOhz)         
    {
      setPlutoRxFreq(LOrxfreqhz);          //Control Pluto directly to bypass problems with Gnu Radio Sink
      lastLOhz=LOrxfreqhz;
    }
  
  char offsetStr[32];
  sprintf(offsetStr,"O%d",rxoffsethz);   //send the rx offset tuning value 
  sendFifo(offsetStr);
}

void setHwTxFreq(double fr)
{                                                                        
  long long txfreqhz;
  double frTx;
  
  frTx=fr+bandTxOffset[band];
  
 
  if(bandTxHarmonic[band] < 2)
  {
    if(frTx > maxHwFreq)
    {
    frTx = maxHwFreq;
    }
    if(frTx < minHwFreq)
    {
    frTx = minHwFreq;
    }
  }
  
  if((mode==FM)&&(bandDuplex[band]==1))
    {
    frTx=frTx+bandRepShift[band];
    }
 
  txfreqhz=frTx*1000000;
   
  if(bandTxHarmonic[band]>1)                    //allow for Harmonic mixing for higher bands or for external multiplier such as Hydra
    {
    txfreqhz= txfreqhz/bandTxHarmonic[band];
    } 
  
  setPlutoTxFreq(txfreqhz);          //Control Pluto directly to bypass problems with Gnu Radio Sink
}

void displayFreq(double fr)
{
  long long freqhz;
  char digit[16]; 
  
  fr=fr+0.0000001;   // correction for rounding errors.
  freqhz=fr*1000000;    
  freqhz=freqhz+100000000000;     //force it to be 12 digits long
  sprintf(digit,"%lld",freqhz);
  
  gotoXY(freqDisplayX,freqDisplayY);
  setForeColour(255,255,255);
  textSize=5;
  if(digit[1]>'0') displayChar(digit[1]); else displayChar(' ');
  if((digit[1]>'0') | (digit[2]>'0')) displayChar(digit[2]); else displayChar(' ');
  if((digit[1]>'0') | (digit[2]>'0') | digit[3]>'0')  displayChar(digit[3]); else displayChar(' ');
  displayChar(digit[4]);
  displayChar(digit[5]);
  displayChar('.');
  displayChar(digit[6]);
  displayChar(digit[7]);
  displayChar(digit[8]);
  displayChar('.');
  displayChar(digit[9]);
  displayChar(digit[10]);
  
// Underline the currently selected tuning digit
  
  for(int dtd=0;dtd<12;dtd++)
    {
      gotoXY(freqDisplayX+dtd*freqDisplayCharWidth+4,freqDisplayY+freqDisplayCharHeight+5);
      int bb = 0;
      if (dtd==tuneDigit) bb=166;
      for(int p=0; p < freqDisplayCharWidth; p++)
        {
          setPixel(currentX+p,currentY+1,bb,bb,bb);
          setPixel(currentX+p,currentY+2,bb,bb,bb);
          setPixel(currentX+p,currentY+3,bb,bb,bb);
        }
    }

}

void setFreq(double fr)
{

  
  if(ptt | ptts) 
  {
    setHwTxFreq(fr);        //set Hardware Tx frequency if we are transmitting
  }
  else
  {
  setHwRxFreq(fr);       //set Hardware Rx frequency if we are receiving
  }

displayFreq(fr);
 
//set XVTR,SPLIT or MULT indication if needed.
  // XVTR section: x=418..498 (81px), text centred per actual width
  textSize=2;
  setForeColour(0,255,0);
  gotoXY(418, txvtrY); displayStr("      ");  // clear section
  if(multMode()==1)
    { gotoXY(435, txvtrY); displayStr("MULT"); }  // 4ch → x=435
  else if(txvtrMode()==1)
    { gotoXY(435, txvtrY); displayStr("XVTR"); }  // 4ch → x=435
  else if(splitMode()==1)
    { gotoXY(428, txvtrY); displayStr("SPLIT"); } // 5ch → x=428
  else
    { /* already cleared */ }
  // Redraw all dividers — displayStr erases background pixels that may hit them
   
   if(inputMode==FREQ)
   {
    // Redraw AGC button only when agcMode changed (avoid flicker on VFO turn)
    static int lastAgcMode = -1;
    if(agcMode != lastAgcMode || statusForceRedraw)
      {
      const char* agcLabels[] = {"AGC OFF","AGC F","AGC M","AGC S","AGC L"};
      int agcState = (agcMode == 0) ? BTN_WARN : BTN_ON;
      drawButtonIC7300(agcButtonX, agcButtonY,
                       agcLabels[agcMode], agcState);
      lastAgcMode = agcMode;
      }
    setMoni(0);
    }
 
 configCounter=configDelay;                       //write config after this amount of inactivity    
}

int satMode(void)
{
if(((abs(bandTxOffset[band]-bandRxOffset[band]) > 1 ) & (bandRxOffset[band]!=0) ) & bandRxHarmonic[band]<2  & bandTxHarmonic[band]<2 )     // if we have a differnt Rx and Tx offset and we are not multiplying then we must be in Sat mode. 
  {
  return 1;
  }
else
  {
  return 0;
  }
}

int txvtrMode(void)
{
if((abs(bandTxOffset[band]-bandRxOffset[band]) <1) & (abs(bandTxOffset[band]) >1 )  )       //if the tx and rx offset are the same and non zero then we are in Transverter mode
  {
  return 1;
  }
else
  {
  return 0;
  }
}

int splitMode(void)
{
if((abs(bandTxOffset[band])>0) & (bandRxOffset[band]==0))     //  if tx Offset is non zero and rxoffset is zero then we are in split mode. 
  {
  return 1;
  }
else
  {
  return 0;
  }
}

int multMode(void)
{
if((bandRxHarmonic[band]>1) || (bandTxHarmonic[band] >1))     //  if either of the Harmonic modes is set then we are in Mult mode. 
  {
  return 1;
  }
else
  {
  return 0;
  }
}

void setDialLock(int d)
{
  if(d==0)
  {
    dialLock=0;
    gotoXY(diallockX,diallockY);
    textSize=2;
    displayStr("    ");
  }
  else
  {
    dialLock=1;
    gotoXY(diallockX,diallockY);
    textSize=2;
    setForeColour(255,0,0);
    displayStr("LOCK");
  }
}

void setMoni(int m)
{
  if(m==1)
    {
     setMute(0);
     moni=1;
     gotoXY(moniX,moniY);
     textSize=2;
     setForeColour(0,255,0);
     displayStr("MONI");
    } 
  else
    {
     if (ptt | ptts) setMute(1);
     moni=0;
     gotoXY(moniX,moniY);
     textSize=2;
     displayStr("    ");
    }
}

void send1750(void)
{
  sendFifo("B1");
  drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "1750", BTN_DANGER);
  usleep(BurstLength);
  sendFifo("B0");
  drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "1750", BTN_ON);
}

void setBandBits(int b)
{
    if(b & 0x01) 
        {
        lgGpioWrite(gpiohandle,bandPin1,1);
        lgGpioWrite(gpiohandle,bandPin1alt,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin1,0);
        lgGpioWrite(gpiohandle,bandPin1alt,0);
        }
        
    if(b & 0x02) 
        {
        lgGpioWrite(gpiohandle,bandPin2,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin2,0);
        }   
    
    if(b & 0x04) 
        {
        lgGpioWrite(gpiohandle,bandPin3,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin3,0);
        }   
    
    if(b & 0x08) 
        {
        lgGpioWrite(gpiohandle,bandPin4,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin4,0);
        }   
    
    if(b & 0x10) 
        {
        lgGpioWrite(gpiohandle,bandPin5,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin5,0);
        }   
    
    if(b & 0x20) 
        {
        lgGpioWrite(gpiohandle,bandPin6,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin6,0);
        }   
        
    if(b & 0x40) 
        {
        lgGpioWrite(gpiohandle,bandPin7,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin7,0);
        }   
        
    if(b & 0x80) 
        {
        lgGpioWrite(gpiohandle,bandPin8,1);
        }
    else
        {
        lgGpioWrite(gpiohandle,bandPin8,0);
        }       

//  copy bits 0,1 and 2to Pluto GPO Pins if enabled

 if(bandBitsToPluto==1)
 {
  if(b & 0x01) 
      {
      plutoGpo=plutoGpo | 0x20;
      }
  else
      {
      plutoGpo=plutoGpo & 0xDF;
      }
      
  if(b & 0x02) 
      {
      plutoGpo=plutoGpo | 0x40;
      }
  else
      {
      plutoGpo=plutoGpo & 0xBF;
      }   
  
  if(b & 0x04) 
      {
      plutoGpo=plutoGpo | 0x80;
      }
  else
      {;
      plutoGpo=plutoGpo & 0x7F;
      }   
  setPlutoGpo(plutoGpo);
 } 
 else
 {
   plutoGpo=plutoGpo & 0x1F;
   setPlutoGpo(plutoGpo);
 }
 
  
}


void changeSetting(void)
{
  if(settingNo==SSB_MIC)        //SSB Mic Gain
      {
      SSBMic=SSBMic+mouseScroll;
      mouseScroll=0;
      if(SSBMic<0) SSBMic=0;
      if(SSBMic>maxSSBMic) SSBMic=maxSSBMic;
      setSSBMic(SSBMic);
      displaySetting(settingNo);
      }
   if(settingNo==FM_MIC)        // FM Mic Gain
      {
      FMMic=FMMic+mouseScroll;
      mouseScroll=0;
      if(FMMic<0) FMMic=0;
      if(FMMic>maxFMMic) FMMic=maxFMMic;
      setFMMic(FMMic);
      displaySetting(settingNo);
      }
   if(settingNo==AM_MIC)        // AM Mic Gain
      {
      AMMic=AMMic+mouseScroll;
      mouseScroll=0;
      if(AMMic<0) AMMic=0;
      if(AMMic>maxAMMic) AMMic=maxAMMic;
      setAMMic(AMMic);
      displaySetting(settingNo);
      }
    if(settingNo==REP_SHIFT)        //Repeater Shift
      {
        bandRepShift[band]=bandRepShift[band]+mouseScroll*freqInc;
        mouseScroll=0;
        setFreq(freq);
        displaySetting(settingNo);
      }  
    if(settingNo==CTCSS)        //CTCSS Tone
      {
        bandCTCSS[band]=bandCTCSS[band]+mouseScroll;
        if(bandCTCSS[band] < 0 ) bandCTCSS[band]=0;
        if(bandCTCSS[band] > (NUMCTCSS - 1) ) bandCTCSS[band]= NUMCTCSS - 1;        
        mouseScroll=0;
        setCTCSS(bandCTCSS[band]);
        displaySetting(settingNo);
      }  
   if(settingNo==RX_OFFSET)        //Transverter Rx Offset 
      {
        bandRxOffset[band]=bandRxOffset[band]-mouseScroll*freqInc;
        if(bandRxOffset[band] > 99999.9) bandRxOffset[band]= 99999.9;
        if(bandRxOffset[band] < -99999.9) bandRxOffset[band]= -99999.9;
        freq=freq+mouseScroll*freqInc;
        if((freq + bandRxOffset[band]) > maxHwFreq )
        {
          freq = freq - ((freq + bandRxOffset[band]) - maxHwFreq); 
        }
        
        if((freq + bandRxOffset[band]) < minHwFreq )
        {
          freq = freq + ( minHwFreq - (freq + bandRxOffset[band])); 
        }  
            
        mouseScroll=0;
        setFreq(freq);
        displaySetting(settingNo);
      } 
 if(settingNo==RX_HARMONIC)        // RX Harmonic mixing number
      {
      if(mouseScroll>0)
      {
        bandRxHarmonic[band]=5;
      }
      if(mouseScroll<0)
      {
         bandRxHarmonic[band]=1;
      }
      mouseScroll=0;
      setFreq(freq);
      displaySetting(settingNo);                                                                 
      }    
   if(settingNo==TX_OFFSET)        //Transverter Tx Offset
      {
        bandTxOffset[band]=bandTxOffset[band]-mouseScroll*freqInc;
        if(bandTxOffset[band] > 99999.9) bandTxOffset[band]= 99999.9;
        if(bandTxOffset[band] < -99999.9) bandTxOffset[band]= -99999.9;
 
        freq=freq+mouseScroll*freqInc;
  
        if((freq + bandTxOffset[band]) > maxHwFreq )
        {
          freq = freq - ((freq + bandTxOffset[band]) - maxHwFreq); 
        }
        
        if((freq + bandTxOffset[band]) < minHwFreq )
        {
          freq = freq + ( minHwFreq - (freq + bandTxOffset[band])); 
        }
        
        mouseScroll=0;
        setFreq(freq);
        displaySetting(settingNo);
      }  
 if(settingNo==TX_HARMONIC)        // TX Harmonic mixing number or external multiplier
      {
      bandTxHarmonic[band]=bandTxHarmonic[band]+mouseScroll;
      if(bandTxHarmonic[band]<1) bandTxHarmonic[band]=1;
      if(bandTxHarmonic[band]>5) bandTxHarmonic[band]=5;
      if(mouseScroll>0)
      {
        if(bandTxHarmonic[band]==4) bandTxHarmonic[band]=5;
        if(bandTxHarmonic[band]==3) bandTxHarmonic[band]=5;
      }
      else
      {
        if(bandTxHarmonic[band]==4) bandTxHarmonic[band]=2;
        if(bandTxHarmonic[band]==3) bandTxHarmonic[band]=2;
      }
      mouseScroll=0;
      setFreq(freq);
      displaySetting(settingNo);  
      }      
   if(settingNo==BAND_BITS_RX)        // Band Bits Rx
      {
      if(setIndex==7)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x01; 
        }
      if(setIndex==6)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x02; 
        }
      if(setIndex==5)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x04; 
        }
      if(setIndex==4)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x08; 
        }
       if(setIndex==3)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x10; 
        }
      if(setIndex==2)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x20; 
        }
      if(setIndex==1)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x40; 
        }
      if(setIndex==0)
        {
        bandBitsRx[band]=bandBitsRx[band] ^ 0x80; 
        }
      mouseScroll=0;
      if(bandBitsRx[band]<0) bandBitsRx[band]=0;
      if(bandBitsRx[band]>255) bandBitsRx[band]=255;
      setBandBits(bandBitsRx[band]);
      displaySetting(settingNo);  
      }  
if(settingNo==BAND_BITS_TX)        // Band Bits Tx
      {
      if(setIndex==7)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x01; 
        }
      if(setIndex==6)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x02; 
        }
      if(setIndex==5)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x04; 
        }
      if(setIndex==4)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x08; 
        }
       if(setIndex==3)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x10; 
        }
      if(setIndex==2)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x20; 
        }
      if(setIndex==1)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x40; 
        }
      if(setIndex==0)
        {
        bandBitsTx[band]=bandBitsTx[band] ^ 0x80; 
        }
      mouseScroll=0;
      if(bandBitsTx[band]<0) bandBitsTx[band]=0;
      if(bandBitsTx[band]>255) bandBitsTx[band]=255;
      displaySetting(settingNo);  
      }  
    if(settingNo==BAND_BITS_RX)        // Copy Band Bits to Pluto
      {
      if(mouseScroll>0)
      {
        bandBitsToPluto=1;
      }
      if(mouseScroll<0)
      {
         bandBitsToPluto=0;
      }
      mouseScroll=0;
      displaySetting(settingNo);  
      }                 
   if(settingNo==FFT_REF)        // FFT Ref Level
      {
      FFTRef=FFTRef+mouseScroll;
      mouseScroll=0;
      if(FFTRef<-80) FFTRef=-80;
      if(FFTRef>30) FFTRef=30;
      bandFFTRef[band]=FFTRef;
      displaySetting(settingNo);  
      }    
    if(settingNo==TX_ATT)        // Tx Attenuator
      {
      TxAtt=TxAtt+mouseScroll;
      mouseScroll=0;
      if(TxAtt<-89) TxAtt=-89;
      if(TxAtt>0) TxAtt=0;
      bandTxAtt[band]=TxAtt;
      setPlutoTxAtt(TxAtt);
      displaySetting(settingNo);  
      }  
     if(settingNo==RX_GAIN)        // Rx Gain Setting
      {
      if (bandRxGain[band] == 100)
        {
        bandRxGain[band]=maxGain(freq)+1+mouseScroll;
        }
      else
      {
        bandRxGain[band]=bandRxGain[band]+mouseScroll;
      }

      mouseScroll=0;
      if(bandRxGain[band]< minGain(freq)) bandRxGain[band]=minGain(freq);
      if(bandRxGain[band]> maxGain(freq)) bandRxGain[band]=100;
      setPlutoRxGain(bandRxGain[band]);
      displaySetting(settingNo);  
      }
    if(settingNo==SSB_GEQH)        // SSB EQ-H
      { 
      bandssbGeqH[band]=bandssbGeqH[band]+mouseScroll;
      mouseScroll=0;
      if(bandssbGeqH[band]< 0) bandssbGeqH[band]=0;
      if(bandssbGeqH[band]> 200) bandssbGeqH[band]=200;
      setLimessbGeqH(bandssbGeqH[band]);
      displaySetting(settingNo);  
      }
    if(settingNo==SSB_GEQM2)        // SSB EQ-M2
      { 
      bandssbGeqM2[band]=bandssbGeqM2[band]+mouseScroll;
      mouseScroll=0;
      if(bandssbGeqM2[band]< 0) bandssbGeqM2[band]=0;
      if(bandssbGeqM2[band]> 62) bandssbGeqM2[band]=60;
      setLimessbGeqM2(bandssbGeqM2[band]);
      displaySetting(settingNo);  
      }
    if(settingNo==SSB_GEQM1)        // SSB EQ-M1
      { 
      bandssbGeqM1[band]=bandssbGeqM1[band]+mouseScroll;
      mouseScroll=0;
      if(bandssbGeqM1[band]< 0) bandssbGeqM1[band]=0;
      if(bandssbGeqM1[band]> 62) bandssbGeqM1[band]=60;
      setLimessbGeqM1(bandssbGeqM1[band]);
      displaySetting(settingNo);  
      }
    if(settingNo==SSB_GEQL)        // SSB EQ-L
      { 
      bandssbGeqL[band]=bandssbGeqL[band]+mouseScroll;
      mouseScroll=0;
      if(bandssbGeqL[band]< 0) bandssbGeqL[band]=0;
      if(bandssbGeqL[band]> 62) bandssbGeqL[band]=60;
      setLimessbGeqL(bandssbGeqL[band]);
      displaySetting(settingNo);  
      }
    if(settingNo==S_ZERO)        // S Meter Zero
      {
      bandSmeterZero[band]=bandSmeterZero[band]+mouseScroll;
      mouseScroll=0;
      if(bandSmeterZero[band]<-140) bandSmeterZero[band]=-140;
      if(bandSmeterZero[band]>-30) bandSmeterZero[band]=-30;
      displaySetting(settingNo);  
      }   
    if(settingNo==SSB_FILT_LOW)        // SSB Filter Low
      {
      bandSSBFiltLow[band]=bandSSBFiltLow[band]+mouseScroll*10;
      mouseScroll=0;
      if(bandSSBFiltLow[band] < 0) bandSSBFiltLow[band]=0;
      if(bandSSBFiltLow[band] >1000) bandSSBFiltLow[band]=1000;
      setMode(mode);                 //refresh mode to set new filter settings
      displaySetting(settingNo);  
      }                    
    if(settingNo==SSB_FILT_HIGH)        // SSB Filter High
      {
      bandSSBFiltHigh[band]=bandSSBFiltHigh[band]+mouseScroll*10;
      mouseScroll=0;
      if(bandSSBFiltHigh[band] < 1000) bandSSBFiltHigh[band]=1000;
      if(bandSSBFiltHigh[band] > 5000) bandSSBFiltHigh[band]=5000;
      setMode(mode);                 //refresh mode to set new filter settings
      displaySetting(settingNo);  
      }     
    if(settingNo==CW_CARRIER)        // CWID Carrier time
      {
      CWIDkeyDownTime=CWIDkeyDownTime+mouseScroll*100;
      mouseScroll=0;
      if(CWIDkeyDownTime< 0) CWIDkeyDownTime=0;
      if(CWIDkeyDownTime> 12000) CWIDkeyDownTime=12000;
      displaySetting(settingNo);  
      }  
      
     if(settingNo==CWID)            //CW Ident string
     {
     int c;
     c= morseIdent[setIndex];
     c=c+mouseScroll;
     if(mouseScroll>0)
       {
         if(c>95) c=47;
         if((c>57)&&(c<65)) c=65;
         if(c>90) c=95;
       }
     if(mouseScroll<0)
       {
          if(c<47) c=95;
          if((c>90)&&(c<95)) c=90;
          if((c>57)&&(c<65)) c=57;
       }
     
     morseIdent[setIndex] = c;
     mouseScroll=0;
     displaySetting(settingNo); 
     }  
     
  if(settingNo==BREAK_IN_TIME)        // CW Break In Timer
      {
      breakInTime=breakInTime+mouseScroll;
      mouseScroll=0;
      if(breakInTime< 50) breakInTime=50;
      if(breakInTime> 200) breakInTime=200;
      displaySetting(settingNo);  
      } 
      
  if(settingNo==BANDS24)        // 24 band mode
      {
      if(mouseScroll > 0)   bands24= 1;
      if(mouseScroll < 0)   bands24= 0;
      mouseScroll=0;  
      displaySetting(settingNo);  
      }

  if(settingNo==WF_FLOOR)       // Waterfall brightness offset
      {
      WFFloor = WFFloor + mouseScroll;
      if(WFFloor < -20) WFFloor = -20;
      if(WFFloor >  20) WFFloor =  20;
      wf_low_smooth = -999.0f;  // force recalculation
      mouseScroll=0;
      displaySetting(settingNo);
      }
  if(settingNo==SPEC_STRETCH)      // Spectrum stretch
      {
      specStretch = specStretch + mouseScroll;
      if(specStretch < 0)  specStretch = 0;
      if(specStretch > 10) specStretch = 10;
      mouseScroll = 0;
      displaySetting(settingNo);
      }
  if(settingNo==CALLSIGN)          // Callsign entry
      {
      int c = (unsigned char)callSign[setIndex];
      c = c + mouseScroll;
      if(mouseScroll > 0)
        {
        if(c > 95) c = 47;
        if((c > 57) && (c < 65)) c = 65;
        if(c > 90) c = 95;
        }
      if(mouseScroll < 0)
        {
        if(c < 47) c = 95;
        if((c > 90) && (c < 95)) c = 90;
        if((c > 57) && (c < 65)) c = 57;
        }
      callSign[setIndex] = (char)c;
      // trim trailing underscores/spaces
      callSignLen = 0;
      for(int i=0;i<11;i++) if(callSign[i]>47 && callSign[i]!=95) callSignLen=i+1;
      mouseScroll = 0;
      displaySetting(settingNo);
      }
  if(settingNo==AGC_ADJ)        // AGC level adjust (reference dB)
      {
      AGCAdj = AGCAdj + mouseScroll;
      if(AGCAdj < -20) AGCAdj = -20;
      if(AGCAdj >  20) AGCAdj =  20;
      mouseScroll=0;
      // send 'Y<value>' via FIFO
      char agcAdjStr[8];
      sprintf(agcAdjStr, "Y%d", AGCAdj);
      sendFifo(agcAdjStr);
      displaySetting(settingNo);
      }
      
  if(settingNo==ROTATE)        // Rotate Screen
      {
      if(mouseScroll > 0)   screenrotate= 1;
      if(mouseScroll < 0)   screenrotate= 0;
      mouseScroll=0;
      setRotation(screenrotate);
      initGUI(); 
      showSettingsMenu(); 
      displaySetting(settingNo);  
      }                                                                                                                    
}

               
int minGain(double freq)
{
double rxfreq;

rxfreq=(freq+bandRxOffset[band])/bandRxHarmonic[band];

if(rxfreq<1300)
 {
 return -1;
 }
if((rxfreq>=1300) && (rxfreq<4000))
  {
  return -3;
  }
if(rxfreq>=4000)
  {
  return -10;
  }
  
return 0;
}

int maxGain(double freq)
{
double rxfreq; 

rxfreq=(freq+bandRxOffset[band])/bandRxHarmonic[band];

if(rxfreq<1300)
 {
 return 73;
 }
if((rxfreq>=1300) && (rxfreq<4000))
  {
  return 71;
  }
if(rxfreq>=4000)
  {
  return 62;
  }
  
return 73;
}



// ══ PORT V3H ══════════════════════════════════════════════════
// Top bar positions
void drawButtonIC7300(int x, int y, const char *label, int state)
{
  // ── Colour scheme ─────────────────────────────────────────────
  // bg fill (RGB)
  int bgR, bgG, bgB;
  // outer border
  int brR, brG, brB;
  // accent top bar
  int acR, acG, acB;
  // text
  int txR, txG, txB;
  // accent bar height
  int acH;

  switch(state)
    {
    case BTN_DANGER:
      bgR=255; bgG=0;   bgB=0;    // vermelho brilhante — fundo
      brR=74;  brG=24;  brB=16;   // castanho escuro — borda
      acR=255; acG=255; acB=255;  // branco — accent
      txR=255; txG=255; txB=255;  // branco — texto
      acH=3;
      break;
    case BTN_WARN:
      bgR=20;  bgG=18;  bgB=12;   // castanho muito escuro — fundo
      brR=58;  brG=48;  brB=16;   // castanho âmbar escuro — borda
      acR=170; acG=136; acB=0;    // âmbar — accent
      txR=192; txG=152; txB=32;   // âmbar claro — texto
      acH=3;
      break;
    case BTN_AMBER:
      bgR=22;  bgG=20;  bgB=13;   // quase preto — fundo
      brR=42;  brG=64;  brB=96;   // azul escuro — borda
      acR=221; acG=153; acB=0;    // âmbar dourado — accent
      txR=200; txG=160; txB=48;   // âmbar claro — texto
      acH=3;
      break;
    case BTN_ON:
      bgR=0;  bgG=170;  bgB=187;  // cyan IC-7300 — fundo
      brR=0;  brG=170;  brB=187;  // cyan IC-7300 — borda (sem separação)
      acR=0;  acG=0;    acB=0;    // preto — accent
      txR=0;  txG=0;    txB=0;    // preto — texto sobre fundo cyan
      acH=3;
      break;
    default:                       // BTN_OFF
      bgR=50;  bgG=50;  bgB=50;   // cinzento médio — fundo
      brR=30;  brG=46;  brB=66;   // azul acinzentado — borda
      acR=0;   acG=136; acB=153;  // cyan escuro — accent
      txR=255; txG=255; txB=255;  // branco — texto
      acH=2;
      break;
    }

  // ── Clear area (black) ────────────────────────────────────────
  for(int r=y; r<y+buttonHeight; r++)
    drawLine(x, r, x+buttonWidth-1, r, 0,0,0);

  // ── Fill background ───────────────────────────────────────────
  for(int r=y+1; r<y+buttonHeight-1; r++)
    drawLine(x+1, r, x+buttonWidth-2, r, bgR,bgG,bgB);

  // ── Outer border ──────────────────────────────────────────────
  drawLine(x,               y,                x+buttonWidth-1,  y,                brR,brG,brB);
  drawLine(x,               y+buttonHeight-1, x+buttonWidth-1,  y+buttonHeight-1, brR,brG,brB);
  drawLine(x,               y,                x,                y+buttonHeight-1, brR,brG,brB);
  drawLine(x+buttonWidth-1, y,                x+buttonWidth-1,  y+buttonHeight-1, brR,brG,brB);

  // ── Inner border (very subtle) ────────────────────────────────
  int ib = state==BTN_OFF ? 14 : 18;
  drawLine(x+2,             y+2,              x+buttonWidth-3,  y+2,              ib,ib+4,ib+8);
  drawLine(x+2,             y+buttonHeight-3, x+buttonWidth-3,  y+buttonHeight-3, ib,ib+4,ib+8);
  drawLine(x+2,             y+2,              x+2,              y+buttonHeight-3, ib,ib+4,ib+8);
  drawLine(x+buttonWidth-3, y+2,              x+buttonWidth-3,  y+buttonHeight-3, ib,ib+4,ib+8);

  // ── Accent top bar ────────────────────────────────────────────
  for(int r=y+1; r<y+1+acH; r++)
    drawLine(x+4, r, x+buttonWidth-5, r, acR,acG,acB);

  // ── Text — drawn with own bitmap font, pixel by pixel ────────
  // We do NOT use displayStr() because its background pixel colour
  // depends on Graphics.c internals we cannot control.
  // Instead: draw each pixel explicitly with drawLine, using bgR/G/B
  // for 'off' pixels and txR/G/B for 'on' pixels.
  // Font: 5 columns × 7 rows per character, scaled ×2 → 10×14px
  // Characters supported: A-Z 0-9 space + - . / %
  {
  // 5×7 bitmap font — each char = 5 bytes (one per column, bit0=top)
  static const unsigned char btnFont[][5] = {
    {0x7E,0x09,0x09,0x09,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x00,0x00,0x00,0x00}, // space (index 36)
    {0x00,0x08,0x08,0x08,0x00}, // - (index 37)
    {0x00,0x60,0x60,0x00,0x00}, // . (index 38)
    {0x20,0x10,0x08,0x04,0x02}, // / (index 39)
    {0x08,0x08,0x3E,0x08,0x08}, // + (index 40)
  };

  // Map a character to font index
  #define BTN_FONT_IDX(c) ( \
    ((c)>='A'&&(c)<='Z') ? (c)-'A' : \
    ((c)>='a'&&(c)<='z') ? (c)-'a' : \
    ((c)>='0'&&(c)<='9') ? 26+(c)-'0' : \
    (c)==' ' ? 36 : (c)=='-' ? 37 : (c)=='.' ? 38 : \
    (c)=='/' ? 39 : (c)=='+' ? 40 : 36)

  const int SCALE = 2;                     // ×2 → 10×14px per char
  const int CW    = 5*SCALE + SCALE;       // 12px per char (5cols + 1gap)
  const int CH    = 7*SCALE;              // 14px char height

  // Count printable characters
  int len = 0;
  for(int i=0; label[i]; i++) if(label[i]!=' ') len++;
  int tlen = 0;
  for(int i=0; label[i]; i++) tlen++;  // total length including spaces

  if(tlen > 0)
    {
    int strW = tlen * CW;
    int tx   = x + (buttonWidth  - strW) / 2;
    int ty   = y + (buttonHeight - CH)   / 2 + 1;

    // Draw each character pixel by pixel
    for(int ci=0; label[ci]; ci++)
      {
      int fi = BTN_FONT_IDX((unsigned char)label[ci]);
      int cx = tx + ci * CW;

      for(int col=0; col<5; col++)
        {
        unsigned char coldata = btnFont[fi][col];
        for(int row=0; row<7; row++)
          {
          int bit = (coldata >> row) & 1;
          int px  = cx + col*SCALE;
          int py  = ty + row*SCALE;
          // Draw SCALE×SCALE pixel block
          for(int sy=0; sy<SCALE; sy++)
            drawLine(px, py+sy, px+SCALE-1, py+sy,
                     bit ? txR : bgR,
                     bit ? txG : bgG,
                     bit ? txB : bgB);
          }
        }
      // Gap column between chars (background colour)
      int gx = cx + 5*SCALE;
      for(int sy=0; sy<CH; sy++)
        drawLine(gx, ty+sy, gx+SCALE-1, ty+sy, bgR,bgG,bgB);
      }
    }
  #undef BTN_FONT_IDX
  }
}

void doUpgrade(char* script)
{
  char cmd[320];
  char verStr[64];

  // Try to read version from version.txt on PEN
  char verPath[256];
  snprintf(verPath, sizeof(verPath), "%s", script);
  char* slash = strrchr(verPath, '/');
  if(slash) strcpy(slash+1, "version.txt");
  FILE* vf = fopen(verPath, "r");
  if(vf)
    {
    fgets(verStr, sizeof(verStr), vf);
    fclose(vf);
    int vl = strlen(verStr);
    if(vl > 0 && verStr[vl-1] == '\n') verStr[vl-1] = '\0';
    }
  else
    {
    strcpy(verStr, "unknown");
    }

  // Stop SDR cleanly
  setBandBits(0);
  sendFifo("H0");
  sendFifo("Q");
  writeConfig();
  sleep(1);

  // Upgrade screen
  clearScreen();
  textSize = 3; setForeColour(180,180,180);
  gotoXY(160,160); displayStr("UPGRADING...");
  textSize = 2; setForeColour(255,255,0);
  gotoXY(160,220); displayStr("Version: "); displayStr(verStr);
  textSize = 2; setForeColour(255,100,0);
  gotoXY(130,270); displayStr("Do NOT remove USB pen");
  textSize = 1; setForeColour(128,128,128);
  gotoXY(130,310); displayStr("System will reboot when complete");

  sleep(1);

  // Run upgrade script
  snprintf(cmd, sizeof(cmd), "sudo bash %s", script);
  system(cmd);

  // Completion message
  textSize = 2; setForeColour(0,255,0);
  gotoXY(160,360); displayStr("Done! Rebooting...");
  sleep(2);

  system("sudo reboot");
}

void setAGC(int mode)
{
  if(mode < 0 || mode > 4) mode = 2;
  agcMode = mode;

  char agcStr[4];
  sprintf(agcStr, "J%d", agcMode);
  sendFifo(agcStr);

  const char* agcLabels[] = {"AGC OFF","AGC F","AGC M","AGC S","AGC L"};
  int agcState = (agcMode == 0) ? BTN_WARN : BTN_ON;
  drawButtonIC7300(agcButtonX, agcButtonY, agcLabels[agcMode], agcState);

  configCounter = configDelay;
}

char* findUpgradePen(void)
{
  static char scriptPath[256];

  const char* mountRoots[] =
    {
    "/media/pi",
    "/mnt",
    NULL
    };

  for(int i = 0; mountRoots[i] != NULL; i++)
    {
    DIR *d = opendir(mountRoots[i]);
    if(!d) continue;

    struct dirent *e;
    while((e = readdir(d)) != NULL)
      {
      if(e->d_name[0] == '.') continue;

      snprintf(scriptPath, sizeof(scriptPath),
               "%s/%s/langstone_upgrade.sh",
               mountRoots[i], e->d_name);

      if(access(scriptPath, F_OK) == 0)
        {
        closedir(d);
        return scriptPath;
        }
      }
    closedir(d);
    }
  return NULL;
}

void drawRoundBox(int x1, int y1, int x2, int y2, int r, int g, int b)
{
  drawLine(x1+1, y1, x2-1, y1, r,g,b);  // top
  drawLine(x1+1, y2, x2-1, y2, r,g,b);  // bottom
  drawLine(x1, y1+1, x1, y2-1, r,g,b);  // left
  drawLine(x2, y1+1, x2, y2-1, r,g,b);  // right
  drawLine(x1+1, y1+1, x1+1, y1+1, r,g,b);  // top-left
  drawLine(x2-1, y1+1, x2-1, y1+1, r,g,b);  // top-right
  drawLine(x1+1, y2-1, x1+1, y2-1, r,g,b);  // bottom-left
  drawLine(x2-1, y2-1, x2-1, y2-1, r,g,b);  // bottom-right
}

void drawScaleLabel(int x, int y, const char *label)
{
  static const unsigned char slFont[][5] = {
    {0x7E,0x09,0x09,0x09,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},   // A-Z
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},   // 0-9
    {0x00,0x00,0x00,0x00,0x00},   // 36=space
    {0x00,0x08,0x08,0x08,0x00},   // 37=-
    {0x00,0x60,0x60,0x00,0x00},   // 38=.
    {0x20,0x10,0x08,0x04,0x02},   // 39=/
    {0x08,0x08,0x3E,0x08,0x08},   // 40=+
    {0x00,0x06,0x0A,0x12,0x00},   // 41=k (small)
  };
  const int S = 2;   // scale ×2 → 10×14px per char
  int cx = x;
  for(int ci=0; label[ci]; ci++, cx+=5*S+S)
    {
    unsigned char c = (unsigned char)label[ci];
    int fi;
    if(c>='A'&&c<='Z')      fi=c-'A';
    else if(c>='a'&&c<='z') fi=c-'a';
    else if(c>='0'&&c<='9') fi=26+c-'0';
    else if(c==' ')  fi=36;
    else if(c=='-')  fi=37;
    else if(c=='.')  fi=38;
    else if(c=='/')  fi=39;
    else if(c=='+')  fi=40;
    else if(c=='k')  fi=41;
    else             fi=36;
    for(int col=0; col<5; col++)
      {
      unsigned char cd = slFont[fi][col];
      for(int row=0; row<7; row++)
        {
        int bit = (cd>>row)&1;
        int px = cx + col*S;
        int py = y  + row*S;
        for(int sy=0; sy<S; sy++)
          drawLine(px, py+sy, px+S-1, py+sy,
                   bit?255:0, bit?255:0, bit?255:0);  // white on black
        }
      }
    }
}





void takeSnapshot(void)
{
  const int W = 800, H = 480, BPP = 4;

  // Flash SNAP button — give display time to flush before capturing fb0
  drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_ON);
  usleep(150000);  // 150ms — enough for LCD controller to render the button

  // Open framebuffer
  FILE *fb = fopen("/dev/fb0", "rb");
  if(!fb)
    {
    displayError("  SNAP: cannot open fb0  ");
    drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);
    return;
    }
  unsigned char *fbuf = (unsigned char*)malloc(W * H * BPP);
  if(!fbuf) { fclose(fb); return; }
  fread(fbuf, BPP, W*H, fb);
  fclose(fb);

  // Convert BGRA→RGB
  unsigned char *rgb = (unsigned char*)malloc(W * H * 3);
  if(!rgb) { free(fbuf); return; }
  for(int i = 0; i < W*H; i++)
    {
    rgb[i*3+0] = fbuf[i*BPP+2];  // R
    rgb[i*3+1] = fbuf[i*BPP+1];  // G
    rgb[i*3+2] = fbuf[i*BPP+0];  // B
    }
  free(fbuf);

  // Filename: snapshot_CU2ED_HHMMSS_DDMMYYYY.png
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char fname[128];
  snprintf(fname, sizeof(fname),
           "/home/pi/Langstone/snap/snapshot_%s_%02d%02d%02d_%02d%02d%04d.png",
           callSign,
           t->tm_hour, t->tm_min, t->tm_sec,
           t->tm_mday, t->tm_mon+1, t->tm_year+1900);

  // ── Write PNG using zlib (always available on RPi) ─────────────
  static uint32_t crc_tab[256];
  static int crc_ready = 0;
  if(!crc_ready)
    {
    for(int n=0;n<256;n++)
      { uint32_t c=n; for(int k=0;k<8;k++) c=(c&1)?0xEDB88320L^(c>>1):c>>1; crc_tab[n]=c; }
    crc_ready=1;
    }
  #define PCRC(type,data,len) ({ uint32_t _c=0xFFFFFFFF; for(int _i=0;_i<4;_i++) _c=crc_tab[(_c^((unsigned char*)(type))[_i])&0xFF]^(_c>>8); for(int _i=0;_i<(int)(len);_i++) _c=crc_tab[(_c^((unsigned char*)(data))[_i])&0xFF]^(_c>>8); _c^0xFFFFFFFF; })
  #define PU32(f,v) do{fputc(((v)>>24)&0xFF,f);fputc(((v)>>16)&0xFF,f);fputc(((v)>>8)&0xFF,f);fputc((v)&0xFF,f);}while(0)

  // Build raw scanlines (filter byte 0 + RGB per row)
  int raw_sz = H*(1+W*3);
  unsigned char *raw = (unsigned char*)malloc(raw_sz);
  if(!raw) { free(rgb); return; }
  for(int y=0;y<H;y++) { raw[y*(1+W*3)]=0; memcpy(&raw[y*(1+W*3)+1],&rgb[y*W*3],W*3); }
  free(rgb);

  uLongf comp_sz = compressBound(raw_sz);
  unsigned char *comp = (unsigned char*)malloc(comp_sz);
  if(!comp) { free(raw); return; }
  compress2(comp, &comp_sz, raw, raw_sz, Z_BEST_SPEED);
  free(raw);

  FILE *pf = fopen(fname, "wb");
  if(!pf)
    {
    free(comp);
    displayError("  SNAP: write failed  ");
    drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);
    return;
    }

  // Signature
  const unsigned char sig[]={137,80,78,71,13,10,26,10};
  fwrite(sig,1,8,pf);

  // IHDR
  unsigned char ihdr[13]={(W>>24)&0xFF,(W>>16)&0xFF,(W>>8)&0xFF,W&0xFF,
                          (H>>24)&0xFF,(H>>16)&0xFF,(H>>8)&0xFF,H&0xFF,8,2,0,0,0};
  PU32(pf,13); fwrite("IHDR",1,4,pf); fwrite(ihdr,1,13,pf); PU32(pf,PCRC("IHDR",ihdr,13));

  // IDAT
  PU32(pf,(uint32_t)comp_sz); fwrite("IDAT",1,4,pf); fwrite(comp,1,comp_sz,pf);
  PU32(pf,PCRC("IDAT",comp,comp_sz));
  free(comp);

  // IEND
  PU32(pf,0); fwrite("IEND",1,4,pf); PU32(pf,PCRC("IEND","",0));
  fclose(pf);

  #undef PCRC
  #undef PU32

  // ── Show filename on status line (between waterfall and settings) ─
  {
  // Extract just the filename without path
  char *fn = strrchr(fname, '/');
  fn = fn ? fn+1 : fname;
  // Clear the ENTIRE zone between waterfall bottom and settings line
  // textSize=2 at settingY=390 has pixels reaching up to ~settingY-14
  // Clear from settingY-26 to settingY-1 (26px strip, fully black)
  for(int cy = settingY-29; cy < settingY; cy++)
    drawLine(0, cy, 799, cy, 0,0,0);
  // Show filename centred vertically in the strip — Y = settingY-18
  gotoXY(0, settingY-18);
  setForeColour(255,220,0);
  textSize=1;
  char snapMsg[140];
  snprintf(snapMsg, sizeof(snapMsg), "%s  saved", fn);
  displayStr(snapMsg);
  }

  drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);
}

// ══ END PORT V3H ══════════════════════════════════════════════

void showSettingsMenu(void)
  {
    drawButtonIC7300(funcButtonsX,              funcButtonsY, "MENU",   BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX, funcButtonsY, "MODE",   BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "PREV", BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*3, funcButtonsY, "NEXT", BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "UPGRDE", BTN_OFF);
    if(portsdownPresent==1)
      {drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "EXIT",   BTN_DANGER);}
    else
      {drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "REBOOT", BTN_DANGER);}
    drawButtonIC7300(funcButtonsX+buttonSpaceX*6, funcButtonsY, "OFF",  BTN_DANGER);
  }


void displaySetting(int se)
{
  char valStr[30];
  gotoXY(0,settingY);
  textSize=2;                                                            
  setForeColour(255,255,255);
  displayStr("                                                ");
  gotoXY(0,settingY+8);
  displayStr("                                                ");
  gotoXY(settingX,settingY);
  if((se==TX_OFFSET)||(se==RX_OFFSET)||(se==CWID))
  {
  gotoXY(0,settingY);
  }
  displayStr(settingText[se]);
 
 if(se==SSB_MIC)
  {
  sprintf(valStr,"%d",SSBMic);
  displayStr(valStr);
  }
 if(se==FM_MIC)
  {
  sprintf(valStr,"%d",FMMic);
  displayStr(valStr);
  }
 if(se==AM_MIC)
  {
  sprintf(valStr,"%d",AMMic);
  displayStr(valStr);
  } 
if(se==REP_SHIFT)
  {
  sprintf(valStr,"%.5f",bandRepShift[band]);
  displayStr(valStr);
  }
if(se==CTCSS)
  {
  sprintf(valStr,"%.1f Hz",CTCSSTone[bandCTCSS[band]]/10.0);
  displayStr(valStr);
  }
  if(se==RX_OFFSET)
  {
  sprintf(valStr,"%.5f",bandRxOffset[band]);
  displayStr(valStr);
  displayStr(" Rx Freq= ");
  sprintf(valStr,"%.5f",freq+bandRxOffset[band]);
  displayStr(valStr);
  }
  if(se==RX_HARMONIC)
  {
  sprintf(valStr,"X%d",bandRxHarmonic[band]);
  displayStr(valStr);
  } 
  if(se==TX_OFFSET)
  {
  sprintf(valStr,"%.5f",bandTxOffset[band]);
  displayStr(valStr);
  displayStr(" Tx Freq= ");
  sprintf(valStr,"%.5f",freq+bandTxOffset[band]);
  displayStr(valStr);
  }
  if(se==TX_HARMONIC)
  {
  sprintf(valStr,"X%d",bandTxHarmonic[band]);
  displayStr(valStr);
  }   
  if(se==BAND_BITS_RX)
  { 
  maxSetIndex=7;
  setForeColour(255,255,255);
  for(int b=128;b>0;b=b>>1)
    {
      if(((setIndex==7)&&(b==1)) || ((setIndex==6)&&(b==2))  || ((setIndex==5)&&(b==4)) || ((setIndex==4)&&(b==8)) || ((setIndex==3)&&(b==16))  || ((setIndex==2)&&(b==32))  || ((setIndex==1)&&(b==64))  || ((setIndex==0)&&(b==128)))
        {
          setForeColour(0,255,0);
        }
      else
        {
          setForeColour(255,255,255);
        }
      if(bandBitsRx[band] & b)
        {
        displayChar('1');
        }
      else
        {
        displayChar('0');
        }
    } 
  }
if(se==BAND_BITS_TX)
  { 
  maxSetIndex=7;
  setForeColour(255,255,255);
  for(int b=128;b>0;b=b>>1)
    {
      if(((setIndex==7)&&(b==1)) || ((setIndex==6)&&(b==2))  || ((setIndex==5)&&(b==4)) || ((setIndex==4)&&(b==8)) || ((setIndex==3)&&(b==16))  || ((setIndex==2)&&(b==32))  || ((setIndex==1)&&(b==64))  || ((setIndex==0)&&(b==128)))
        {
          setForeColour(0,255,0);
        }
      else
        {
          setForeColour(255,255,255);
        }
      if(bandBitsTx[band] & b)
        {
        displayChar('1');
        }
      else
        {
        displayChar('0');
        }
    } 
  }
  if(se==BAND_BITS_RX)
  {
    if(bandBitsToPluto==1)
      {
      sprintf(valStr,"Yes");
      }
    else
      {
      sprintf(valStr,"No");
      }
   displayStr(valStr);  
  } 
  if(se==FFT_REF)
  {
  sprintf(valStr,"%d",FFTRef);
  displayStr(valStr);
  }
  if(se==TX_ATT)
  {
  sprintf(valStr,"%d dB",TxAtt);
  displayStr(valStr);
  }
  if(se==RX_GAIN)
  {
    if(bandRxGain[band]>maxGain(freq))
    {
    sprintf(valStr,"Auto");
    }
    else
    {
    sprintf(valStr,"%d dB",bandRxGain[band]);
    }
  displayStr(valStr);
  }
  if(se==S_ZERO)
  {
  sprintf(valStr,"%.0f dB",bandSmeterZero[band]);
  displayStr(valStr);
  }
  if(se==SSB_FILT_LOW)
  {
  sprintf(valStr,"%d Hz",bandSSBFiltLow[band]);
  displayStr(valStr);
  }
  if(se==SSB_FILT_HIGH)
  {
  sprintf(valStr,"%d Hz",bandSSBFiltHigh[band]);
  displayStr(valStr);
  }
  if(se==CW_CARRIER)
  {
  sprintf(valStr,"%d Secs",CWIDkeyDownTime/100);
  displayStr(valStr);
  }
  if(se==CWID)
  {
    maxSetIndex=MORSEIDENTLENGTH-2;
   for(int c=0; c<MORSEIDENTLENGTH;c++)
    {
      if(setIndex==c)
        {
          setForeColour(0,255,0);
        }
      else
        {
          setForeColour(255,255,255);
        }
      if(morseIdent[c]>0)
        {
          if((morseIdent[c]==95) && (setIndex!=c))
            {
              displayChar(32);
            }
          else
            {
              displayChar(morseIdent[c]);
            }
        } 
      else
        {
          if(setIndex>=c)
            {
            setIndex=c;
            morseIdent[c]=95;
            setForeColour(0,255,0);
            displayChar(95);
            morseIdent[c+1]=0;          
            }
          break;
        }   
    }
  }
  
 if(se==BREAK_IN_TIME)
  {
  sprintf(valStr,"%d ms",breakInTime*10);
  displayStr(valStr);
  }  
  
if(se==BANDS24)
  {
    if(bands24 == 0)
    {
      displayStr("No");
    }
    else
    {
        displayStr("Yes");
    }
  }

  if(se==WF_FLOOR)
  {
  sprintf(valStr,"%d",WFFloor);
  displayStr(valStr);
  }
  if(se==SPEC_STRETCH)
  {
  if(specStretch == 0) displayStr("Off (linear)");
  else
    {
    char ssStr[16];
    // exponent = 1.0 - specStretch * 0.07  (0=1.0, 5=0.65, 10=0.30)
    float exp = 1.0f - specStretch * 0.07f;
    sprintf(ssStr,"%d (%.2f)", specStretch, exp);
    displayStr(ssStr);
    }
  }
  if(se==CALLSIGN)
  {
  maxSetIndex = 10;
  for(int c=0; c<11; c++)
    {
    if(setIndex==c) setForeColour(0,255,0);
    else            setForeColour(255,255,255);
    char ch = callSign[c];
    // _ (95) = space placeholder: show ' ' always except cursor pos shows '_'
    if(ch == 95)
      { if(setIndex == c) displayChar('_'); else displayChar(' '); }
    else if(ch >= 47)
      displayChar(ch);
    else
      displayChar(' ');
    }
  setForeColour(255,255,255);
  // Update live display
  static char lastCS[12]="";
  if(strcmp(callSign,lastCS)!=0) { strncpy(lastCS,callSign,11); drawCallsignDisplay(); }
  }
  if(se==AGC_ADJ)
  {
  sprintf(valStr,"%+d dB",AGCAdj);
  displayStr(valStr);
  }
    
 if(se==ROTATE)
  {
    if(screenrotate == 0)
    {
      displayStr("No");
    }
    else
    {
        displayStr("Yes");
    }
  }
  if(se==SSB_GEQH)
  {
  sprintf(valStr,"%d",bandssbGeqH[band]);
  displayStr(valStr);
  }
  if(se==SSB_GEQM2)
  {
  sprintf(valStr,"%d",bandssbGeqM2[band]);
  displayStr(valStr);
  }
  if(se==SSB_GEQM1)
  {
  sprintf(valStr,"%d",bandssbGeqM1[band]);
  displayStr(valStr);
  }
  if(se==SSB_GEQL)
  {
  sprintf(valStr,"%d",bandssbGeqL[band]);
  displayStr(valStr);
  }
}

int readConfig(void)
{
FILE * conffile;
char variable[50];
char value[100];
char vname[20];

conffile=fopen("/home/pi/Langstone/Langstone_Pluto.conf","r");

if(conffile==NULL)
  {
    return -1;
  }

while(fscanf(conffile,"%49s %99s [^\n]\n",variable,value) !=EOF)
  {
     
    if(strstr(variable,"CW_IDENT"))
      {
       value[MORSEIDENTLENGTH-1]=0;           //force to maximum length if necessary
       strcpy(morseIdent,value);
      }
    if(strstr(variable,"CWID_KEY_DOWN_TIME"))
      {
        sscanf(value,"%d",&CWIDkeyDownTime);
        CWIDkeyDownTime=CWIDkeyDownTime*100;
      }

    
    for(int b=0;b<numband;b++)
    {
    sprintf(vname,"bandFreq%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%lf",&bandFreq[b]);
    sprintf(vname,"bandMode%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandMode[b]);   
    sprintf(vname,"bandTxOffSet%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%lf",&bandTxOffset[b]); 
    sprintf(vname,"bandTxHarmonic%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandTxHarmonic[b]);             
    sprintf(vname,"bandRxOffSet%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%lf",&bandRxOffset[b]);      
    sprintf(vname,"bandRxHarmonic%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandRxHarmonic[b]);   
    sprintf(vname,"bandRepShift%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%lf",&bandRepShift[b]);
    sprintf(vname,"bandCTCSS%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandCTCSS[b]);
    sprintf(vname,"bandDuplex%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandDuplex[b]);  
 
 //handle old config file format by converting bandbits to bandbitsRx and BandbitsTx   
    sprintf(vname,"bandBits%02d",b);
    if(strstr(variable,vname)) 
    {
    sscanf(value,"%d",&bandBitsRx[b]); 
    sscanf(value,"%d",&bandBitsTx[b]);
    }
       
    sprintf(vname,"bandRxBits%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandBitsRx[b]); 
    sprintf(vname,"bandTxBits%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandBitsTx[b]);    
    sprintf(vname,"bandFFTRef%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandFFTRef[b]);     
    sprintf(vname,"bandSquelchUSB%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][USB]);
    sprintf(vname,"bandSquelchLSB%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][LSB]);
    sprintf(vname,"bandSquelchCW%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][CW]);
    sprintf(vname,"bandSquelchCWN%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][CWN]);
    sprintf(vname,"bandSquelchFM%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][FM]);
    sprintf(vname,"bandSquelchAM%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSquelch[b][AM]);     
    sprintf(vname,"bandTxAtt%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandTxAtt[b]);
    sprintf(vname,"bandRxGain%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandRxGain[b]);
    sprintf(vname,"bandssbGeqH%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandssbGeqH[b]);
    sprintf(vname,"bandssbGeqM2%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandssbGeqM2[b]);
    sprintf(vname,"bandssbGeqM1%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandssbGeqM1[b]);
    sprintf(vname,"bandssbGeqL%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandssbGeqL[b]);
    sprintf(vname,"bandSmeterZero%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%f",&bandSmeterZero[b]);
    sprintf(vname,"bandSSBFiltLow%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSSBFiltLow[b]); 
    sprintf(vname,"bandSSBFiltHigh%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandSSBFiltHigh[b]); 
    sprintf(vname,"bandFFTBW%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandFFTBW[b]);    
    }

    
    if(strstr(variable,"currentBand")) sscanf(value,"%d",&band);
    if(strstr(variable,"tuneDigit")) sscanf(value,"%d",&tuneDigit);   
    if(strstr(variable,"mode")) sscanf(value,"%d",&mode);
    if(strstr(variable,"SSBMic")) sscanf(value,"%d",&SSBMic);
    if(strstr(variable,"FMMic")) sscanf(value,"%d",&FMMic);
    if(strstr(variable,"AMMic")) sscanf(value,"%d",&AMMic);
    if(strstr(variable,"volume")) sscanf(value,"%d",&volume);
    if(strstr(variable,"breakInTime")) sscanf(value,"%d",&breakInTime);
    if(strstr(variable,"bandBitsToPluto")) sscanf(value,"%d",&bandBitsToPluto);
    if(strstr(variable,"bands24")) sscanf(value,"%d",&bands24);
    if(strstr(variable,"WFFloor")) sscanf(value,"%d",&WFFloor);
    if(strstr(variable,"AGCAdjMode")) sscanf(value,"%d %d %d %d %d %d",&AGCAdjByMode[0],&AGCAdjByMode[1],&AGCAdjByMode[2],&AGCAdjByMode[3],&AGCAdjByMode[4],&AGCAdjByMode[5]);
    if(strstr(variable,"callSign")) { strncpy(callSign,value,11); callSign[11]=0; }
    if(strstr(variable,"RotateScreen")) sscanf(value,"%d",&screenrotate);
    if(mode>nummode-1) mode=0;
            
  }

fclose(conffile);
return 0;

}

int writeConfig(void)
{
FILE * conffile;
char variable[80];
int value;

bandFreq[band]=freq;

for(int i=0;i<MORSEIDENTLENGTH;i++)                                                 //trim morse Ident to remove redundant spaces.
  {
  if((morseIdent[i]==95) && (morseIdent[i+1]==95))                    //find double space
    {
      morseIdent[i]=0;                                                //terminate string here
      break;
    }
  }


conffile=fopen("/home/pi/Langstone/Langstone_Pluto.conf","w");

if(conffile==NULL)
  {
    return -1;
  }

fprintf(conffile,"CW_IDENT %s\n",morseIdent);
fprintf(conffile,"CWID_KEY_DOWN_TIME %d\n",CWIDkeyDownTime/100);

for(int b=0;b<numband;b++)
{
  fprintf(conffile,"bandFreq%02d %lf\n",b,bandFreq[b]);
  fprintf(conffile,"bandMode%02d %d\n",b,bandMode[b]);
  fprintf(conffile,"bandTxOffSet%02d %lf\n",b,bandTxOffset[b]);
  fprintf(conffile,"bandTxHarmonic%02d %d\n",b,bandTxHarmonic[b]);
  fprintf(conffile,"bandRxOffSet%02d %lf\n",b,bandRxOffset[b]);
  fprintf(conffile,"bandRxHarmonic%02d %d\n",b,bandRxHarmonic[b]);
  fprintf(conffile,"bandRepShift%02d %lf\n",b,bandRepShift[b]);
  fprintf(conffile,"bandCTCSS%02d %d\n",b,bandCTCSS[b]);
  fprintf(conffile,"bandDuplex%02d %d\n",b,bandDuplex[b]);
  fprintf(conffile,"bandRxBits%02d %d\n",b,bandBitsRx[b]);
  fprintf(conffile,"bandTxBits%02d %d\n",b,bandBitsTx[b]);
  fprintf(conffile,"bandFFTRef%02d %d\n",b,bandFFTRef[b]);
  fprintf(conffile,"bandSquelchUSB%02d %d\n",b,bandSquelch[b][USB]);
  fprintf(conffile,"bandSquelchLSB%02d %d\n",b,bandSquelch[b][LSB]);
  fprintf(conffile,"bandSquelchCW%02d %d\n",b,bandSquelch[b][CW]);
  fprintf(conffile,"bandSquelchCWN%02d %d\n",b,bandSquelch[b][CWN]);
  fprintf(conffile,"bandSquelchFM%02d %d\n",b,bandSquelch[b][FM]);
  fprintf(conffile,"bandSquelchAM%02d %d\n",b,bandSquelch[b][AM]);
  fprintf(conffile,"bandTxAtt%02d %d\n",b,bandTxAtt[b]);
  fprintf(conffile,"bandRxGain%02d %d\n",b,bandRxGain[b]);
  fprintf(conffile,"bandssbGeqH%02d %d\n",b,bandssbGeqH[b]);
  fprintf(conffile,"bandssbGeqM2%02d %d\n",b,bandssbGeqM2[b]);
  fprintf(conffile,"bandssbGeqM1%02d %d\n",b,bandssbGeqM1[b]);
  fprintf(conffile,"bandssbGeqL%02d %d\n",b,bandssbGeqL[b]);
  fprintf(conffile,"bandSmeterZero%02d %f\n",b,bandSmeterZero[b]);
  fprintf(conffile,"bandSSBFiltLow%02d %d\n",b,bandSSBFiltLow[b]);
  fprintf(conffile,"bandSSBFiltHigh%02d %d\n",b,bandSSBFiltHigh[b]);
  fprintf(conffile,"bandFFTBW%02d %d\n",b,bandFFTBW[b]);    
}

fprintf(conffile,"currentBand %d\n",band);
fprintf(conffile,"tuneDigit %d\n",tuneDigit);
fprintf(conffile,"mode %d\n",mode);
fprintf(conffile,"SSBMic %d\n",SSBMic);
fprintf(conffile,"FMMic %d\n",FMMic);
fprintf(conffile,"AMMic %d\n",AMMic);
fprintf(conffile,"volume %d\n",volume);
fprintf(conffile,"breakInTime %d\n",breakInTime);
fprintf(conffile,"bandBitsToPluto %d\n",bandBitsToPluto);
fprintf(conffile,"bands24 %d\n",bands24);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandWFFloor%02d %d\n",_b,bandWFFloor[_b]);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandAGCAdj%02d %d\n",_b,bandAGCAdj[_b]);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandSpecStretch%02d %d\n",_b,bandSpecStretch[_b]);
fprintf(conffile,"AGCAdjMode %d %d %d %d %d %d\n",AGCAdjByMode[0],AGCAdjByMode[1],AGCAdjByMode[2],AGCAdjByMode[3],AGCAdjByMode[4],AGCAdjByMode[5]);
  fprintf(conffile,"callSign %s\n",callSign);
fprintf(conffile,"RotateScreen %d\n",screenrotate);

fclose(conffile);
return 0;

}


