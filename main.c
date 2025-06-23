//------------------------------------------- main.c CODE STARTS --------------------------------------------------------------------------- 
#include <stdio.h>
#include "NUC100Series.h"

#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2

//Function Declaring
void System_Config(void);
void SPI2_Config(void);
void ADC7_Config(void);
void SPI2_send(unsigned char temp); 
void ADC_IRQHandler(void);
void led5_setup(void);
void led5(void);
void Data_send(void);

volatile uint32_t adc7_val;
volatile char adc7_val_s[4] = "0000";
      
int main(void)
{
  //External functions
  System_Config();
  SPI2_Config();
  ADC7_Config();
  led5_setup();

  ADC->ADCR |= (1 << 11); // start ADC channel 7 conversion

  while (1) 
		{	
			while (!(ADC->ADSR & (1 << 0))); // wait until conversion is completed (ADF = 1)
      ADC->ADSR |= (1 << 0); // write 1 to clear ADF
      //Print ADC value on the LCD
      adc7_val = ADC->ADDR[7] & 0x0000FFFF;     
            
      //External Function
      Data_send();
      led5();
    }
}
//Function used to call for sending data through SPI2
//Sending "2023" via SPI2
void SPI2_send(unsigned char temp)  
{
      SPI2->SSR |= 1 << 0;
      SPI2->TX[0] = temp;
      SPI2->CNTRL |= 1 << 0;
      while(SPI2->CNTRL & (1 << 0));
      SPI2->SSR &= ~(1 << 0);
}

void System_Config(void) 
{
  SYS_UnlockReg(); // Unlock protected registers
    
  CLK->PWRCON |= (1 << 0);
  while(!(CLK->CLKSTATUS & HXT_STATUS));  
  //PLL configuration starts
  CLK->PLLCON &= ~(1 << 19);                            //0: PLL input is HXT
  CLK->PLLCON &= ~(1 << 16);                          //PLL in normal mode
  CLK->PLLCON &= (~(0x01FF << 0));
  CLK->PLLCON |= 48;                                                    // PLL 50MHz
  CLK->PLLCON &= ~(1 << 18);                          //0: enable PLLOUT
  while(!(CLK->CLKSTATUS & PLL_STATUS));
  //PLL configuration ends    
  
	//clock source selection
  CLK->CLKSEL0 &= (~(0x07 << 0));
  CLK->CLKSEL0 |= (0x02 << 0); // HCLK = PLL 50MHz
  
  //clock frequency division
  CLK->CLKDIV &= (~0x0F << 0);
    
  // SPI3 clock enable
  CLK->APBCLK |= 1 << 15;
    
  //Enable clock of SPI2
  CLK->APBCLK |= 1 << 14;
      
  //ADC Clock selection and configuration
  CLK->CLKSEL1 &= ~(0x03 << 2); // ADC clock source is 12 MHz
  CLK->CLKDIV &= ~(0x0FF << 16);
  CLK->CLKDIV |= (0x0B << 16);  // ADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
  CLK->APBCLK |= (0x01 << 28);  // enable ADC clock
    
  SYS_LockReg();  // Lock protected registers    
}

void SPI2_Config(void)
{
  SYS->GPD_MFP |= 1 << 3;           //1: PD3 is configured for SPI2 - SPI2_MOSI
  SYS->GPD_MFP |= 1 << 1;           //1: PD1 is configured for SPI2 - SPI_CLK
  SYS->GPD_MFP |= 1 << 0;           //1: PD0 is configured for SPI2 - SPI2_SS

  SPI2->CNTRL &= ~(1 << 23);  //0: disable variable clock feature
  SPI2->CNTRL &= ~(1 << 22);  //0: disable two bits transfer mode
  SPI2->CNTRL &= ~(1 << 18);  //0: select Master mode
  SPI2->CNTRL &= ~(1 << 17);  //0: disable SPI interrupt    
  SPI2->CNTRL |= 1 << 11;     //1: SPI clock idle high 
  SPI2->CNTRL |= (1 << 10);   //1: LSB is sent first   
  SPI2->CNTRL &= ~(3 << 8);   //00: one transmit/receive word will be executed in one data transfer
  SPI2->CNTRL &= ~(31 << 3);  //Transmit/Receive bit length
  SPI2->CNTRL |= 8 << 3;      //8 bits transmitted/received per data transfer
  SPI2->CNTRL &= ~(1 << 2);   //1: Transmit at rising edge of SPI CLK       
  SPI2->DIVIDER = 24;         // SPI clock divider. SPI clock = HCLK/((24+1)*2)=1MHz. HCLK = 50 MHz
}

void ADC7_Config(void) 
{
  PA->PMD &= ~(0b11 << 14);
  PA->PMD |= (0b01 << 14);    // PA.7 is input pin
  PA->OFFD |= (0x01 << 7);    // PA.7 digital input path is disabled
  SYS->GPA_MFP |= (1 << 7);   // GPA_MFP[7] = 1 for ADC7
  SYS->ALT_MFP &= ~(1 << 11); // ALT_MFP[11] = 0 for ADC7

  //ADC operation configuration
  ADC->ADCR |= (0b11 << 2);   // continuous scan mode
  ADC->ADCR |= (1 << 1);      // ADC interrupt is enabled
  ADC->ADCR |= (0x01 << 0);   // ADC is enabled
  ADC->ADCHER &= ~(0b11 << 8);// ADC7 input source is external pin
  ADC->ADCHER |= (1 << 7);    // ADC channel 7 is enabled.
      
  //NVIC interrupt configuration for ADC in terrupt source
  NVIC->ISER[0] |= 1ul<<29;
  NVIC->IP[7] &= (~(3ul<<14));
}

// Interrupt Service Routine of ADC
void ADC_IRQHandler(void)
{
	ADC->ADSR |= (0b01 << 0); // write 1 to clear ADF when conversion is done and ISR is called
}
void led5_setup(void)
{
	//LED display via GPIO-C12 to indicate main program execution 
  PC->PMD &= (~(0x03 << 24)); 
  PC->PMD |= (0x01 << 24); 
}

void led5(void)   // Turn on LED5 when SPI2 sending letter
{
	if (adc7_val >= 2481) // When larger than 2V
		{
			PC->DOUT &= ~(1<<12); //LED5 on
    } 
		else // if the ADC is below 2481
    {
      PC->DOUT |= (1<<12); // LED5 off
    }
}

void Data_send(void) //Sending Character via SPI2
{
	if(adc7_val > 2481) // 2V / 0.806 = 2481
		{     
			// Transfer 2023
			SPI2_send('2');
			SPI2_send('0');
			SPI2_send('2');
			SPI2_send('3');
    }              
}
//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
