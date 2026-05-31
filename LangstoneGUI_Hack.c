// ══════════════════════════════════════════════════════════════════════════════
//  Langstone V3H — HackRF GUI  (LangstoneGUI_Hack.c)
//  Hardware: HackRF One, RPi5, LCD 800×480 táctil, GNU Radio 3.10
//  Build: cc LangstoneGUI_Hack.c -o GUI_HackRF -llgpio -lz -lm
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
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <lgpio.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
#include <stdint.h>
#include "Graphics.h"
#include "Touch.h"
#include "Mouse.h"
#include "hmi.h"
#include "Morse.c"
#include <netdb.h>
#include <netinet/in.h>                                                                                                      
#include <arpa/inet.h>

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
void setHackRxFreq(long long rxfreq);
void setHackTxFreq(long long rxfreq);
void setHwRxFreq(double fr);
void setHwTxFreq(double fr);
void detectHw();
int buttonTouched(int bx,int by);
void setKey(int k);
void displayMenu(void);
// ── takeSnapshot — reads /dev/fb0 and saves PPM to Langstone/snap/ ──
// RPi framebuffer: 800×480, format BGRA8888 (32bpp)
// Saves as snap_YYYYMMDD_HHMMSS.ppm — no external libs needed
// If ImageMagick is installed, also converts to PNG


void showSettingsMenu(void);
void displaySetting(int se);
void changeSetting(void);
void processGPIO(void);
void initGPIO(void);
int readConfig(void);
int writeConfig(void);
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
void StatusPanel(void);
void drawButtonIC7300(int x, int y, const char *label, int state);
void setRit(int rit);
void setInputMode(int n);
void gen_palette(char colours[][3],int num_grads);
void drawScaleLabel(int x, int y, const char *label);
void drawRoundBox(int x1, int y1, int x2, int y2, int r, int g, int b);
void takeSnapshot(void);
void setHackTxAmp(int gain);
void setHackTxGain(int gain);
void setHackRxGain(int gain);
void setHackRxAmp(int gain);
void setHackRxBase(int gain);
void setLimessbGeqH(int gain);
void setLimessbGeqM2(int gain);
void setLimessbGeqM1(int gain);
void setLimessbGeqL(int gain);
void setBand(int b);
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

