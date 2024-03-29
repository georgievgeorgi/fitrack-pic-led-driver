#define __16f88
#include<pic14/pic16f88.h>
#include<string.h>
#include<stdlib.h>


typedef unsigned int config;
config __at _CONFIG1 gCONFIG1 =
_LVP_OFF &  // RB3 is digital I/O, HV on MCLR must be used for programming.
_BODEN_OFF  &

//_INTRC_IO &
//_INTRC_CLKOUT &
_FOSC_HS &
//_FOSC_XT &

_MCLR_OFF &
_WDT_OFF  &
_CP_OFF;



#define RB_TRIG  RB0
#define RB_READY_soft RB1
#define RB_READY RB6
//#define RB_RX  RB2
#define RB_BAUD  RB3
#define RB_BAUD_bitfiled _PORTBbits,3
#define RB_WRAP  RB4
//#define RB_TX  RB5
//#define RB_NWRAP RB7
#define RB_GATE  RB7




//                               ┌  Gate output
//                      ┌Xt┐     │  ┌  READY
//     Blinking LED  ┐  │  │     │  │  ┌  TX RS232
//   Blinking LED ┐  │  │  │  +  │  │  │  ┌  Burst WRAP
//                ↑  ↑  ↕  ↕     ↑  ↑  ↑  ↑
//              ┌─┴──┴──┴──┴──┴──┴──┴──┴──┴─┐
//              │ 1  0  7  6  P  7  6  5  4 │
//              │             w             │
//              │    PORTA    S    PORTB    │
//              │             u             │
//              │ 2  3  4  5  p  0  1  2  3 │
//              └─┬──┬──┬──┬──┬──┬──┬──┬──┬─┘
//                ↓  ↓  ↑  ↑     ↑  ↓  ↑  ↓
//  Blinking LED  ┘  │  │  │  -  │  │  │  └  Baud
//     Blinking LED  ┘  │  │     │  │  └  RX RS232
//                 ADC  ┘  │     │  └  READY_soft
//                    N/C  ┘     └  Trigger input



//   run()    timer1=ON, timer2=ON
//   timer1   gate=OFF, READY=ON
//   timer2   timer2=OFF, timer0=ON
//   timer0   toggle led outputs
//            if (n_i>N) timer0=OFF


char uartRXi=0;
char uartTXi=0;
char uartTXlen=0;
char uartRXbuf[33];
char uartTXbuf[33];
char tmpstr[10];
char echo;
char voltmeas_en;
unsigned int tmpint;

unsigned int nPeaks_i;
unsigned int t1postscale_i;

unsigned int nPeaks;
unsigned int t1postscale;

unsigned char portaMask;
unsigned char impOffset;
unsigned char impint;

typedef union{
  unsigned int value;
  struct{
    unsigned int:8;
    unsigned int H:8;
  };
  struct{
    unsigned int L:8;
  };
} ad_result_t;

ad_result_t voltage;

void rs_char(char data){/*{{{*/
  uartTXlen=1;
  TXREG=data;
  TXEN=1;
}/*}}}*/
void rs_send(char* data){/*{{{*/
  //if(!echo)return;
  uartTXi=0;
  uartTXbuf[0]='\n';
  uartTXbuf[1]='\r';
  uartTXlen=2+strlen(data);
  if(uartTXlen>27){
    rs_send("err");
    return;
  }
  strcpy(&(uartTXbuf[2]),data);
  uartTXbuf[uartTXlen++]='\n';
  uartTXbuf[uartTXlen++]='\r';
  uartTXbuf[uartTXlen++]='>';
  uartTXlen++;
  TXEN=1;
  //while(TXEN);
}/*}}}*/
void wait(){//16 nop /*{{{*/
  __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
  __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
  __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
  __asm__("nop"); __asm__("nop"); __asm__("nop"); __asm__("nop");
}/*}}}*/
void voltmeas(){/*{{{*/
  ADON=1;
  GO_NOT_DONE=1;
  while(GO_NOT_DONE);
  voltage.H=ADRESH;
  voltage.L=ADRESL;
  ADON=0;
}/*}}}*/
char run(){/*{{{*/
  if(!RB_READY)return 0;
  if(voltmeas_en){
    voltmeas();
    _itoa(voltage.value,tmpstr,10);
    rs_send(tmpstr);
    //while(TXEN);
  }
  RB_READY=RB_READY_soft=0;
  RB_GATE=1;

  //TMR0IE=0; //disable TMR0

  t1postscale_i=0;
  TMR1H=TMR1L=0;  // clear counters
  TMR1ON=1;

  TMR2=(impOffset==0?PR2-4:0); // if 0ms offset, make tmr2 to finish immediately
  TMR2ON=1;

  return 1;
}/*}}}*/

