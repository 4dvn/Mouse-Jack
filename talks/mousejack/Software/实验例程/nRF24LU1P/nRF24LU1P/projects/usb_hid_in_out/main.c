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
** Descriptions:		USB HID 收发数据实验程序：NRF24LU1P接收到USB数据后，将USB数据返回。
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
#include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hal_usb.h"
#include "hal_usb_hid.h"
#include "usb_map.h"
#include "hal_delay.h"


/*-------------------管脚定义--------------------------------------------------*/
#define  LED    P04  // 开发板上的指示灯
#define  SW     P05  // 开发板上的按键


/* 本例程中nRF24LU1P管脚配置
P00: sck， 编程接口，也可以配置为其他功能。
P01: mosi，编程接口，也可以配置为其他功能。
P02: miso，编程接口，也可以配置为其他功能。
P03: csn， 编程接口，也可以配置为其他功能。

P04：输出，驱动LED	          
P05：输入，按键检测
*/

/*-----------------------------------------------------------------------------
** 变量定义
-----------------------------------------------------------------------------*/
static xdata uint8_t usb_in_buf[EP1_2_PACKET_SIZE];
static xdata uint8_t usb_out_buf[EP1_2_PACKET_SIZE];
static bool xdata app_usb_out_data_ready = false;
static bool xdata app_pending_usb_write = false;

/*-----------------------------------------------------------------------------
** USB回调函数声明
-----------------------------------------------------------------------------*/
hal_usb_dev_req_resp_t device_req_cb(hal_usb_device_req* req, uint8_t** data_ptr, uint8_t* size) large reentrant;
void resume_cb() large reentrant;
void reset_cb() large reentrant;
void suspend_cb(uint8_t allow_remote_wu) large reentrant;
uint8_t ep_1_in_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant;
uint8_t ep_2_out_cb(uint8_t *adr_ptr, uint8_t* size) large reentrant;
static void app_send_usb_in_data(uint8_t * buf, uint8_t size);
static void app_wait_while_usb_pending();

/*******************************************************************************************************
 * 描  述 : MAIN函数
 * 入  参 : none
 * 返回值 : none
 *******************************************************************************************************/
void main(void)
{ 
  P0DIR = 0xEF;	      //配置P0:P04配置为输出
	
  // USB初始化
  hal_usb_init(true, device_req_cb, reset_cb, resume_cb, suspend_cb);   
  hal_usb_endpoint_config(0x81, EP1_2_PACKET_SIZE, ep_1_in_cb);  // Configure 32 byte IN endpoint 1
  hal_usb_endpoint_config(0x02, EP1_2_PACKET_SIZE, ep_2_out_cb); // Configure 32 byte OUT endpoint 2

  EA = 1;         // 使能全局中断
  P0DIR = 0xEF;	  // 配置P0:P04配置为输出
  LED = 1;        // 熄灭指示灯
	
	
	while(1)
  {		
		if(hal_usb_get_state() == CONFIGURED)  // USB已配置?
    { 
      if(app_usb_out_data_ready)// USB 接收到主机发送的数据
      {
				LED = 0;   // 点亮指示灯
				app_send_usb_in_data(usb_out_buf, EP1_2_PACKET_SIZE);  // 返回接收数据
        app_usb_out_data_ready = false;
				LED = 1;   // 熄灭指示灯
      }
    }
  }
}

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
void suspend_cb(uint8_t allow_remote_wu) large reentrant
{
  USBSLP = 1; // Disable USB clock (auto clear)
  allow_remote_wu = 0;  
}

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
  return 0xff; 
}
 /*********************************END FILE****************************************************************/	