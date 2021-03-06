/****************************************Copyright (c)****************************************************
**                                        
**                                 
**
**--------------File Info---------------------------------------------------------------------------------
** File name:			main.c
** Last modified  Date:          
** Last Version:	1.0
** Descriptions:		
**						
**--------------------------------------------------------------------------------------------------------
** Created by:			FiYu
** Created date:		2014-8-5
** Version:			    1.0
** Descriptions:		USB HID 无线收发数据实验程序�
**	                通过USB HID配置nRF24LU1P的无线参数:无线信道和接收数据长度
**                  接收发射端的无线信息，通过USB HID上传
**                  通过USB HID将信息发送给nRF24LU1P
**                  
**--------------------------------------------------------------------------------------------------------
** Modified by:			
** Modified date:		
** Version:				
** Descriptions:		
**
** Rechecked by:			
**********************************************************************************************************/
#include "nrf24lu1p.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hal_nrf.h"
#include "hal_usb.h"
#include "hal_usb_hid.h"
#include "usb_map.h"
#include "hal_flash.h"
#include "hal_delay.h"


/*-----------------------------------------------------------------------------
** 内部宏定义
-----------------------------------------------------------------------------*/
#define USB_IN_CMD usb_in_buf[0]
#define USB_OUT_CMD usb_out_buf[0]
#define USB_CONFIGRF_CH usb_out_buf[1]
#define USB_CONFIGRF_LEN usb_out_buf[2]
#define ERROR_CODE usb_in_buf[1]

#define RF_OUT_CMD rf_out_buf[0]
#define RF_IN_CMD rf_in_buf[0]

#define SEND_USB() app_send_usb_in_data(0, 0)
#define FORWARD_RF_TO_USB() app_send_usb_in_data(rf_in_buf, 32)


/*-----------------------------------------------------------------------------
** USB相关变量定义
-----------------------------------------------------------------------------*/
static xdata uint8_t usb_in_buf[EP1_2_PACKET_SIZE];
static xdata uint8_t usb_out_buf[EP1_2_PACKET_SIZE];
static bool xdata app_usb_out_data_ready = false;
extern code const usb_string_desc_templ_t g_usb_string_desc;
static bool xdata app_pending_usb_write = false;

/*-----------------------------------------------------------------------------
** RF相关变量定义
-----------------------------------------------------------------------------*/
static uint8_t xdata rf_in_buf[32];
static uint8_t xdata rf_out_buf[32];
static bool xdata packet_received = false;
static bool xdata radio_busy = false;
static bool xdata transmitted = false;
uint8_t cmd = 0;



//-----------------------------------------------------------------------------
// Internal function prototypes
//-----------------------------------------------------------------------------
static void app_send_usb_in_data(uint8_t * buf, uint8_t size);
static void app_parse_usb_out_packet();
static void app_wait_while_usb_pending();
static void rf_config(void);

/*-----------------------------------------------------------------------------
** USB回调函数声明
-----------------------------------------------------------------------------*/
hal_usb_dev_req_resp_t device_req_cb(hal_usb_device_req* req, uint8_t** data_ptr, uint8_t* size) large reentrant;
void suspend_cb(uint8_t allow_remote_wu) large reentrant;
void resume_cb() large reentrant;
void reset_cb() large reentrant;
uint8_t ep_1_in_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant;
uint8_t ep_2_out_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant;

/*******************************************************************************************************
 * 描  述 : MAIN函数
 * 入  参 : none
 * 返回值 : none
 *******************************************************************************************************/
void main()
{
  // USB HAL initialization
  hal_usb_init(true, device_req_cb, reset_cb, resume_cb, suspend_cb);   
  hal_usb_endpoint_config(0x81, EP1_2_PACKET_SIZE, ep_1_in_cb);  // Configure 32 byte IN endpoint 1
  hal_usb_endpoint_config(0x02, EP1_2_PACKET_SIZE, ep_2_out_cb); // Configure 32 byte OUT endpoint 2

  
  rf_config();    // 无线配置

  P0DIR = 0xEF;	  // 配置P0:P04配置为输出
  LED = 1;        // 熄灭指示灯

  EA = 1;         // 使能全局中断

  while(true)                                                                               
  {    
    if(hal_usb_get_state() == CONFIGURED)  // USB已配置?
    { 
      if(app_usb_out_data_ready)// USB 接收到主机发送的数据
      {
        app_parse_usb_out_packet();
        app_usb_out_data_ready = false;
      }
      if(packet_received == true) // 接收到无线数据?
			{
				app_send_usb_in_data(rf_in_buf, 31);
				packet_received = false;
				LED = ~LED;
			}
		}
		if(cmd == CMD_CONFIG_RF)
		{
      CE_LOW(); // Enable receiver 		
		  hal_nrf_set_rf_channel(USB_CONFIGRF_CH);
		  hal_nrf_set_rx_payload_width((int)HAL_NRF_PIPE0, USB_CONFIGRF_LEN);// 接收模式下需要设置数据长度
		  CE_HIGH(); // Enable receiver 
			cmd = CMD_IDLE;
			
		}
  }
}