void main(void){
  // Global configuration/*{{{*/
  INTCON=0;  // GIE=0;

  //OSCCON=0b01101110;       // Fosc 4MHz
  OSCCONbits.IRCF=0b110; // 4MHz
  OSTS=1; // freq is stable
  //OSCCONbits.SCS=0b10; // internal rc
  OSCCONbits.SCS=0b00; // defined by FOSC



  OSCTUNE=0;

  // watchdog
  SWDTEN=0; //disable watchdog
  //WDTCON=0; // presc watchdog 1:32
  //WDTCON=0b1000; // presc watchdog 1:512
  WDTCON=0b1110; // presc watchdog

  //IOFS=0; while(IOFS!=1);  // wait for stable frequency

  TRISA=TRISB=0x0;
  ANSEL=0;
  PORTA=PORTB=0;

  INTCON=PIE1=PIR1=PIE2=PIR2=0;
  PCON=2;

  uartRXi=
    uartTXi=
    uartTXlen=
    uartRXbuf[0]=
    uartTXbuf[0]=
    uartRXbuf[1]=
    uartTXbuf[1]=0;

  nPeaks=10;
  portaMask=0b00001111;//=0b11011111;
  t1postscale=4; // ~1 s
  impOffset=3;
  impint=2;



  // OPTION_REG
  NOT_RBPU=0;
  INTEDG=1; // interrupt on rising edge of RB0
  T0CS=0;
  T0SE=0;
  PSA=0;
  OPTION_REGbits.PS=impint;

  /*}}}*/
  // Configure Timer 0/*{{{*/



  /*}}}*/
  // Configure Timer 1/*{{{*/
  // T1CON
  TMR1ON=0;
  T1RUN=0;
  T1CONbits.T1CKPS=2; // T1 Clock Prescale 1:4
  T1OSCEN=0;
  TMR1CS=0;
  TMR1H=TMR1L=0;  // clear counters
  /*}}}*/
  // Configure Timer 2/*{{{*/
  // T2CON
  TMR2ON=0;
  T2CONbits.TOUTPS=impOffset; // 4 bits
  //T2CONbits.T2CKPS=1; // Prescale 1:4 for ms
  T2CONbits.T2CKPS=3; // Prescale 1:16 N*4ms
  PR2=250; // t2 period
  /*}}}*/
  // Configure RS232/*{{{*/
  TRISB2=TRISB5=1;

  // TXSTA
  TX9=0;
  TXEN=1;
  SYNC=0;
  BRGH=1;
  TRMT=0; // Transmit Shift Register Status Bit 1=TSR empty 0=TSR full
  TX9D=0;


  // RCSTA
  SPEN=1;
  RX9=0;
  CREN=1;
  ADDEN=0;
  FERR=0; // Framing error
  OERR=0;
  RX9D=0;

  // 9k6 bps is not stable
  //SPBRG=25;  //9600
  SPBRG=207; //1200

  // 0xFF nop
  wait();wait(); wait();wait();
  wait();wait(); wait();wait();
  wait();wait(); wait();wait();
  wait();wait(); wait();wait();
  echo=0;

  TXIE=1;
  GIE=1;  //enable global interrupts
  rs_send("\nPic Led Driver\n");
  //while(TXEN);
  GIE=0;
  /*}}}*/
  // Configure A/D/*{{{*/
  TRISA4=1;
  ANS4=1;
  //ADCON0
  ADCON0bits.ADCS=0b01; // Fosc/8
  ADCON0bits.CHS=0b100; // AN4
  GO_NOT_DONE=0;
  ADON=0;

  //ADCON1
  ADFM=1;  // Right justified
  //ADFM=0;  // Left justified
  ADCS2=0;
  ADCON1bits.VCFG=0b00; // Vref+ = Vdd, Vref- = Vss
  //ADCON1bits.VCFG=0b11; // Vref+ = RA3, Vref- = RA2
  //TRISA2=TRISA3=1; //inputs

  voltmeas_en=1;/*}}}*/
  // Configure CVRM/*{{{*/
  TRISA2=1;
  ANS2=1;
  CVROE=1; //CVref is output on the AN2
  CVRR=0; // disables last 8R
  CVRCONbits.CVR=0; // 0-15
  CVREN=1;/*}}}*/






  INTCON=PIE1=PIR1=PIE2=PIR2=0;

  // Trigger input
  TRISB0=1;
  // INTCON
  PEIE=1; // Peripheral Interrupt
  INT0IE=1; // RB0

  // PIE1
  RCIE=1;
  TXIE=1;
  TMR2IE=1;
  TMR1IE=1;

  GIE=1; // Global interrupt enable
  RB_READY_soft=0;
  RB_READY=1;
  while(1);
}