void setDialLock(int d);
void drawTopBox(int textX, int nchars, int r, int g, int b);
void eraseTopBox(int textX, int nchars);
void setBeacon(int b);
char* findUpgradePen(void);
void doUpgrade(char* script);
void setAGC(int mode);
void drawCallsignDisplay(void);
int firstpass=1;
double freq;
double freqInc=0.001;
#define numband 24
int band=3;
#define nummode 6
double bandFreq[numband] = {28.5,50.2,70.200,144.200,432.200,1296.200,2300.2,2320.200,2400.100,3400.100,5760.100,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2,1296.2};
double bandTxOffset[numband]={0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
double bandRxOffset[numband]={0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,-0,0,0,0,0,0,0,0,0,0,0,0,0};
double bandRepShift[numband]={0,0,0,-0.6,1.6,-6.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int bandTxHarmonic[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int bandRxHarmonic[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int bandMode[numband]={0};
int bandBitsRx[numband]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
int bandBitsTx[numband]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
int bandSquelch[numband][nummode]={0};
int bandFFTRef[numband]={-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10,-10};
int bandTxAmp[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
int bandTxGain[numband]={47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47};
int bandRxGain[numband]={40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40,40};  
int bandRxAmp[numband]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}; 
int bandRxBase[numband]={24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24}; 
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
#define minHwFreq 1.0
#define maxHwFreq 5999.99999



int mode=0;
int lastmode=0;
char * modename[nummode]={"USB","LSB","CW ","CWN","FM ","AM "};
enum {USB,LSB,CW,CWN,FM,AM};

#define numSettings 33

char * settingText[numSettings]={"Rotate Screen = ","FFT Ref= ", "S-Meter Zero= ","SSB Gain EQ-H= ","SSB Gain EQ-M2= ","SSB Gain EQ-M1= ","SSB Gain EQ-L= ","SSB Rx Filter High= ","SSB Rx Filter Low= ", "Tx Amp= ","Tx Gain= ","Rx Amp= ","Rx Gain= ","Rx Baseband= ","SSB Mic Gain= ","FM Mic Gain= ","AM Mic Gain= ","Repeater Shift= ","CTCSS= "," Rx Offset= ","Rx Harmonic Mixing= "," Tx Offset= ","Tx Harmonic Mixing= ","Band Bits (Rx)= ","Band Bits (Tx)= ", "CW Ident= ", "CWID Carrier= ", "CW Break-In Hang Time= ", "24 Bands= ","WF Level= ","AGC Adj= ","Callsign= ","Spec Stretch= "};
enum {ROTATE,FFT_REF,S_ZERO,SSB_GEQH,SSB_GEQM2,SSB_GEQM1,SSB_GEQL,SSB_FILT_HIGH,SSB_FILT_LOW,TX_AMP,TX_GAIN,RX_AMP,RX_GAIN,RX_BASE,SSB_MIC,FM_MIC,AM_MIC,REP_SHIFT,CTCSS,RX_OFFSET,RX_HARMONIC,TX_OFFSET,TX_HARMONIC,BAND_BITS_RX,BAND_BITS_TX,CWID,CW_CARRIER,BREAK_IN_TIME,BANDS24,WF_FLOOR,AGC_ADJ,CALLSIGN,SPEC_STRETCH};

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
#define txX 620
#define txY 15
#define versionX 670
#define versionY 15
#define modeX 196
#define modeY 15
#define txvtrX 421
#define txvtrY 15
#define moniX 525
#define moniY 15
#define settingX 200
#define settingY 390
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
#define diallockX 354
#define diallockY 15
#define version "V3H-080525"


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
int statusForceRedraw = 1;          //force full status panel redraw on startup and after settings changes
int upgradeConfirm = 0;             //two-touch safety for USB upgrade (0=idle, 1=waiting confirm)
int agcMode = 2;                    //AGC mode: 0=OFF 1=FAST 2=MED 3=SLOW 4=LONG

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


int tuneDigit=8;
#define maxTuneDigit 11

#define TXDELAY 10000      //10ms delay between setting Tx output bit and sending tx command to SDR
#define RXDELAY 10000       //10ms delay between sending rx command to SDR and setting Tx output bit low. 

#define TXHACKDELAY 400000      //400ms time that the Tx clock rate is temporarily increased at the start of each transmission. This helps to prevent tx Buffer Underruns with the HackRF One. 
#define BurstLength 500000     //length of 1750Hz Burst   500ms

char mousePath[30];
char touchPath[30];
char hmiPath[30];
int mousePresent;
int touchPresent;
int hmiPresent;
int portsdownPresent;

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

//robs Waterfall — optimised with ring buffer and single fread()

float fftRowBuf[512];                   // single-read buffer for one FFT frame
FILE *fftstream;
FILE *txfftstream;
float buf[512][130];                    // waterfall history buffer
int   wfHead = 0;                       // ring buffer head index (current newest row)
int points=512;
int rows=130;
int FFTRef = -30;
int bandWFFloor[numband]={0};    // waterfall brightness offset per band
#define WFFloor bandWFFloor[band]
int bandSpecStretch[numband];    // spectrum stretch per band
#define specStretch bandSpecStretch[band]
char callSign[12] = "CU2ED      ";  // operator callsign display (max 11 chars + null)
int  callSignLen  = 5;               // number of active chars in callSign     // waterfall brightness offset
int bandAGCAdj[numband]={0};     // AGC level adjust per band
#define AGCAdj bandAGCAdj[band]
int AGCAdjByMode[6] = {0,0,0,0,0,0};  // AGC level memory per mode (USB,LSB,CW,CWN,FM,AM)
float wf_low_smooth = -999.0f; // IIR smoothed noise floor for auto waterfall (-999=uninitialised)
int smNeedsFullRedraw = 0;     // set by P_Meter on last TX frame to force S_Meter full redraw
int pmNeedsReset = 0;          // set on TX start to reset P_Meter statics
int spectrum_rows=80;
unsigned char * palette;
int HzPerBin=94;                        //calculated from FFT width and number of samples. Width=48000 number of samples =512
int bwBarStart=3;
int bwBarEnd=34;
float sMeter;                             //peak reading S meter.
float sMeterPeak;


//UDP server to receive FFT data from GNU RAdio

#define RXPORT 7373
#define TXPORT 7474




int main(int argc, char* argv[])
{  
  initUDP();  
  lastClock=0;
  readConfig();
  // Init bandSpecStretch defaults (5) — overwritten by readConfig if saved
  for(int _i=0;_i<numband;_i++) if(bandSpecStretch[_i]==0) bandSpecStretch[_i]=5;
  setRotation(screenrotate);
  detectHw();
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
  { char adj[8]; sprintf(adj,"Y%d",AGCAdj); sendFifo(adj); }  // restore AGC level
  //  IC-7300 palette — {R,G,B} format, 9 stops = 8 gradients
  //  Levels 0-64: dark (noise floor preserved)
  //  Level 64-96: sharp jump to cyan (signals immediately visible)
  //  Level 96+:   rapid progression through green/yellow/red
  gen_palette((char [][3]){{0,0,0},{0,0,8},{0,5,30},{0,120,200},{0,220,180},{150,220,0},{230,180,0},{240,60,0},{200,0,0}},8);

  
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
    StatusPanel();                                    //status panel — independent of S-Meter, low overhead
    drawCallsignDisplay();

    if(configCounter>0)                                       //save the config after 5 seconds of inactivity.
    {
      configCounter=configCounter-1;                                                                                                  
      if(configCounter==0)
        {
        writeConfig();
        }
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
  float flushbuf;   // local discard buffer (inbuf global replaced by fftRowBuf)
  int ret;
  do
    {
    ret = fread(&flushbuf, sizeof(float), 1, txfftstream);
    }
  while(ret > 0);
  do
    {
    ret = fread(&flushbuf, sizeof(float), 1, fftstream);
    }
  while(ret > 0);
}


// ================================================================

// ── drawRoundBox — rounded rectangle, R=1, single colour ─────────
// R=1: only the exact corner pixel is omitted, giving a minimal
// but clean rounded look with no visible gaps on small boxes.
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

// drawScaleLabel() — draws small text at (x,y) using 5×7 bitmap font
// Cyan (0,180,220) on black. Used for FFT frequency scale labels.
// Characters: A-Z a-z 0-9 space - . / + k
// ================================================================
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

void waterfall()
{
  // ================================================================
  // waterfall() — optimised for RPi 5, IC-7300 visual style
  //
  // Optimisations vs original:
  //  1. Single fread(512 floats) — eliminates ~51200 syscalls/second
  //  2. Ring buffer with head index — zero float copies per frame
  //  3. Spectrum clear: drawLine per row (faster than setPixel loop)
  //
  // Visual improvements (IC-7300 style):
  //  4. Spectrum fill below curve (dim cyan gradient look)
  //  5. Spectrum line: cyan (0,220,255) instead of white
  //  6. dBm grid: subtle reference lines at -10,-20,-30,-40 dBFS
  //  7. BW bar: cyan instead of orange
  //  8. X axis: cyan instead of green
  //  9. Palette: ICOM-style black->blue->cyan->yellow->red
  //
  // DSP / latency: ZERO impact — same 100Hz GUI loop,
  // never touches GNU Radio process or audio path.
  // ================================================================

  int   level, level2;
  int   ret;
  int   baselevel;
  int   bwbaroffset;
  float scaling;
  int   fftref;
  int   centreShift = 0;

  // ── 1. Single fread() — one syscall for full 512-float frame ────
  if(transmitting == 1)
    {
    ret = fread(fftRowBuf, sizeof(float), points, txfftstream);
    fftref = 10;
    }
  else
    {
    ret = fread(fftRowBuf, sizeof(float), points, fftstream);
    fftref = FFTRef;
    }

  if(ret < points) return;

  // ── 2. Ring buffer advance — zero float copies ──────────────────
  wfHead = (wfHead + rows - 1) % rows;

  // ── 3. Store new row, recentring FFT (same logic as original) ───
  for(int p = 0; p < points; p++)
    {
    if(p < points/2)
      buf[p + points/2][wfHead] = fftRowBuf[p];
    else
      buf[p - points/2][wfHead] = fftRowBuf[p];
    }

  // ── BW bar offset (CW TX) ────────────────────────────────────────
  if(((mode == CW) || (mode == CWN)) && (transmitting == 1))
    bwbaroffset = 800 / HzPerBin;
  else
    bwbaroffset = 0;

  // ── CW RX centre shift ───────────────────────────────────────────
  // In CW RX the displayed frequency is offset by 800Hz (sidetone)
  // Both the BW bars AND the centre line must shift together
  if(((mode == CW) || (mode == CWN)) && (transmitting == 0))
    centreShift = 800 / HzPerBin;
  else
    centreShift = 0;

  // ── Update bwBarStart/End BEFORE S-meter peak calculation ────────
  // (original code updated these AFTER S-meter, causing 1-frame lag)
  bwBarStart = rxFilterLow  / HzPerBin;
  bwBarEnd   = rxFilterHigh / HzPerBin;
  if(bwBarStart < -255) bwBarStart = -255;
  if(bwBarEnd   >  255) bwBarEnd   =  255;

  // ── S-meter peak from current row ────────────────────────────────
  // Use centreShift (RX) or bwbaroffset (TX) depending on mode
  int smShift = transmitting ? bwbaroffset : centreShift;
  sMeterPeak = -200;
  for(int p = points/2 + bwBarStart + smShift;
          p < points/2 + bwBarEnd   + smShift; p++)
    {
    if(p >= 0 && p < points)
      if(buf[p][wfHead] > sMeterPeak)
        sMeterPeak = buf[p][wfHead];
    }

  // ── Scaling — spectrum ───────────────────────────────────────────
  // TX needs wider window: audio signals typically -50 to -10dBFS
  // RX uses 45dB window relative to fftref
  if(transmitting)
    baselevel = fftref - 80;   // TX: -70dBFS floor gives full visibility
  else
    baselevel = fftref - 45;   // RX: 45dB window as before
  scaling   = 255.0f / (float)(fftref - baselevel);

  // ── Waterfall scaling — auto in RX, fixed in TX ─────────────────
  // TX: all bins contain signal → percentile gives wrong result → fixed scale
  // RX: 20th percentile + IIR smoothing (piHPSDR method)
  float wf_low, wf_high;
  if(transmitting)
    {
    // TX fixed: wider window to cover typical audio levels (-70 to +10dBFS)
    wf_low  = (float)(fftref - 80) + (float)WFFloor;
    wf_high = (float)fftref;
    }
  else
    {
    // Noise floor estimation — O(n) with outlier rejection
    // Use the 10th percentile approximation via histogram bucketing:
    // Find the value below which 10% of bins fall, in a single pass.
    // This rejects DC spikes and strong signals without the O(n*k) sort.
    // Step 1: find range of current row
    float row_min = buf[0][wfHead], row_max = buf[0][wfHead];
    for(int p = 1; p < points; p++)
      {
      if(buf[p][wfHead] < row_min) row_min = buf[p][wfHead];
      if(buf[p][wfHead] > row_max) row_max = buf[p][wfHead];
      }
    // Step 2: 32-bucket histogram over the range
    float range = row_max - row_min;
    if(range < 1.0f) range = 1.0f;
    int hist[32] = {0};
    for(int p = 0; p < points; p++)
      {
      int bucket = (int)((buf[p][wfHead] - row_min) * 31.0f / range);
      if(bucket < 0)  bucket = 0;
      if(bucket > 31) bucket = 31;
      hist[bucket]++;
      }
    // Step 3: find value at 20th percentile (cumulative sum)
    int target = points / 5;  // 20% of bins
    int cum = 0;
    float noise_floor = row_min;
    for(int b = 0; b < 32; b++)
      {
      cum += hist[b];
      if(cum >= target)
        {
        noise_floor = row_min + (b * range / 31.0f);
        break;
        }
      }
    // Step 4: asymmetric IIR — fast down, slow up
    if(wf_low_smooth < -900.0f) wf_low_smooth = noise_floor;
    else if(noise_floor < wf_low_smooth)
      wf_low_smooth = 0.7f * wf_low_smooth + 0.3f * noise_floor; // fast down
    else
      wf_low_smooth = 0.97f * wf_low_smooth + 0.03f * noise_floor; // slow up
    wf_low  = wf_low_smooth - 5.0f + (float)WFFloor;
    wf_high = wf_low + 55.0f;
    }
  float wf_range = 1.0f / (wf_high - wf_low);

  // ── 4. Draw waterfall — piHPSDR hardcoded palette ────────────────
  // Palette is applied per-pixel via percent 0.0-1.0 within the window.
  // Colour segments (from piHPSDR waterfall.c):
  //   0.000-0.222: noise colour → blue
  //   0.222-0.333: blue → cyan
  //   0.333-0.444: cyan → green
  //   0.444-0.555: green → yellow
  //   0.555-0.777: yellow → red
  //   0.777-0.888: red → purple
  //   0.888-1.000: purple → white
  // colorLow = RGB(0,0,64) dark blue for noise floor
  const int colorLowR=0, colorLowG=0, colorLowB=64;

  for(int r = 0; r < rows-20; r++)
    {
    int ridx = (wfHead + r) % rows;
    for(int p = 0; p < points; p++)
      {
      // ── TX: black background, only show signal inside TX filter BW ──
      if(transmitting == 1)  // no satMode in Hack
        {
        // Check if pixel is within TX filter BW (bwBarStart..bwBarEnd around centre)
        int prel = p - points/2 - bwbaroffset;
        int inBW = (prel >= bwBarStart && prel <= bwBarEnd);
        float v  = buf[p][ridx];
        if(!inBW || v <= wf_low)
          {
          setPixel(p + FFTX, FFTY + 3 + r, 0, 0, 0);  // black outside BW
          continue;
          }
        // Inside BW: show normal palette
        }
      float v = buf[p][ridx];
      int pr, pg, pb;

      if(v <= wf_low)
        {
        pr = colorLowR; pg = colorLowG; pb = colorLowB;
        }
      else if(v >= wf_high)
        {
        pr = 255; pg = 255; pb = 255;
        }
      else
        {
        float pct = (v - wf_low) * wf_range;
        if(pct < 0.222222f)
          {
          float lp = pct * 4.5f;
          pr = (int)((1.0f-lp)*colorLowR);
          pg = (int)((1.0f-lp)*colorLowG);
          pb = (int)(colorLowB + lp*(255-colorLowB));
          }
        else if(pct < 0.333333f)
          {
          float lp = (pct-0.222222f)*9.0f;
          pr = 0; pg = (int)(lp*255); pb = 255;
          }
        else if(pct < 0.444444f)
          {
          float lp = (pct-0.333333f)*9.0f;
          pr = 0; pg = 255; pb = (int)((1.0f-lp)*255);
          }
        else if(pct < 0.555555f)
          {
          float lp = (pct-0.444444f)*9.0f;
          pr = (int)(lp*255); pg = 255; pb = 0;
          }
        else if(pct < 0.777777f)
          {
          float lp = (pct-0.555555f)*4.5f;
          pr = 255; pg = (int)((1.0f-lp)*255); pb = 0;
          }
        else if(pct < 0.888888f)
          {
          float lp = (pct-0.777777f)*9.0f;
          pr = 255; pg = 0; pb = (int)(lp*255);
          }
        else
          {
          float lp = (pct-0.888888f)*9.0f;
          pr = (int)((0.75f+0.25f*(1.0f-lp))*255.0f);
          pg = (int)(lp*255.0f*0.5f);
          pb = 255;
          }
        }
      setPixel(p + FFTX, FFTY + 3 + r, pr, pg, pb);
      }
    }

  // ── Outer borders ────────────────────────────────────────────────
  int wf_bot = FFTY + 3 + rows;   // bottom of waterfall
  drawRoundBox(137, 129, 654, wf_bot+1, 160,160,160);
  drawLine(138, 216, 653, 216, 160,160,160);  // spectrum/waterfall divider

  // ── 5. Clear spectrum area (drawLine per row, faster than setPixel loop)
  for(int r = 0; r < spectrum_rows + 1; r++)
    drawLine(FFTX, FFTY - r, FFTX + points - 1, FFTY - r, 0, 0, 0);

  // ── Pre-compute spectrum levels for current row ──────────────────
  scaling = (float)spectrum_rows / (float)(fftref - baselevel);
          int specLevel[512];
  static float specSmooth[512] = {0};  // IIR smoothed spectrum — persists between frames
  for(int p = 0; p < points; p++)
    {
    float v = buf[p][wfHead];
    if(v < baselevel) v = baselevel;
    if(v > fftref)    v = fftref;
    float raw = (v - baselevel) * scaling;
    // IIR: α=0.35 → ~3 frame average (~30ms) — removes random FFT spikes
    // Asymmetric attack/decay: fast rise, slow fall
    // Attack (rising signal):  α=0.6  → ~2 frames to reach peak
    // Decay  (falling signal): α=0.08 → ~12 frames to decay  (~120ms)
    if(raw > specSmooth[p])
      specSmooth[p] = 0.6f  * raw + 0.4f  * specSmooth[p];  // fast attack
    else
      specSmooth[p] = 0.08f * raw + 0.92f * specSmooth[p];  // slow decay
    int lv = (int)specSmooth[p];
    // specStretch: 0=linear, 5=default(0.65), 10=max(0.30)
    {
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

  // ── 6. Spectrum fill — blue gradient (dark → bright, matches waterfall) ──
  for(int p = 0; p < points; p++)
    {
    int lv = specLevel[p];
    if(lv > 2)
      {
      for(int y = 0; y <= lv; y++)
        {
        int t = y * 255 / (lv > 0 ? lv : 1);
        // Dark blue (0,0,64) at base → bright cyan-blue (0,180,255) at peak
        int R = 0;
        int G = t * 180 / 255;
        int B = 64 + t * 191 / 255; if(B > 255) B = 255;
        drawLine(p+FFTX, FFTY-y, p+FFTX, FFTY-y, R, G, B);
        }
      }
    }



  // ── 8. Spectrum curve — bright cyan line ────────────────────────
  for(int p = 0; p < points - 1; p++)
    {
    drawLine(p + FFTX,     FFTY - specLevel[p],
             p + 1 + FFTX, FFTY - specLevel[p+1],
             180, 180, 180);  // light grey
    }

  // ── BW indicator ─────────────────────────────────────────────────
  {
  int p = points / 2;

  // bwBarStart/End already updated above — do NOT recalculate here

  // In TX CW: bars shift by bwbaroffset (away from centre)
  // In RX CW: bars AND centre line shift together by centreShift
  // In all other modes: no shift
  int barShift = transmitting ? -bwbaroffset : centreShift;

  // BW bar top line + vertical ticks — cyan
  if(bwBarStart > -255)
    drawLine(p+FFTX+bwBarStart+barShift, FFTY-spectrum_rows+5,
             p+FFTX+bwBarStart+barShift, FFTY-spectrum_rows, 0,180,220);
  drawLine(p+FFTX+bwBarStart+barShift, FFTY-spectrum_rows,
           p+FFTX+bwBarEnd  +barShift, FFTY-spectrum_rows, 0,180,220);
  if(bwBarEnd < 255)
    drawLine(p+FFTX+bwBarEnd+barShift, FFTY-spectrum_rows+5,
             p+FFTX+bwBarEnd+barShift, FFTY-spectrum_rows, 0,180,220);

  // Centre line — red, always consistent with barShift
  drawLine(p+FFTX+barShift, FFTY-10,
           p+FFTX+barShift, FFTY-spectrum_rows, 255,0,0);


  }  // ── BW indicator end ─────────────────────────────────────────

  // ── X axis scale — fixed strip below waterfall, redrawn only on BW change
  {
  static int lastScaleBW = -1;
  int p = points/2;
  int ticks[11] = {0,21,43,64,85,107,128,149,171,192,213};
  int scaleY = FFTY + 3 + rows - 20;
  if(lastScaleBW != bandFFTBW[band])
    {
    lastScaleBW = bandFFTBW[band];
    // background
    for(int r=scaleY; r<scaleY+20; r++)
      drawLine(FFTX, r, FFTX+points-1, r, 0,0,0);
    // tick line + ticks
    drawLine(FFTX, scaleY, FFTX+points, scaleY, 0,180,220);
    for(int tick=0; tick<11; tick++)
      {
      drawLine(FFTX+p+ticks[tick], scaleY, FFTX+p+ticks[tick], scaleY+3, 0,180,220);
      drawLine(FFTX+p-ticks[tick], scaleY, FFTX+p-ticks[tick], scaleY+3, 0,180,220);
      }
    int sy = scaleY + 4;
    drawScaleLabel(p+FFTX-6, sy, "0");
    switch(bandFFTBW[band])
      {
      case 0:
        drawScaleLabel(p+FFTX-ticks[5]-24,  sy, "-10k");
        drawScaleLabel(p+FFTX-ticks[10]-24, sy, "-20k");
        drawScaleLabel(p+FFTX+ticks[5]-24,  sy, "+10k");
        drawScaleLabel(p+FFTX+ticks[10]-24, sy, "+20k");
        break;
      case 1:
        drawScaleLabel(p+FFTX-ticks[5]-18,  sy, "-5k");
        drawScaleLabel(p+FFTX-ticks[10]-24, sy, "-10k");
        drawScaleLabel(p+FFTX+ticks[5]-18,  sy, "+5k");
        drawScaleLabel(p+FFTX+ticks[10]-24, sy, "+10k");
        break;
      case 2:
        drawScaleLabel(p+FFTX-ticks[5]-30,  sy, "-2.5k");
        drawScaleLabel(p+FFTX-ticks[10]-18, sy, "-5k");
        drawScaleLabel(p+FFTX+ticks[5]-30,  sy, "+2.5k");
        drawScaleLabel(p+FFTX+ticks[10]-18, sy, "+5k");
        break;
      case 3:
        drawScaleLabel(p+FFTX-ticks[5]-36,  sy, "-1.25k");
        drawScaleLabel(p+FFTX-ticks[10]-30, sy, "-2.5k");
        drawScaleLabel(p+FFTX+ticks[5]-36,  sy, "+1.25k");
        drawScaleLabel(p+FFTX+ticks[10]-30, sy, "+2.5k");
        break;
      }
    }
  }

  // Version string removed — top bar box drawn instead
    // Callsign — drawn once, static position right of top bar

  if(transmitting == 0)
    S_Meter();
  else
    P_Meter();
}


// ── S-Meter geometry defines (file-scope, used by S_Meter and P_Meter) ──────
#define SM_NUM_SEG   20
#define SM_SEG_W     7
#define SM_SEG_GAP   1
#define SM_BAR_H     10
#define SM_TRI_H     6
#define SM_GAP       2
#define SM_TXT_H     14
#define SM_S9_SEG    14
#define SM_MARGIN    3
#define SM_BAR_X     (sMeterX + 4)
#define SM_TRI_Y     (sMeterY + SM_MARGIN)
#define SM_BAR_Y     (SM_TRI_Y + SM_TRI_H + SM_GAP)
#define SM_TXT_Y     (SM_BAR_Y + SM_BAR_H + SM_GAP + SM_TXT_H - 2)

// ── Helper: draw one bargraph segment (lit or dim) ───────────────────────────
// Inline to avoid function-call overhead in the 100Hz loop.
// seg: 0..SM_NUM_SEG-1, lit: 1=aceso 0=apagado
static inline void smDrawSeg(int seg, int lit)
{
  int sx = SM_BAR_X + seg*(SM_SEG_W+SM_SEG_GAP);
  int r, g, b;
  if(seg < SM_S9_SEG)       { r=0;   g=210; b=40;  }   // verde  S0-S9
  else if(seg < SM_S9_SEG+2){ r=230; g=185; b=0;   }   // amarelo +6dB
  else                       { r=240; g=30;  b=0;   }   // vermelho +dB
  if(!lit) { r=r/4; g=g/4; b=b/4; }
  for(int ln=0; ln<SM_BAR_H; ln++)
    drawLine(sx, SM_BAR_Y+ln, sx+SM_SEG_W-1, SM_BAR_Y+ln, r,g,b);
}

// ── Helper: draw peak-hold marker (or erase it) ──────────────────────────────
// visible=1: draws 2px white marker in centre of segment
// visible=0: redraws full segment in dim colour (complete cleanup, no ghosts)
static inline void smDrawPeak(int seg, int visible)
{
  if(visible)
    {
    // 2px white marker centred in segment
    int px = SM_BAR_X + seg*(SM_SEG_W+SM_SEG_GAP) + SM_SEG_W/2 - 1;
    for(int ln=0; ln<SM_BAR_H; ln++)
      drawLine(px, SM_BAR_Y+ln, px+1, SM_BAR_Y+ln, 255,255,255);
    }
  else
    {
    // Redraw full dim segment — eliminates any ghost pixels completely
    smDrawSeg(seg, 0);
    }
}

// ── Helper: draw squelch triangle (or erase previous one) ────────────────────
static inline void smDrawTriangle(int sqx, int visible)
{
  int r=0, g=0, b=0;
  if(visible) { g=200; b=200; }
  drawLine(sqx-3, SM_TRI_Y+0, sqx+3, SM_TRI_Y+0, r, visible?200:0, visible?200:0);
  drawLine(sqx-2, SM_TRI_Y+1, sqx+2, SM_TRI_Y+1, r, visible?210:0, visible?210:0);
  drawLine(sqx-1, SM_TRI_Y+2, sqx+1, SM_TRI_Y+2, r, visible?220:0, visible?220:0);
  drawLine(sqx,   SM_TRI_Y+3, sqx,   SM_TRI_Y+3, r, visible?230:0, visible?230:0);
  drawLine(sqx-2, SM_TRI_Y+4, sqx+2, SM_TRI_Y+4, r, visible?210:0, visible?210:0);
  drawLine(sqx,   SM_TRI_Y+5, sqx,   SM_TRI_Y+5, r, visible?200:0, visible?200:0);
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
  // ── S-Meter — LED bargraph com redesenho diferencial ─────────────
  // Filosofia: só redesenha o que mudou face ao frame anterior.
  // Custo típico em sinal estável: 0 segmentos redesenhados.
  // Custo máximo (sinal a variar): ~4 segs = ~40 drawLine calls.
  // Original tinha ~16 calls/frame fixos. Pior caso aqui: ~50.
  // Médio real: ~8 calls/frame — muito abaixo do original.
  // ─────────────────────────────────────────────────────────────────

  char smStr[20];
  static int   sMeterCount    = 0;
  static int   lastLitSegs    = -1;
  static int   lastSqSeg      = -1;
  static int   lastSquelch    = -1;
  static int   lastGated      = -1;
  static int   lastSValue     = -1;
  static int   lastDbOver     = -1;
  static int   lastSmType     = -1;
  static int   borderDrawn    = 0;

  // ── Ajuste de offset e escala ─────────────────────────────────
  sMeterPeak = sMeterPeak - bandSmeterZero[band];
  int dbOver = 0;
  int sValue = 0;

  if(sMeterPeak < 0) sMeterPeak = 0;

  // Attack / decay
  if(sMeterPeak >= sMeter)
    sMeter = sMeterPeak;
  else
    if(sMeter > 0) sMeter = sMeter - 2;

  // S-value
  if(sMeter < 55) sValue = (int)(sMeter / 6);
  else            { sValue = 9; dbOver = (int)(sMeter - 54); }

  // Calcular posições actuais
  int litSegs = (int)(sMeter * SM_NUM_SEG / 80);
  if(litSegs > SM_NUM_SEG) litSegs = SM_NUM_SEG;

  int sqSegs = (int)(squelch * SM_NUM_SEG / 80);
  if(sqSegs > SM_NUM_SEG-1) sqSegs = SM_NUM_SEG-1;

  int gated = ((sMeter < squelch) && (squelch > 0)) ? 1 : 0;

  // Force full S-meter redraw when requested (TX/RX transition, band change, etc.)
  if(statusForceRedraw || smNeedsFullRedraw)
    {
    lastLitSegs = -1;
    smNeedsFullRedraw = 0;
    for(int r=sMeterY+1; r<sMeterY+sMeterHeight; r++)
      drawLine(sMeterX+1, r, sMeterX+sMeterWidth-1, r, 0,0,0);
    }

  // ── Borda e fundo — só na primeira vez ou força de fora ───────
  if(!borderDrawn || lastLitSegs == -1)
    {
    // caixa
    drawRoundBox(sMeterX, sMeterY, sMeterX+sMeterWidth, sMeterY+sMeterHeight, 160,160,160);
    // limpar interior uma vez
    for(int r=sMeterY+1; r<sMeterY+sMeterHeight; r++)
      drawLine(sMeterX+1, r, sMeterX+sMeterWidth-1, r, 0,0,0);
    // desenhar todos os segmentos no estado inicial
    for(int s=0; s<SM_NUM_SEG; s++)
      smDrawSeg(s, s < litSegs);
    borderDrawn   = 1;
    lastLitSegs   = litSegs;
    lastSquelch   = -1;   // forçar triângulo
    }

  // ── Segmentos — redesenho diferencial ────────────────────────
  if(litSegs != lastLitSegs)
    {
    if(litSegs > lastLitSegs)
      for(int s=lastLitSegs; s<litSegs; s++) smDrawSeg(s, 1);
    else
      for(int s=litSegs; s<lastLitSegs; s++) smDrawSeg(s, 0);
    lastLitSegs = litSegs;
    }

  // Peak hold removido — sem custo de CPU, sem artefactos visuais

  // ── Triângulo squelch — só redesenha se posição/estado mudou ──
  if(sqSegs != lastSqSeg || squelch != lastSquelch)
    {
    // apagar triângulo anterior
    if(lastSqSeg >= 0 && lastSquelch > 0)
      {
      int oldx = SM_BAR_X + lastSqSeg*(SM_SEG_W+SM_SEG_GAP) + SM_SEG_W/2;
      smDrawTriangle(oldx, 0);
      }
    // desenhar novo
    if(squelch > 0)
      {
      int sqx = SM_BAR_X + sqSegs*(SM_SEG_W+SM_SEG_GAP) + SM_SEG_W/2;
      smDrawTriangle(sqx, 1);
      }
    lastSqSeg   = sqSegs;
    lastSquelch = squelch;
    }

  // ── Texto — a cada 6 frames (~60ms), só se valor mudou ────────
  sMeterCount++;
  if(sMeterCount > 5)
    {
    sMeterCount = 0;
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

  // ── Squelch gate — lógica original inalterada ─────────────────
  if(gated)
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

// TempC() removed — functionality merged into StatusPanel() below

void StatusPanel(void)
{
  // ----------------------------------------------------------------
  // StatusPanel — painel de estado do lado esquerdo do ecrã
  //
  // Chamado no loop principal (main) depois de waterfall().
  // NÃO é chamado dentro de S_Meter/P_Meter para não bloquear
  // o caminho crítico do waterfall a 100Hz.
  //
  // Redesenha apenas quando:
  //   a) statusForceRedraw==1  (mudança de setting, banda, modo)
  //   b) um valor em cache mudou
  //   c) contador atingiu STATUS_INTERVAL (temperatura a ~2Hz)
  //
  // Impacto no loop de 10ms: negligenciável — fopen() da temperatura
  // só acontece a cada 500ms, e os campos de texto só são redesenhados
  // quando o valor efectivamente muda.
  // ----------------------------------------------------------------

  #define STATUS_INTERVAL 50        // 50 × 10ms = 500ms (temperatura)

  static int  statusCounter  = 0;
  static int  last_TxAmp     = -1;
  static int  last_TxGain    = -99;
  static int  last_RxAmp     = -1;
  static int  last_RxGain    = -99;
  static int  last_RxBase    = -99;
  static int  last_Mic       = -99;
  static int  last_mode_mic  = -1;
  static long last_TempRaw   = -1;
  static long long last_cpu_total = 0;
  static long long last_cpu_idle  = 0;
  static int  last_cpu_pct   = -1;

  char vStr[16];
  int  tempTick = 0;

  statusCounter++;
  if(statusCounter >= STATUS_INTERVAL)
    {
    statusCounter = 0;
    tempTick = 1;                   // hora de refrescar temperatura
    }

  // sai cedo se nada mudou e não é hora da temperatura
  if(!statusForceRedraw && !tempTick &&
     bandTxAmp[band]  == last_TxAmp  &&
     bandTxGain[band] == last_TxGain &&
     bandRxAmp[band]  == last_RxAmp  &&
     bandRxGain[band] == last_RxGain &&
     bandRxBase[band] == last_RxBase &&
     mode             == last_mode_mic)
    {
    // verifica só o Mic separadamente (depende do modo)
    int curMic = (mode==FM) ? FMMic : (mode==AM) ? AMMic : SSBMic;
    if(curMic == last_Mic) return;
    }

  statusForceRedraw = 0;

  // ── Temperatura (só quando tempTick ou force) ─────────────────
  if(tempTick || last_TempRaw == -1)
    {
    char vtemp[12];
    char *end;
    FILE *tmp = fopen("/sys/class/thermal/thermal_zone0/temp","rt");
    if(tmp != NULL)
      {
      fread(vtemp, 10, 1, tmp);
      fclose(tmp);
      long raw = strtol(vtemp, &end, 10);
      if(raw != last_TempRaw || last_TempRaw == -1)
        {
        last_TempRaw = raw;
        textSize = 1;
        setForeColour(255,255,0);
        gotoXY(5,90);
        displayStr("Temp CPU");
        sprintf(vStr,"=%5.1fC  ", raw/1000.0);
        gotoXY(70,90);
        displayStr(vStr);
        }
      }
    // ── CPU% — /proc/stat delta ───────────────────────────────
    FILE *pstat = fopen("/proc/stat","rt");
    if(pstat != NULL)
      {
      char pline[128];
      fgets(pline, sizeof(pline), pstat);
      fclose(pstat);
      long long u,n,s,id,iow,irq,sirq,st;
      if(sscanf(pline,"cpu %lld %lld %lld %lld %lld %lld %lld %lld",
                &u,&n,&s,&id,&iow,&irq,&sirq,&st) == 8)
        {
        long long total  = u+n+s+id+iow+irq+sirq+st;
        long long idle   = id+iow;
        long long dtotal = total - last_cpu_total;
        long long didle  = idle  - last_cpu_idle;
        int pct = 0;
        if(dtotal > 0) pct = (int)(100LL*(dtotal-didle)/dtotal);
        if(pct < 0) pct=0; if(pct > 100) pct=100;
        last_cpu_total = total;
        last_cpu_idle  = idle;
        if(pct != last_cpu_pct || last_cpu_pct == -1)
          {
          last_cpu_pct = pct;
          textSize=1; setForeColour(255,220,0);
          gotoXY(5,103);  displayStr("CPU%=");
          sprintf(vStr,"%3d%%  ", pct);
          gotoXY(50,103); displayStr(vStr);
          }
        }
      }
    }

  // ── Tx Amp ────────────────────────────────────────────────────
  if(bandTxAmp[band] != last_TxAmp)
    {
    last_TxAmp = bandTxAmp[band];
    textSize = 1; setForeColour(255,255,0);
    gotoXY(5,116);  displayStr("Tx Amp= ");
    sprintf(vStr,"%d  ", last_TxAmp);
    gotoXY(63,116); displayStr(vStr);
    }

  // ── Tx Gain ───────────────────────────────────────────────────
  if(bandTxGain[band] != last_TxGain)
    {
    last_TxGain = bandTxGain[band];
    textSize = 1; setForeColour(255,255,0);
    gotoXY(5,129);  displayStr("Tx Gain= ");
    sprintf(vStr,"%d dB  ", last_TxGain);
    gotoXY(70,129); displayStr(vStr);
    }

  // ── Rx Amp ────────────────────────────────────────────────────
  if(bandRxAmp[band] != last_RxAmp)
    {
    last_RxAmp = bandRxAmp[band];
    textSize = 1; setForeColour(255,255,0);
    gotoXY(5,142);  displayStr("Rx Amp= ");
    sprintf(vStr,"%d  ", last_RxAmp);
    gotoXY(63,142); displayStr(vStr);
    }

  // ── Rx Gain ───────────────────────────────────────────────────
  if(bandRxGain[band] != last_RxGain)
    {
    last_RxGain = bandRxGain[band];
    textSize = 1; setForeColour(255,255,0);
    gotoXY(5,155);  displayStr("Rx Gain= ");
    sprintf(vStr,"%d dB  ", last_RxGain);
    gotoXY(70,155); displayStr(vStr);
    }

  // ── Rx Base ───────────────────────────────────────────────────
  if(bandRxBase[band] != last_RxBase)
    {
    last_RxBase = bandRxBase[band];
    textSize = 1; setForeColour(255,255,0);
    gotoXY(5,168);  displayStr("Rx Base= ");
    sprintf(vStr,"%d dB  ", last_RxBase);
    gotoXY(70,168); displayStr(vStr);
    }

  // ── Mic Gain (depende do modo) ────────────────────────────────
  {
  int curMic = (mode==FM) ? FMMic : (mode==AM) ? AMMic : SSBMic;
  if(curMic != last_Mic || mode != last_mode_mic)
    {
    last_Mic      = curMic;
    last_mode_mic = mode;
    textSize = 1;
    gotoXY(5,181);
    if((mode==USB)||(mode==LSB)||(mode==AM)||(mode==FM))
      {
      setForeColour(255,255,0);
      displayStr("Mic Gain= ");
      sprintf(vStr,"%d  ", curMic);
      gotoXY(80,181);
      displayStr(vStr);
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
    
  
}

void displayError(char*st)
{
  gotoXY(errorX,errorY);
  setForeColour(255,0,0);
  textSize=2;
  displayStr(st);
}


void setHackRxFreq(long long rxfreq)
{
  char freqStr[12];
  sprintf(freqStr,"L%lld",rxfreq);
  sendFifo(freqStr);
}

void setHackTxFreq(long long txfreq)
{
  char freqStr[12];
  sprintf(freqStr,"l%lld",txfreq);
  sendFifo(freqStr);
}

void setHackTxGain(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"a%d",gain);
  sendFifo(gainStr);
  statusForceRedraw=1;
}

void setHackRxGain(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"A%d",gain);
  sendFifo(gainStr);
  statusForceRedraw=1;
}
void setHackTxAmp(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"P%d",gain);
  sendFifo(gainStr);
  statusForceRedraw=1;
}
void setHackRxAmp(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"p%d",gain);
  sendFifo(gainStr);
  statusForceRedraw=1;
}

void setHackRxBase(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"b%d",gain);
  sendFifo(gainStr);
  statusForceRedraw=1;
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



// ================================================================
// IC-7300 style button renderer
// Replaces setForeColour + displayButton throughout the code.
// Uses only drawLine/setPixel/gotoXY/displayStr — same primitives
// already used everywhere, zero extra CPU overhead.
// Called only on state changes, never inside the 100Hz loop.
//
// States:
//   BTN_OFF    — inactivo (fundo escuro, texto cinzento, accent cyan fino)
//   BTN_ON     — activo   (fundo azulado, texto branco, accent cyan espesso)
//   BTN_DANGER — perigo   (PTT TX, fundo castanho-escuro, texto laranja)
//   BTN_WARN   — aviso    (SET, Zero — fundo escuro, texto amarelo)
//   BTN_AMBER  — beacon/cwid activo (fundo azulado, accent âmbar)
// ================================================================

#define BTN_OFF    0
#define BTN_ON     1
#define BTN_DANGER 2
#define BTN_WARN   3
#define BTN_AMBER  4

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

    clearWaterfall();    drawCallsignDisplay();
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
  // Restore AGC and ATK modes from config
  char agcStr[4];
  sprintf(agcStr, "J%d", agcMode);
  sendFifo(agcStr);
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
      {
      displayPopupMode();
      return;
      }
     else if(inputMode==SETTINGS)
      {
      takeSnapshot();
      return;
      }
      else
      {
      setInputMode(FREQ);
      clearPopUp();
      return;
      }
    }
      
if(buttonTouched(funcButtonsX+buttonSpaceX*2,funcButtonsY))  // Button 3 = ATK or DUP or 1750 or NEXT
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
      else
        {
        // Button 3 blank in non-FM FREQ mode
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
    if(inputMode==SETTINGS)
      {
      // ── USB Upgrade — two-touch safety ───────────────────────
      char* script = findUpgradePen();
      if(!script)
        {
        upgradeConfirm = 0;
        drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "UPGRDE", BTN_DANGER);
        displayError(" No USB pen found ");
        sleep(2);
        displayError("                  ");
        drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);
        return;
        }
      if(upgradeConfirm == 0)
        {
        upgradeConfirm = 1;
        drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SURE?", BTN_WARN);
        displayError("  Touch again to upgrade  ");
        return;
        }
      upgradeConfirm = 0;
      doUpgrade(script);
      return;
      }
    else
      {
      takeSnapshot();
      return;
      }
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
  setHackTxAmp(bandTxAmp[band]);
  setHackTxGain(bandTxGain[band]);
  setHackRxGain(bandRxGain[band]);
  setHackRxAmp(bandRxAmp[band]);
  setHackRxBase(bandRxBase[band]);
  setLimessbGeqH(bandssbGeqH[band]);
  setLimessbGeqM2(bandssbGeqM2[band]);
  setLimessbGeqM1(bandssbGeqM1[band]);
  setLimessbGeqL(bandssbGeqL[band]);
  statusForceRedraw=1;                              //force immediate status panel refresh on band change
  wf_low_smooth = -999.0f;                          //reset waterfall auto noise floor on band change
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
  statusForceRedraw=1;
}

void setLimessbGeqH(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"n%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqM2(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"N%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqM1(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"z%d",gain);
  sendFifo(gainStr);
}

void setLimessbGeqL(int gain)
{ 
  char gainStr[10];
  sprintf(gainStr,"Z%d",gain);
  sendFifo(gainStr);
}

void setFMMic(int mic)
{
  char micStr[10];
  sprintf(micStr,"g%d",mic);
  sendFifo(micStr);
  statusForceRedraw=1;
}

void setAMMic(int mic)
{
  char micStr[10];
  sprintf(micStr,"d%d",mic);
  sendFifo(micStr);
  statusForceRedraw=1;
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


// ── Top bar unified box (IC-7300 style) ──────────────────────────
// Single white box x=175..661, y=11..35, with vertical dividers
// Sections (each 81px wide, text centred):
//   Mode:  x=175..255  text_x=197 (3ch)
//   DUP:   x=256..336  text_x=278 (3ch)
//   LOCK:  x=337..417  text_x=353 (4ch)
//   XVTR:  x=418..498  text_x=428 (5ch)
//   MONI:  x=499..579  text_x=515 (4ch)
//   Tx/Rx: x=580..661  text_x=608 (2ch)
#define TB_Y1     9   // box top
#define TB_Y2     37   // box bottom
#define TB_CW     12   // char width at textSize=2

// 15×21px per char, col-major, 3 bytes/col (21 rows = bits 0-20)
// Centred between last freq digit (x=630) and screen edge (x=800)
// Vertically centred in freq display zone (y=65..110)
// Colour: yellow RGB(255,220,0)




// drawTopBox and eraseTopBox kept as no-ops for compatibility
void drawTopBox(int textX, int nchars, int r, int g, int b) { (void)textX; (void)nchars; (void)r; (void)g; (void)b; }
void eraseTopBox(int textX, int nchars) { (void)textX; (void)nchars; }

void setMode(int md)
{
  // ── Save AGCAdj for current mode, restore for new mode ───────────
  if(mode >= 0 && mode < 6) AGCAdjByMode[mode] = AGCAdj;
  AGCAdj = AGCAdjByMode[md];
  { char adj[8]; sprintf(adj,"Y%d",AGCAdj); sendFifo(adj); }

  bandMode[band]=md;
  // Mode text — centred per actual text width in section x=175..255
  {
  int nc = strlen(modename[md]);
  int mx = 175 + (81 - nc*12) / 2;
  // Clear full section first (avoid residue from longer text)
  gotoXY(175, modeY); textSize=2; displayStr("       ");
  gotoXY(mx, modeY);
  setForeColour(255,255,0);
  textSize=2;
  displayStr(modename[md]);
  }
  // DUP text — centred in section x=256..336, text_x=278 (3ch)
  gotoXY(275, modeY);
  if((md==FM)&&(bandDuplex[band]==1))
    {
    setForeColour(255,255,0);
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
        } 
      usleep(TXDELAY);
      setHwTxFreq(freq);
      if((mode==FM)&&(bandDuplex[band]==1))
        {
        displayFreq(freq+bandRepShift[band]);
        displayMenu();
        }
      if (moni==0) setMute(1);                        //mute the receiver
      sMeter=0;
      statusForceRedraw=1;                             //force S-meter full redraw on TX start
      pmNeedsReset=1;                                  //reset P-meter statics on TX start
      setHwRxFreq(freq+10.0);               //offset the Rx frequency to prevent unwanted mixing. (happens even if disabled!) 
      clearWaterfall();
      sendFifo("r6");                       //temporarily increase output sample rate to ensure the output sink doesn't run out of data
      sendFifo("T");
      gotoXY(txX,txY);
      setForeColour(255,0,0);
      textSize=2;
      displayStr("Tx");
      drawTopBox(txX-2, 3, 255,0,0);   // red box for Tx
            transmitting=1;  
      usleep(100000);                        //allow time for the Flowgraph to switch to Tx. 
      setHackTxAmp(bandTxAmp[band]);         //turn on the HackRF Tx amplifier if required. (Going to Rx seems to turn it off internally)
      usleep(TXHACKDELAY);                   //wait 1/2 secondfor the Tx to start up
      sendFifo("r0");                       //reset to the nominal sample rate     
    }
  else if((pt==0)&&(transmitting==1))
    {
      sMeter=0;
      clearWaterfall();
      sendFifo("R");
      setMute(0);                  //unmute the receiver
      wf_low_smooth = -999.0f;    //reset waterfall auto level on RX return
      statusForceRedraw=1;         //force S-meter full redraw on RX return
      setHwTxFreq(freq+10.0);           //offset the Tx freq to prevent unwanted spurious
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
      drawTopBox(txX-2, 3, 0,200,0);   // green box for Rx
            transmitting=0;
      usleep(RXDELAY);
      setTxPin(0);
      setBandBits(bandBitsRx[band]);
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
  
  if (rxfreqhz<1000000) rxfreqhz=100000;         //this is the lowest frequency we can receive
  
  rxoffsethz=(rxfreqhz % 100000)+50000;        //use just the +50Khz to +150Khz positive side of the sampled spectrum. This avoids seeing the DC hump .
  LOrxfreqhz=rxfreqhz-rxoffsethz;

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
      setHackRxFreq(LOrxfreqhz);          
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
  
  setHackTxFreq(txfreqhz);        
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
      //int agcState = (agcMode == 0) ? BTN_WARN : BTN_ON;
      //drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY,
      //                 agcLabels[agcMode], agcState);
      lastAgcMode = agcMode;
      }
    setMoni(0);
    }
 
 configCounter=configDelay;                       //write config after this amount of inactivity    
}


int txvtrMode(void)
{
if((abs(bandRxOffset[band]) >1) & (abs(bandTxOffset[band]) >1 )  )       //if the tx and rx offset are non zero then we are in Transverter mode
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
    eraseTopBox(diallockX, 4);
      }
  else
  {
    dialLock=1;
    gotoXY(diallockX,diallockY);
    textSize=2;
    setForeColour(255,0,0);
    displayStr("LOCK");
    drawTopBox(diallockX, 4, 255,0,0);
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
     drawTopBox(moniX, 4, 0,200,0);
         } 
  else
    {
     if (ptt | ptts) setMute(1);
     moni=0;
     gotoXY(moniX,moniY);
     textSize=2;
     displayStr("    ");
     eraseTopBox(moniX, 4);
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


// ================================================================
// setAGC() — AGC mode control via FIFO command "J<mode>"
// Modes: 0=OFF  1=FAST(5ms/0.5s)  2=MED(15ms/2s)
//        3=SLOW(30ms/5s)  4=LONG(80ms/15s)
// GNU Radio set_AGC_Mode() applies the corresponding attack/decay rates.
// ================================================================

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


// ================================================================
// USB Upgrade functions
//
// How it works:
//   1. findUpgradePen() scans /media/pi and /mnt for a PEN USB
//      containing "langstone_upgrade.sh"
//   2. doUpgrade() stops the SDR, shows status on screen,
//      runs the script as root, then reboots
//
// What to put on the PEN USB (volume name: anything):
//   langstone_upgrade.sh    <- upgrade script (required)
//   LangstoneGUI_Hack       <- new compiled binary (optional)
//   Lang_TRX_Hack.py        <- new GNU Radio flowgraph (optional)
//   version.txt             <- version string shown on screen (optional)
//
// Safety:
//   - Two touches required (1st = confirm, 2nd = execute)
//   - upgradeConfirm resets on any navigation away from SETTINGS
//   - SDR stopped cleanly before upgrade (sendFifo Q)
//   - Config saved before reboot
//   - Old binary backed up to LangstoneGUI_Hack.bak by the script
// ================================================================

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
   if(settingNo==FFT_REF)        // FFT Ref Level
      {
      FFTRef=FFTRef+mouseScroll;
      mouseScroll=0;
      if(FFTRef<-80) FFTRef=-80;
      if(FFTRef>30) FFTRef=30;
      bandFFTRef[band]=FFTRef;
      displaySetting(settingNo);  
      }    
    if(settingNo==TX_GAIN)        // Tx Attenuator
      {
      bandTxGain[band]=bandTxGain[band]+mouseScroll;
      mouseScroll=0;
      if(bandTxGain[band]>47) bandTxGain[band]=47;
      if(bandTxGain[band]<0) bandTxGain[band]=0;
      setHackTxGain(bandTxGain[band]);
      displaySetting(settingNo);  
      }  
    if(settingNo==TX_AMP)        // Tx AMP Setting
      { 
      bandTxAmp[band]=bandTxAmp[band]+mouseScroll;
      mouseScroll=0;
      if(bandTxAmp[band]< 0) bandTxAmp[band]=0;
      if(bandTxAmp[band]> 1) bandTxAmp[band]=1;
      setHackTxAmp(bandTxAmp[band]);
      displaySetting(settingNo);  
      } 
    if(settingNo==RX_GAIN)        // Rx Gain Setting
      { 
      bandRxGain[band]=bandRxGain[band]+mouseScroll;
      mouseScroll=0;
      if(bandRxGain[band]< 0) bandRxGain[band]=0;
      if(bandRxGain[band]> 40) bandRxGain[band]=40;
      setHackRxGain(bandRxGain[band]);
      displaySetting(settingNo);  
      } 
    if(settingNo==RX_AMP)        // Rx AMP Setting
      { 
      bandRxAmp[band]=bandRxAmp[band]+mouseScroll;
      mouseScroll=0;
      if(bandRxAmp[band]< 0) bandRxAmp[band]=0;
      if(bandRxAmp[band]> 1) bandRxAmp[band]=1;
      setHackRxAmp(bandRxAmp[band]);
      displaySetting(settingNo);  
      }         
    if(settingNo==RX_BASE)        // Rx Baseband Setting
      { 
      bandRxBase[band]=bandRxBase[band]+mouseScroll * 2;
      mouseScroll=0;
      if(bandRxBase[band]< 0) bandRxBase[band]=0;
      if(bandRxBase[band]> 62) bandRxBase[band]=62;
      setHackRxBase(bandRxBase[band]);
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

  if(settingNo==WF_FLOOR)     // Waterfall brightness offset
      {
      WFFloor=WFFloor+mouseScroll;
      mouseScroll=0;
      if(WFFloor < -20) WFFloor=-20;
      if(WFFloor >  20) WFFloor= 20;
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
  if(settingNo==AGC_ADJ)      // AGC level adjust (reference dB)
      {
      AGCAdj=AGCAdj+mouseScroll;
      mouseScroll=0;
      if(AGCAdj < -20) AGCAdj=-20;
      if(AGCAdj >  20) AGCAdj= 20;
      char agcAdjStr[8];
      sprintf(agcAdjStr,"Y%d",AGCAdj);
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
  gotoXY(0, settingY-10);
  setForeColour(255,220,0);
  textSize=1;
  char snapMsg[140];
  snprintf(snapMsg, sizeof(snapMsg), "%s  saved", fn);
  displayStr(snapMsg);
  }

  drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "SNAP", BTN_OFF);
}

void showSettingsMenu(void)
  {
    drawButtonIC7300(funcButtonsX,              funcButtonsY, "MENU",   BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX, funcButtonsY, "MODE",   BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*2, funcButtonsY, "NEXT", BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*3, funcButtonsY, "PREV", BTN_OFF);
    drawButtonIC7300(funcButtonsX+buttonSpaceX*4, funcButtonsY, "UPGRDE", BTN_OFF);
    if(portsdownPresent==1)
      {
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "EXIT",   BTN_DANGER);
      }
    else
      {
      drawButtonIC7300(funcButtonsX+buttonSpaceX*5, funcButtonsY, "REBOOT", BTN_DANGER);
      }
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
  if(se==FFT_REF)
  {
  sprintf(valStr,"%d",FFTRef);
  displayStr(valStr);
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
  if(se==TX_GAIN)
  {
  sprintf(valStr,"%d",bandTxGain[band]);
  displayStr(valStr);
  }
  if(se==TX_AMP)
  {
    if(bandTxAmp[band] == 0)
     {
       displayStr("Off");
     }
     else
     {
       displayStr("On (14dB)");
     }
  }
  if(se==RX_GAIN)
  {
  sprintf(valStr,"%d dB",bandRxGain[band]);
  displayStr(valStr);
  }
  if(se==RX_AMP)
  {
    if(bandRxAmp[band] == 0)
     {
       displayStr("Off");
     }
     else
     {
       displayStr("On (14dB)");
     }
  }
  if(se==RX_BASE)
  {
  sprintf(valStr,"%d dB",bandRxBase[band]);
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

conffile=fopen("/home/pi/Langstone/Langstone_Hack.conf","r");

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
    sprintf(vname,"bandTxGain%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandTxGain[b]);
    sprintf(vname,"bandTxAmp%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandTxAmp[b]);
    sprintf(vname,"bandRxGain%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandRxGain[b]); 
    sprintf(vname,"bandRxAmp%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandRxAmp[b]); 
    sprintf(vname,"bandRxBase%02d",b);
    if(strstr(variable,vname)) sscanf(value,"%d",&bandRxBase[b]);   
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
    if(strstr(variable,"agcMode"))     sscanf(value,"%d",&agcMode);
    for(int _b=0;_b<numband;_b++){
      char _k[32]; sprintf(_k,"bandWFFloor%02d",_b);
      if(strstr(variable,_k)) sscanf(value,"%d",&bandWFFloor[_b]);
      sprintf(_k,"bandAGCAdj%02d",_b);
      if(strstr(variable,_k)) sscanf(value,"%d",&bandAGCAdj[_b]);
      sprintf(_k,"bandSpecStretch%02d",_b);
      if(strstr(variable,_k)) { sscanf(value,"%d",&bandSpecStretch[_b]); if(bandSpecStretch[_b]==0 && value[0]=='0') bandSpecStretch[_b]=0; }
    }
    if(strstr(variable,"AGCAdjMode"))   sscanf(value,"%d %d %d %d %d %d",&AGCAdjByMode[0],&AGCAdjByMode[1],&AGCAdjByMode[2],&AGCAdjByMode[3],&AGCAdjByMode[4],&AGCAdjByMode[5]);
    if(strstr(variable,"specStretch"))  sscanf(value,"%d",&specStretch);
    if(strstr(variable,"callSign"))     { strncpy(callSign,value,11); callSign[11]=0; }
    if(strstr(variable,"tuneDigit")) sscanf(value,"%d",&tuneDigit);   
    if(strstr(variable,"mode")) sscanf(value,"%d",&mode);
    if(strstr(variable,"SSBMic")) sscanf(value,"%d",&SSBMic);
    if(strstr(variable,"FMMic")) sscanf(value,"%d",&FMMic);
    if(strstr(variable,"AMMic")) sscanf(value,"%d",&AMMic);
    if(strstr(variable,"volume")) sscanf(value,"%d",&volume);
    if(strstr(variable,"breakInTime")) sscanf(value,"%d",&breakInTime);
    if(strstr(variable,"bands24")) sscanf(value,"%d",&bands24);
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


conffile=fopen("/home/pi/Langstone/Langstone_Hack.conf","w");

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
  fprintf(conffile,"bandTxGain%02d %d\n",b,bandTxGain[b]);
  fprintf(conffile,"bandTxAmp%02d %d\n",b,bandTxAmp[b]);
  fprintf(conffile,"bandRxGain%02d %d\n",b,bandRxGain[b]);
  fprintf(conffile,"bandRxAmp%02d %d\n",b,bandRxAmp[b]);
  fprintf(conffile,"bandRxBase%02d %d\n",b,bandRxBase[b]);
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
fprintf(conffile,"bands24 %d\n",bands24);
fprintf(conffile,"RotateScreen %d\n",screenrotate);
fprintf(conffile,"agcMode %d\n",agcMode);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandWFFloor%02d %d\n",_b,bandWFFloor[_b]);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandAGCAdj%02d %d\n",_b,bandAGCAdj[_b]);
  for(int _b=0;_b<numband;_b++)
    fprintf(conffile,"bandSpecStretch%02d %d\n",_b,bandSpecStretch[_b]);
fprintf(conffile,"AGCAdjMode %d %d %d %d %d %d\n",AGCAdjByMode[0],AGCAdjByMode[1],AGCAdjByMode[2],AGCAdjByMode[3],AGCAdjByMode[4],AGCAdjByMode[5]);
fprintf(conffile,"callSign %s\n",callSign);

fclose(conffile);
return 0;

}


