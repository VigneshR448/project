#include <LPC21xx.h>

//--------------------- DEFINES & MACROS ---------------------//
#define SETBIT(WORD,BITPOS)         (WORD |= 1 << BITPOS)
#define CLRBIT(WORD,BITPOS)         (WORD &= ~(1 << BITPOS))
#define CPLBIT(WORD,BITPOS)         (WORD ^= (1 << BITPOS))
#define WRITEBIT(WORD,BITPOS,BIT)   (BIT ? SETBIT(WORD, BITPOS) : CLRBIT(WORD, BITPOS))
#define READBIT(WORD,BITPOS)        ((WORD >> BITPOS) & 1)

typedef unsigned int u32;
typedef signed int s32;
typedef unsigned short int u16;
typedef signed short int s16;
typedef unsigned char u8;
typedef signed char s8;
typedef float f32;
typedef double f64;

//--------------------- DELAY FUNCTIONS ---------------------//
void delay_seconds(int seconds)
{
    T0PR = 15000000 - 1;
    T0TCR = 0x01;
    while (T0TC < seconds);
    T0TCR = 0x03;
    T0TCR = 0x00;
}

void delay_milliseconds(int milliseconds)
{
    T0PR = 15000 - 1;
    T0TCR = 0x01;
    while (T0TC < milliseconds);
    T0TCR = 0x03;
    T0TCR = 0x00;
}

void delay_microseconds(int us)
{
    T0PR = 15 - 1;
    T0TCR = 0x01;
    while (T0TC < us);
    T0TCR = 0x03;
    T0TCR = 0x00;
}

//--------------------- LCD FUNCTIONS ---------------------//
#define LCD_D (0xff << 8)
#define RS (1 << 16)
#define RW (1 << 19)
#define E  (1 << 17)

void LCD_INIT(void);
void LCD_COMMAND(unsigned char);
void LCD_DATA(unsigned char);
void LCD_INTEGER(int);
void LCD_STR(unsigned char*);
void LCD_fp(float);

void LCD_INIT(void)
{
    IODIR0 |= LCD_D | RS | RW | E;
    LCD_COMMAND(0x01);
    LCD_COMMAND(0x02);
    LCD_COMMAND(0x0C);
    LCD_COMMAND(0x38);
    LCD_COMMAND(0x80);
}

void LCD_COMMAND(unsigned char cmd)
{
    IOCLR0 = LCD_D;
    IOSET0 = cmd << 8;
    IOCLR0 = RS;
    IOCLR0 = RW;
    IOSET0 = E;
    delay_milliseconds(2);
    IOCLR0 = E;
}

void LCD_DATA(unsigned char d)
{
    IOCLR0 = LCD_D;
    IOSET0 = d << 8;
    IOSET0 = RS;
    IOCLR0 = RW;
    IOSET0 = E;
    delay_milliseconds(2);
    IOCLR0 = E;
}

void LCD_STR(unsigned char *s)
{
    while (*s) {
        LCD_DATA(*s++);
    }
}

void LCD_INTEGER(int n)
{
    unsigned char arr[5];
    int i = 0;

    if (n == 0)
        LCD_DATA('0');
    else {
        if (n < 0) {
            LCD_DATA('-');
            n = -n;
        }
        while (n != 0) {
            arr[i++] = n % 10;
            n /= 10;
        }
        for (--i; i >= 0; i--)
            LCD_DATA(arr[i] + 48);
    }
}

void LCD_fp(float f)
{
    int temp = (int)f;
    LCD_INTEGER(temp);
    LCD_DATA('.');
    temp = (f - temp) * 1000;
    LCD_INTEGER(temp);
}

//--------------------- UART FUNCTIONS ---------------------//
void UART0_Tx(unsigned char d)
{
    while (!(U0LSR & (1 << 5)));
    U0THR = d;
}

unsigned char UART0_Rx(void)
{
    while (!(U0LSR & (1 << 0)));
    return U0RBR;
}

void UART0_INIT(void)
{
    PINSEL0 |= 0x00000005;
    U0LCR = 0x83;
    U0DLL = 97;
    U0LCR = 0x03;
}