static void interruptf(void) __interrupt 0 {
  // IRQ Timer 0/*{{{*/
  if(TMR0IE&&TMR0IF){
    TMR0IF=0;
    //RB_BAUD=1;
    //PORTA=portaMask;
    //PORTA=0x0;
    //RB_BAUD=0;
    __asm
      ; W = portaMask
      BANKSEL	_portaMask
      MOVF	_portaMask,W
      BANKSEL	_PORTA
      ;RB_BAUD=1
      BSF	RB_BAUD_bitfiled
      ; PORTA = W
      MOVWF	_PORTA
      ;     PORTA=0x0;
    CLRF	_PORTA
      ;     RB_BAUD=0;
    BCF	RB_BAUD_bitfiled
      __endasm;

    if((++nPeaks_i)>=nPeaks){
      TMR0IE=0;
      RB_WRAP=0;
      //RB_NWRAP=1;
    }
  }
  /*}}}*/
  // IRQ Timer 1/*{{{*/
  if(TMR1ON&&TMR1IF){
    TMR1IF=0;
    if((++t1postscale_i)>t1postscale){
      TMR0IE=0; //disable TMR0
      TMR2ON=0;  //should present
      TMR1ON=0;
      RB_WRAP=
        RB_BAUD=
        RB_GATE=0;
      if(echo)rs_send("READY");
      //while(TXEN);
      RB_READY_soft=RB_READY=1;
    }
  }
  /*}}}*/
  // IRQ Timer 2/*{{{*/
  if(TMR2ON&&TMR2IF){
    TMR2IF=0;
    RB_WRAP=1;  // this should be in the if
    TMR2ON=0;
    nPeaks_i=0;
    //RB_NWRAP=0;
    TMR0=0; // impulses in the middle of the wrap
    TMR0IE=1;
  }
  /*}}}*/
  // IRQ AUSART Transmit {{{
  if(TXIF&&TXEN){
    if(uartTXlen>0){
      if(uartTXi<uartTXlen){
        TXREG=uartTXbuf[uartTXi++];
      }else{
        uartTXlen=0;
        uartTXi=0;
      }
    }else{
      TXEN=0;
    }
    return;
  }
  // AUSART Transmit }}}
  // IRQ AUSART Receive {{{
  if(RCIF&&RCIE){
    char tmp;
    tmp=RCREG;
    rs_char(tmp);
    switch (tmp){
      case '': //clear screen
        break;
      case '': case '': //backspace
        uartRXi-=(uartRXi>0?1:0);
        break;
      case '%':case '': //restart
        GIE=0;
        SWDTEN=1; //enable watchdog
        break;
      case '@':case '': //make ready
        uartRXbuf[uartRXi=0]=0;
        rs_send("Get READY");
        //while(TXEN);
        RB_READY=RB_READY_soft=1;
        break;
      case '#':case '': //make busy
        uartRXbuf[uartRXi=0]=0;
        TMR1ON=0;
        RB_READY=RB_READY_soft=0;
        rs_send("Stay BUSY");
        //while(TXEN);
        break;
      case '!':case '	': //SW Trigger
        uartRXbuf[uartRXi=0]=0;
        if(run())
          rs_send("Soft Trig");
        else
          rs_send("BUSY: Ign STrig");
        break;
      case '\r': case '\n':
        if(uartRXi>0){
          uartRXbuf[uartRXi]=0;
          switch (uartRXbuf[0]){
            //case '\r':
            //  rs_send("");
            //  break;

            //case '\n':
            //  rs_send("main nn\r\n");
            //  break;

            case 'e':  //echo
              if(uartRXi>1)echo=(atoi(&(uartRXbuf[1]))!=0);
              _itoa(echo,tmpstr,10);
              rs_send(tmpstr);
              break;

            case 'n':
              if(uartRXi>1)nPeaks=atoi(&(uartRXbuf[1]));
              _itoa(nPeaks,tmpstr,10);
              rs_send(tmpstr);
              break;

            case 'm': // PORTA mask
              if(uartRXi>1)portaMask=atoi(&(uartRXbuf[1]))&0b00001111; // RA5 is allways input, Osc, AN4
              _itoa(portaMask,tmpstr,2);
              rs_send(tmpstr);
              break;

            case 't': //impulse interval
              if(uartRXi>1)impint=(atoi(&(uartRXbuf[1]))&0b111);
              _itoa(64<<impint,tmpstr,10);
              OPTION_REGbits.PS=impint;
              rs_send(tmpstr);
              break;

            case 'g': //Gate
              if(uartRXi>1)t1postscale=atoi(&(uartRXbuf[1]));
              _itoa(t1postscale>>2,tmpstr,10);
              rs_send(tmpstr);
              break;

            case 'V': // Voltage measurement
              if(uartRXi>1)voltmeas_en=(atoi(&(uartRXbuf[1]))!=0);
              _itoa(voltmeas_en,tmpstr,10);
              rs_send(tmpstr);
              break;

            case 'b':
              if(uartRXi>1)CVRCONbits.CVR=(atoi(&(uartRXbuf[1]))&0b1111);
              _itoa(CVRCONbits.CVR,tmpstr,2);
              rs_send(tmpstr);
              break;

            case 'B':
              if(uartRXi>1)CVRR=(atoi(&(uartRXbuf[1]))&0b1);
              _itoa(CVRR,tmpstr,2);
              rs_send(tmpstr);
              break;

            case 'X': //Crystal
              if(uartRXi>1){
                OSCCONbits.SCS=((atoi(&(uartRXbuf[1]))==0)<<1);
                __asm__("nop");__asm__("nop");
              }
              if(OSTS)
                rs_send("Xtal");
              else
                rs_send("IntOSC");
              break;

            case 'o': //offset for the imp
              if(uartRXi>1){
                impOffset=atoi(&(uartRXbuf[1]));
                if(impOffset>16)impOffset=16;
              }
              _itoa(impOffset*4,tmpstr,10);
              T2CON&=0b111;T2CON|=(impOffset>0?impOffset-1:impOffset)*0b1000; // preserves last 3 bits and changes first four
              rs_send(tmpstr);
              break;

            //case 'l':  // for ls
            //  rs_send("not Linux!\n\rh for help");
            //  break;

            case '?': case 'h': //help
              switch (uartRXbuf[1]){
                case 'n':rs_send("num imp [0-32760]"); break;
                case 'm':rs_send("porta mask [0-F]"); break;
                case 't':rs_send("imp int (64*2^#)us [0-7]"); break;
                case 'g':rs_send("gate ~(#/4)s [0-32768]"); break;
                case 'o':rs_send("offs (#*4)ms [0-16]"); break;
                case 'e':rs_send("echo 0/1");break;
                case 'b':rs_send("CVR 0-15");break;
                case 'B':rs_send("CVRR 0-1");break;
                case 'X':rs_send("Xtal 0/1");break;
                case 'V':rs_send("Volt meas 0/1");break;
                case '1':rs_send("!|^I: self trig"); break;
                case '2':rs_send("@|^F: Ready"); break;
                case '3':rs_send("#|^C: busy"); break;
                case '5':rs_send("%|^R: Rest"); break;
                default:
                         rs_send("[h?][nmtgoeXV1235]");
              }
              break;

            default:
              rs_send("N/A cmd");
          }
          uartRXi=0;
        }else{
          rs_send("");
        }
        break;
      default:
        if(uartRXi>10){
          uartRXi=0;
          rs_send("TooLongInst");
        }
        uartRXbuf[uartRXi++]=tmp;
    }
    return;
  }
  //AUSART Receive }}}
  // IRQ External Trigger RB0/*{{{*/
  if(INT0IF){
    INT0IF=0;
    if(RB_TRIG==1){
      if(run())
        if(echo)rs_send("Ext Trig");
        else
          if(echo)rs_send("BUSY: Ign ETrig");
    }
  }
  /*}}}*/
}