//-----------------------------------------------------------------------------
// Handle commands from Host application
//-----------------------------------------------------------------------------
static void app_parse_usb_out_packet()
{
  switch(USB_OUT_CMD)
  {
    case CMD_CONFIG_RF:
      
		  cmd = CMD_CONFIG_RF;
      break;

    case CMD_SENDTODEV:
      cmd = CMD_SENDTODEV;
      break;
    

    default:
      break;
  }
}

//-----------------------------------------------------------------------------
// RF helper functions
//-----------------------------------------------------------------------------

// Initialize radio module
static void rf_config(void)
{
  // Enable radio SPI and clock
  RFCTL = 0x10;
  RFCKEN = 1;
	RF = 1;
	
	hal_nrf_set_rf_channel(RF_CHANNEL);
	hal_nrf_set_datarate(HAL_NRF_250KBPS);	  // RF速率：250KBPS
	hal_nrf_set_crc_mode(HAL_NRF_CRC_16BIT);  // CRC：16bits	
	hal_nrf_open_pipe(HAL_NRF_PIPE0,false);	  // Open radio pipe(s) and enable/ disable auto acknowledge. 
  hal_nrf_set_rx_payload_width((int)HAL_NRF_PIPE0, RX_PAYLOAD_LEN);// 接收模式下需要设置数据长度

  // Clear and flush radio state
  hal_nrf_get_clear_irq_flags();
  hal_nrf_flush_rx();
  hal_nrf_flush_tx();

  packet_received = false;
   	
	hal_nrf_set_operation_mode(HAL_NRF_PRX);
	hal_nrf_set_power_mode(HAL_NRF_PWR_UP);// Power up radio
	CE_HIGH(); // Enable receiver 
}


// Interrupt handler for RF module
/*******************************************************************************************************
 * 描  述 : RF中断服务函数
 * 入  参 : none
 * 返回值 : none
 *******************************************************************************************************/
NRF_ISR()
{
  uint8_t irq_flags;
  // Read and clear IRQ flags from radio
  irq_flags = hal_nrf_get_clear_irq_flags();

  switch (irq_flags) 
  {
    // Transmission success.
    case (1 << (uint8_t)HAL_NRF_TX_DS):
      radio_busy = false;
      transmitted = true;
      break;

    // Transmission failed (maximum re-transmits)
    case (1 << (uint8_t)HAL_NRF_MAX_RT):
      hal_nrf_flush_tx();
      radio_busy = false;
      transmitted = false;
      break;

    // Data received 
    case (1 << (uint8_t)HAL_NRF_RX_DR):
      // Read payload
      while (!hal_nrf_rx_fifo_empty()) 
			{ 
        hal_nrf_read_rx_payload(rf_in_buf);
      }
      packet_received = true;
      break;

  }
}

//-----------------------------------------------------------------------------
// USB Helper functions
//-----------------------------------------------------------------------------  

/*******************************************************************************************************
 * 描  述 : USB向主机发送数据
 * 入  参 : buf：发送缓存首地址
 *          size：发送数据长度
 * 返回值 : none
 *******************************************************************************************************/  
static void app_send_usb_in_data(uint8_t * buf, uint8_t size)
{
  app_wait_while_usb_pending();
  app_pending_usb_write = true;  
  memcpy(usb_in_buf, buf, size);
  hal_usb_send_data(1, usb_in_buf, EP1_2_PACKET_SIZE);
}


static void app_wait_while_usb_pending()
{    
  uint16_t timeout = 50000;   // Will equal ~ 50-100 ms timeout 
  while(timeout--)
  {
    if(!app_pending_usb_write)
    {
      break;
    }
  }    
}

//-----------------------------------------------------------------------------
// USB Callbacks
//-----------------------------------------------------------------------------  

hal_usb_dev_req_resp_t device_req_cb(hal_usb_device_req* req, uint8_t** data_ptr, uint8_t* size) large reentrant
{
  hal_usb_dev_req_resp_t retval;

  if( hal_usb_hid_device_req_proc(req, data_ptr, size, &retval) == true ) 
  {
    // The request was processed with the result stored in the retval variable
    return retval;
  }
  else
  {
    // The request was *not* processed by the HID subsystem
    return STALL;   
  }
}

void suspend_cb(uint8_t allow_remote_wu) large reentrant
{
  USBSLP = 1; // Disable USB clock (auto clear)
  allow_remote_wu = 0;  
}

void resume_cb(void) large reentrant
{
}

void reset_cb(void) large reentrant
{
}

//-----------------------------------------------------------------------------
// USB Endpoint Callbacks
//-----------------------------------------------------------------------------  
uint8_t ep_1_in_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant
{  
  app_pending_usb_write = false;
  return 0x60; // NAK
  adr_ptr = adr_ptr;
  size = size;
}

uint8_t ep_2_out_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant
{
  memcpy(usb_out_buf, adr_ptr, *size);
  app_usb_out_data_ready = true;
  //P0 = *size;
  return 0xff; // ACK
}

/********************************************END FILE*****************************************************/