void UART0_INTEGER(int n)
{
    unsigned char arr[5];
    int i = 0;

    if (n == 0)
        UART0_Tx('0');
    else {
        if (n < 0) {
            UART0_Tx('-');
            n = -n;
        }
        while (n != 0) {
            arr[i++] = n % 10;
            n /= 10;
        }
        for (--i; i >= 0; i--)
            UART0_Tx(arr[i] + 48);
    }
}

void UART0_fp(float f)
{
    int temp = (int)f;
    UART0_INTEGER(temp);
    UART0_Tx('.');
    temp = (f - temp) * 1000;
    UART0_INTEGER(temp);
}

//--------------------- SPI FUNCTIONS ---------------------//
#define CS 7
#define FOSC 12000000
#define CCLK (5 * FOSC)
#define PCLK (CCLK / 4)
#define SPCCR_VAL 60
#define SPI_RATE (u8)(PCLK / SPCCR_VAL)

#define Mode_0  0x00
#define Mode_1  0x08
#define Mode_2  0x10
#define Mode_3  0x18
#define MSTR_BIT   5
#define LSBF_BIT   6
#define SPIE_BIT   7
#define SPIF_BIT   7
#define SPIINTF_BIT 0

void Init_SPI0(void)
{
    PINSEL0 |= 0x00001500;
    S0SPCCR = SPI_RATE;
    S0SPCR = (1 << MSTR_BIT | Mode_3);
    SETBIT(IOPIN0, CS);
    SETBIT(IODIR0, CS);
}

u8 SPI0(u8 data)
{
    S0SPDR = data;
    while (!READBIT(S0SPSR, SPIF_BIT));
    return S0SPDR;
}

//--------------------- MCP3204 FUNCTION ---------------------//
f32 Read_ADC_MCP3204(u8 channelNo)
{
    u8 hByte, lByte;
    u32 adcVal = 0;

    CLRBIT(IOPIN0, CS);
    SPI0(0x06);
    hByte = SPI0(channelNo << 6);
    lByte = SPI0(0x00);
    SETBIT(IOPIN0, CS);

    adcVal = ((hByte & 0x0F) << 8) | lByte;
    return ((adcVal * 3.3) / 4096) * 100;
}

//--------------------- TIME DISPLAY FUNCTIONS ---------------------//
void display_time(char sec, char min, char hr)
{
    LCD_COMMAND(0xC0);
    LCD_DATA(48 + (hr / 10));
    LCD_DATA(48 + (hr % 10));
    LCD_DATA(':');
    LCD_DATA(48 + (min / 10));
    LCD_DATA(48 + (min % 10));
    LCD_DATA(':');
    LCD_DATA(48 + (sec / 10));
    LCD_DATA(48 + (sec % 10));
}

void uart_time(char sec, char min, char hr)
{
    UART0_Tx(48 + (hr / 10));
    UART0_Tx(48 + (hr % 10));
    UART0_Tx(':');
    UART0_Tx(48 + (min / 10));
    UART0_Tx(48 + (min % 10));
    UART0_Tx(':');
    UART0_Tx(48 + (sec / 10));
    UART0_Tx(48 + (sec % 10));
    UART0_Tx('\r');
}

//--------------------- MAIN FUNCTION ---------------------//
int main()
{
    f32 temp;
    char sec, min, hr;

    Init_SPI0();
    LCD_INIT();
    UART0_INIT();

    CCR = 0x01;
    SEC = 0;
    MIN = 48;
    HOUR = 10;
    PREINT = 0x01C8;
    PREFRAC = 0x61C0;

    while (1)
    {
        sec = SEC;
        min = MIN;
        hr = HOUR;

        temp = Read_ADC_MCP3204(0);

        LCD_COMMAND(0x80);
        LCD_STR("Temp:");
        LCD_COMMAND(0x87);
        LCD_fp(temp);

        LCD_COMMAND(0xC0);
        display_time(sec, min, hr);

        UART0_fp(temp);

        delay_milliseconds(1000);
    }
}
