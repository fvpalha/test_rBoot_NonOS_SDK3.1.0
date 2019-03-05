/*
 * File	: uart.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ets_sys.h"
#include "osapi.h"
#include "uart.h"
#include "uart_register.h"
#include "os_type.h"
#include "task.h"
#include "gpio16.h"
#include "io.h"

#define UART_INTR_MASK          0x1ff /*https://github.com/fdivitto/ESPWebFramework/blob/master/projects/ESPWebFramework/src/fdvserial.cpp*/

// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;


/*uart demo with a system task, to output what uart receives*/
/*this is a example to process uart data from task,please change the priority to fit your application task if exists*/
/*it might conflict with your task, if so,please arrange the priority of different task,  or combine it to a different event in the same task. */
#define uart_recvTaskPrio        0
#define uart_recvTaskQueueLen    10
os_event_t    uart_recvTaskQueue[uart_recvTaskQueueLen];

#define DBG  
#define DBG1 uart1_sendStr_no_wait
#define DBG2 os_printf


LOCAL void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
    if (uart_no == UART1)
    {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    }
    else
    {
        /* rcv_buff size if 0x100 */
        ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
	#if UART_HW_RTS
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);   //HW FLOW CONTROL RTS PIN
        #endif
	#if UART_HW_CTS
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS);   //HW FLOW CONTROL CTS PIN
        #endif
    }
    uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));//SET BAUDRATE
    
    WRITE_PERI_REG(UART_CONF0(uart_no), ((UartDev.exist_parity & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S) //SET BIT AND PARITY MODE
                                                                        | ((UartDev.parity & UART_PARITY_M)  <<UART_PARITY_S )
                                                                        | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                                                                        | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));

    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);    //RESET FIFO
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    
    if (uart_no == UART0)
    {
        //set rx fifo trigger
        WRITE_PERI_REG(UART_CONF1(uart_no),
        ((0x01 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
        #if UART_HW_RTS
        ((110 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
        UART_RX_FLOW_EN |   //enbale rx flow control
        #endif
        (0x01 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
        UART_RX_TOUT_EN);
		//| ((0x10 & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S));//wjl 
        #if UART_HW_CTS
        SET_PERI_REG_MASK( UART_CONF0(uart_no),UART_TX_FLOW_EN);  //add this sentense to add a tx flow control via MTCK( CTS )
        #endif
        SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_TOUT_INT_ENA |UART_FRM_ERR_INT_ENA);
    }
    else
    {
        WRITE_PERI_REG(UART_CONF1(uart_no),((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));//TrigLvl default val == 1
    }
    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
    //enable rx_interrupt
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_OVF_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
 STATUS ICACHE_FLASH_ATTR uart_tx_one_char(uint8 uart, uint8 TxChar)
{
	while (true) {
		uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
		if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
		{
			break;
		}
	}
	WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
	return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART1, '\r');
        uart_tx_one_char(UART1, '\n');
    } else if (c == '\r') {
    
    } else {
        uart_tx_one_char(UART1, c);
    }
}

//os_printf output to fifo or to the tx buffer
LOCAL void ICACHE_FLASH_ATTR
uart0_write_char_no_wait(char c)
{
    if (c == '\n') {
        uart_tx_one_char_no_wait(UART0, '\r');
        uart_tx_one_char_no_wait(UART0, '\n');
    }
    else {
        uart_tx_one_char_no_wait(UART0, c);
    }
}

/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(uint8 *buf, uint16 len)
{
	uint16 i;

	set_gpio_mode(GPIO_4_PIN, GPIO_OUTPUT, GPIO_PULLDOWN); // INTCOM - OUTPUT MODE
	gpio_write(GPIO_12_PIN, 0); // TX enable
	gpio_write(GPIO_4_PIN, 1); // INTCOM enable

	for (i = 0; i < len; i++)
	{
		uart_tx_one_char(UART0, buf[i]);
	}
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendStr(const char *str)
{
	set_gpio_mode(GPIO_4_PIN, GPIO_OUTPUT, GPIO_PULLDOWN); // INTCOM - OUTPUT MODE
	gpio_write(GPIO_12_PIN, 0); // TX enable
	gpio_write(GPIO_4_PIN, 1); // INTCOM enable

    while(*str){
        uart_tx_one_char(UART0, *str++);
    }
}

/******************************************************************************
 * FunctionName : uart0_send
 * Description  : use uart0 to transfer buffer
 * Parameters   : char *str - point to send buffer
 *
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR uart0_send(char *str) {
	uint8 i = 0;
	while (str[i]) {
        uart_tx_one_char(UART0, str[i]);
		i++;
    }
}

//void at_port_print(const char *str) __attribute__((alias("uart0_sendStr")));
/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
//extern void at_recvTask(void);
LOCAL void ICACHE_FLASH_ATTR
uart0_rx_intr_handler(void *para)
{
    // uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    // uart1 and uart0 respectively

    uint8 uart_no = UART0;//UartDev.buff_uart_no;

    //ATTENTION:
	//IN NON-OS VERSION SDK, DO NOT USE "ICACHE_FLASH_ATTR" FUNCTIONS IN THE WHOLE HANDLER PROCESS
	//ALL THE FUNCTIONS CALLED IN INTERRUPT HANDLER MUST BE DECLARED IN RAM
	//IF NOT , POST AN EVENT AND PROCESS IN SYSTEM TASK
    if (UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_FRM_ERR_INT_ST)) {
        //DBG1("FRM_ERR\r\n");
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
    } else if (UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST)) {
        //DBG("f");
        uart_rx_intr_disable(uart_no);
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_FULL_INT_CLR);
        //system_os_post(uart_recvTaskPrio, 0, 0);
        system_os_post(recvTaskPrio, 0, 0);
    } else if (UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST)) {
        //DBG("t");
    	uart_rx_intr_disable(uart_no);
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_TOUT_INT_CLR);
        //system_os_post(uart_recvTaskPrio, 0, 0);
        system_os_post(recvTaskPrio, 0, 0);
    } else if (UART_TXFIFO_EMPTY_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_TXFIFO_EMPTY_INT_ST)) {
    	//DBG("e");
    	// to output uart data from uart buffer directly in empty interrupt handler
    	//instead of processing in system event, in order not to wait for current task/function to quit
		//ATTENTION:
		//IN NON-OS VERSION SDK, DO NOT USE "ICACHE_FLASH_ATTR" FUNCTIONS IN THE WHOLE HANDLER PROCESS
		//ALL THE FUNCTIONS CALLED IN INTERRUPT HANDLER MUST BE DECLARED IN RAM
		CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_TXFIFO_EMPTY_INT_ENA);
        //system_os_post(uart_recvTaskPrio, 1, 0);
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
    } else if (UART_RXFIFO_OVF_INT_ST  == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_OVF_INT_ST)) {
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_OVF_INT_CLR);
        //DBG1("RX OVF!!\r\n");
    }

    if ((READ_PERI_REG(UART_STATUS(uart_no)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT)
    {
    	// INTCOM em alta - comando foi enviado
    	if (gpio_read(GPIO_4_PIN))
		{
			os_timer_arm(getResetBtntimer(), 62, 1); // reiniciar o timer

			gpio_write(GPIO_4_PIN, 0);  // INTCOM disable - desligado
			gpio_write(GPIO_12_PIN, 1); // TX disable

			set_gpio_mode(GPIO_4_PIN, GPIO_INPUT, GPIO_FLOAT); // INPUT MODE - Para evitar conflito com comunicação por cabo
    	}
    }
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
	// rom use 74880 baut_rate, here reinitialize
	UartDev.baut_rate = uart0_br;
	uart_config(UART0);

	ETS_UART_INTR_ENABLE();

	// install uart1 putc callback
	os_install_putc1((void *)uart1_write_char);

	// TX_ENABLE - GPIO12 - Pin 6 - Pino para habilitar o TX pelo WIFI - evitando conflito com Cabo para download
	set_gpio_mode(GPIO_12_PIN, GPIO_OUTPUT, GPIO_FLOAT);
	gpio_write(GPIO_12_PIN, 1); // TX disable
	// INTCOM - GPIO4 - Pin 2
	set_gpio_mode(GPIO_4_PIN, GPIO_INPUT, GPIO_FLOAT); // INPUT MODE - Para evitar conflito com comunicação por cabo


    /*this is a example to process uart data from task,please change the priority to fit your application task if exists*/
/*    system_os_task(uart_recvTask, uart_recvTaskPrio, uart_recvTaskQueue, uart_recvTaskQueueLen);  //demo with a task to process the uart data
    
    UartDev.baut_rate = uart0_br;
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();*/
    

    /*option 1: use default print, output from uart0 , will wait some time if fifo is full */
    //do nothing...

    /*option 2: output from uart1,uart1 output will not wait , just for output debug info */
    /*os_printf output uart data via uart1(GPIO2)*/
    //os_install_putc1((void *)uart1_write_char);    //use this one to output debug information via uart1 //

    /*option 3: output from uart0 will skip current byte if fifo is full now... */
    /*see uart0_write_char_no_wait:you can output via a buffer or output directly */
    /*os_printf output uart data via uart0 or uart buffer*/
    //os_install_putc1((void *)uart0_write_char_no_wait);  //use this to print via uart0
}

void ICACHE_FLASH_ATTR
uart_reattach()
{
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
}

/******************************************************************************
 * FunctionName : uart_tx_one_char_no_wait
 * Description  : uart tx a single char without waiting for fifo 
 * Parameters   : uint8 uart - uart port
 *                uint8 TxChar - char to tx
 * Returns      : STATUS
*******************************************************************************/
STATUS ICACHE_FLASH_ATTR uart_tx_one_char_no_wait(uint8 uart, uint8 TxChar)
{
    uint8 fifo_cnt = (( READ_PERI_REG(UART_STATUS(uart))>>UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);
    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    }
    return OK;
}

STATUS ICACHE_FLASH_ATTR uart0_tx_one_char_no_wait(uint8 TxChar)
{
	uart_tx_one_char_no_wait(UART0, TxChar);
    /*uint8 fifo_cnt = (( READ_PERI_REG(UART_STATUS(UART0))>>UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);
    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(UART0) , TxChar);
    }*/
    return OK;
}


/******************************************************************************
 * FunctionName : uart1_sendStr_no_wait
 * Description  : uart tx a string without waiting for every char, used for print debug info which can be lost
 * Parameters   : const char *str - string to be sent
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR uart1_sendStr_no_wait(const char *str)
{
    while(*str) {
        uart_tx_one_char_no_wait(UART1, *str++);
    }
}

void ICACHE_FLASH_ATTR uart_rx_intr_disable(uint8 uart_no)
{
#if 0
    CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
#else
    ETS_UART_INTR_DISABLE();
#endif
}

void ICACHE_FLASH_ATTR uart_rx_intr_enable(uint8 uart_no)
{
#if 0
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
#else
    ETS_UART_INTR_ENABLE();
#endif
}


//========================================================
LOCAL void ICACHE_FLASH_ATTR
uart0_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART0, '\r');
        uart_tx_one_char(UART0, '\n');
    } else if (c == '\r') {
    } else {
        uart_tx_one_char(UART0, c);
    }
}

void ICACHE_FLASH_ATTR
UART_SetWordLength(uint8 uart_no, UartBitsNum4Char len) 
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_BIT_NUM,len,UART_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetStopBits(uint8 uart_no, UartStopBitsNum bit_num) 
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_STOP_BIT_NUM,bit_num,UART_STOP_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetLineInverse(uint8 uart_no, UART_LineLevelInverse inverse_mask) 
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LINE_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(uart_no), inverse_mask);
}

void ICACHE_FLASH_ATTR
UART_SetParity(uint8 uart_no, UartParityMode Parity_mode) 
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_PARITY |UART_PARITY_EN);
    if(Parity_mode==NONE_BITS){
    }else{
        SET_PERI_REG_MASK(UART_CONF0(uart_no), Parity_mode|UART_PARITY_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_SetBaudrate(uint8 uart_no,uint32 baud_rate)
{
    uart_div_modify(uart_no, UART_CLK_FREQ /baud_rate);
}

void ICACHE_FLASH_ATTR
UART_SetFlowCtrl(uint8 uart_no,UART_HwFlowCtrl flow_ctrl,uint8 rx_thresh)
{
    if(flow_ctrl&USART_HardwareFlowControl_RTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        SET_PERI_REG_BITS(UART_CONF1(uart_no),UART_RX_FLOW_THRHD,rx_thresh,UART_RX_FLOW_THRHD_S);
        SET_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }
    if(flow_ctrl&USART_HardwareFlowControl_CTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
        SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_WaitTxFifoEmpty(uint8 uart_no , uint32 time_out_us) //do not use if tx flow control enabled
{
	uint8 tx_fifo_len;
    uint32 t_s = system_get_time();
    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S)) {
		
        if(( system_get_time() - t_s )> time_out_us) {
            break;
        }
        WRITE_PERI_REG(0X60000914, 0X73);//WTD
    }
}

bool ICACHE_FLASH_ATTR
UART_CheckOutputFinished(uint8 uart_no, uint32 time_out_us)
{
    uint32 t_start = system_get_time();
    uint8 tx_fifo_len;
    uint32 tx_buff_len;
    while(1){
        tx_fifo_len =( (READ_PERI_REG(UART_STATUS(uart_no))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT);
        if( tx_fifo_len==0 && tx_buff_len==0){
            return TRUE;
        }
        if( system_get_time() - t_start > time_out_us){
            return FALSE;
        }
        WRITE_PERI_REG(0X60000914, 0X73);//WTD
    }    
}

void ICACHE_FLASH_ATTR
UART_ResetFifo(uint8 uart_no)
{
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
}

void ICACHE_FLASH_ATTR
UART_ClearIntrStatus(uint8 uart_no,uint32 clr_mask)
{
    WRITE_PERI_REG(UART_INT_CLR(uart_no), clr_mask);
}

void ICACHE_FLASH_ATTR
UART_SetIntrEna(uint8 uart_no,uint32 ena_mask)
{
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), ena_mask);
}


void ICACHE_FLASH_ATTR
UART_SetPrintPort(uint8 uart_no)
{
    if(uart_no==1){
        os_install_putc1(uart1_write_char);
    }else{
        /*option 1: do not wait if uart fifo is full,drop current character*/
        os_install_putc1(uart0_write_char_no_wait);
	/*option 2: wait for a while if uart fifo is full*/
	os_install_putc1(uart0_write_char);
    }
}


//========================================================


/*test code*/
void ICACHE_FLASH_ATTR
uart_init_2(UartBautRate uart0_br, UartBautRate uart1_br)
{
    // rom use 74880 baut_rate, here reinitialize
    UartDev.baut_rate = uart0_br;
    UartDev.exist_parity = STICK_PARITY_EN;
    UartDev.parity = EVEN_BITS;
    UartDev.stop_bits = ONE_STOP_BIT;
    UartDev.data_bits = EIGHT_BITS;
	
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();

    // install uart1 putc callback
    os_install_putc1((void *)uart1_write_char);//print output at UART1
}
