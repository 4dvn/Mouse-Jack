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
** Descriptions:		USB HID �շ�����ʵ�����NRF24LU1P���յ�USB���ݺ󣬽�USB���ݷ��ء�
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


/*-------------------�ܽŶ���--------------------------------------------------*/
#define  LED    P04  // �������ϵ�ָʾ��
#define  SW     P05  // �������ϵİ���


/* ��������nRF24LU1P�ܽ�����
P00: sck�� ��̽ӿڣ�Ҳ��������Ϊ�������ܡ�
P01: mosi����̽ӿڣ�Ҳ��������Ϊ�������ܡ�
P02: miso����̽ӿڣ�Ҳ��������Ϊ�������ܡ�
P03: csn�� ��̽ӿڣ�Ҳ��������Ϊ�������ܡ�

P04�����������LED	          
P05�����룬�������
*/

/*-----------------------------------------------------------------------------
** ��������
-----------------------------------------------------------------------------*/
static xdata uint8_t usb_in_buf[EP1_2_PACKET_SIZE];
static xdata uint8_t usb_out_buf[EP1_2_PACKET_SIZE];
static bool xdata app_usb_out_data_ready = false;
static bool xdata app_pending_usb_write = false;

/*-----------------------------------------------------------------------------
** USB�ص���������
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
 * ��  �� : MAIN����
 * ��  �� : none
 * ����ֵ : none
 *******************************************************************************************************/
void main(void)
{ 
  P0DIR = 0xEF;	      //����P0:P04����Ϊ���
	
  // USB��ʼ��
  hal_usb_init(true, device_req_cb, reset_cb, resume_cb, suspend_cb);   
  hal_usb_endpoint_config(0x81, EP1_2_PACKET_SIZE, ep_1_in_cb);  // Configure 32 byte IN endpoint 1
  hal_usb_endpoint_config(0x02, EP1_2_PACKET_SIZE, ep_2_out_cb); // Configure 32 byte OUT endpoint 2

  EA = 1;         // ʹ��ȫ���ж�
  P0DIR = 0xEF;	  // ����P0:P04����Ϊ���
  LED = 1;        // Ϩ��ָʾ��
	
	
	while(1)
  {		
		if(hal_usb_get_state() == CONFIGURED)  // USB������?
    { 
      if(app_usb_out_data_ready)// USB ���յ��������͵�����
      {
				LED = 0;   // ����ָʾ��
				app_send_usb_in_data(usb_out_buf, EP1_2_PACKET_SIZE);  // ���ؽ�������
        app_usb_out_data_ready = false;
				LED = 1;   // Ϩ��ָʾ��
      }
    }
  }
}

/*******************************************************************************************************
 * ��  �� : USB��������������
 * ��  �� : buf�����ͻ����׵�ַ
 *          size���������ݳ���
 * ����ֵ : none
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