/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <usbhost/usbhost.h>
#include <linux/usb/f_accessory.h>

#include "AndroidAccessory.h"

#define USB_ACCESSORY_VENDOR_ID         0x18D1
#define USB_ACCESSORY_PRODUCT_ID        0x2D00

#define USB_ACCESSORY_ADB_PRODUCT_ID    0x2D01
#define ACCESSORY_STRING_MANUFACTURER   0
#define ACCESSORY_STRING_MODEL          1
#define ACCESSORY_STRING_DESCRIPTION    2
#define ACCESSORY_STRING_VERSION        3
#define ACCESSORY_STRING_URI            4
#define ACCESSORY_STRING_SERIAL         5

#define ACCESSORY_GET_PROTOCOL          51
#define ACCESSORY_SEND_STRING           52
#define ACCESSORY_START                 53


AndroidAccessory::AndroidAccessory(const char* manufacturer,
                                   const char* model,
                                   const char* description,
                                   const char* version,
                                   const char* uri,
                                   const char* serial) :
    _manufacturer(manufacturer),
    _model(model),
    _description(description),
    _version(version),
    _uri(uri),
    _serial(serial),
    _inited(false),
    _connected(false),
    _dev(NULL),
    _epRead(-1),
    _epWrite(-1),
    _context(NULL)
{
    struct usb_host_context* context = usb_host_init();
    if (!context) {
        fprintf(stderr, "usb_host_init failed");
        return;
    }

    _context = (void*)context;

    inited = true;
}

AndroidAccessory::~AndroidAccessory(void)
{
    _disconnect();
    struct usb_host_context* context = _context;
    usb_host_cleanup(context);
}

void AndroidAccessory::powerOn(void)
{
}

void AndroidAccessory::_disconnect(void)
{
    if (_dev != NULL)
        usb_device_close(_dev);

    _dev = NULL;

    _connected = false;
    _epRead = -1;
    _epWrite = -1;
}

bool AndroidAccessory::_isAccessoryDevice(usb_device* dev)
{
    uint16_t vendorId = usb_device_get_vendor_id(dev);
    uint16_t productId = usb_device_get_product_id(dev);

    return (vendorId == 0x18d1 || vendorId == 0x22B8) &&
           (productId == 0x2D00 || productId == 0x2D01);
}

void AndroidAccessory::_sendString(int index, const char* str)
{
    if (_dev == NULL) {
        fprintf(stderr, "%s: Fail: device not opened!");
        return;
    }

    int ret = usb_device_control_transfer(_dev,
                                          USB_DIR_OUT | USB_TYPE_VENDOR,
                                          ACCESSORY_SEND_STRING,
                                          0,
                                          index,
                                          (void*)str, strlen(str) + 1,
                                          0);

    // some devices can't handle back-to-back requests, so delay a bit
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_nsec = 10 * 1000000;
    nanosleep(&tm, NULL);
}

int AndroidAccessory::_getProtocol(struct usb_device* dev)
{
    uint16_t protocol;
    int ret = usb_device_control_transfer(dev,
                                          USB_DIR_IN | USB_TYPE_VENDOR,
                                          ACCESSORY_GET_PROTOCOL,
                                          0,
                                          0,
                                          &protocol,
                                          sizeof(protocol),
                                          0);

    if (ret != sizeof(protocol)) {
        fprintf(stderr, "Failed to get protocol!");
        return -1;
    }

    return protocol;
}

bool AndroidAccessory::_switchDevice(struct usb_device* dev)
{
    int protocol = getProtocol(dev);

    if (protocol == 1) {
        Serial.print("device supports protcol 1\n");
    } else {
        Serial.print("could not read device protocol version\n");
        return false;
    }

    sendString(dev, ACCESSORY_STRING_MANUFACTURER, manufacturer);
    sendString(dev, ACCESSORY_STRING_MODEL, model);
    sendString(dev, ACCESSORY_STRING_DESCRIPTION, description);
    sendString(dev, ACCESSORY_STRING_VERSION, version);
    sendString(dev, ACCESSORY_STRING_URI, uri);
    sendString(dev, ACCESSORY_STRING_SERIAL, serial);

    ret = usb_device_control_transfer(device, USB_DIR_OUT | USB_TYPE_VENDOR,
                                      ACCESSORY_START, 0, 0, 0, 0, 0);

    // TODO: need to wait?

    return ret == 0 ? true : false;
}

// Finds the first bulk IN and bulk OUT endpoints
bool AndroidAccessory::_findEndpoints(struct usb_device* dev,
                                      int* epRead, int* epWrite)
{
    struct usb_descriptor_header* desc;
    struct usb_descriptor_iter iter;
    struct usb_interface_descriptor* intf = NULL;
    struct usb_endpoint_descriptor* ep1 = NULL;
    struct usb_endpoint_descriptor* ep2 = NULL;

    usb_descriptor_iter_init(dev, &iter);
    while ((desc = usb_descriptor_iter_next(&iter)) != NULL &&
            (!intf || !ep1 || !ep2)) {
        if (desc->bDescriptorType == USB_DT_INTERFACE) {
            intf = (struct usb_interface_descriptor*)desc;
        } else if (desc->bDescriptorType == USB_DT_ENDPOINT) {
            if (ep1)
                ep2 = (struct usb_endpoint_descriptor*)desc;
            else
                ep1 = (struct usb_endpoint_descriptor*)desc;
        }
    }

    if (!intf) {
        fprintf(stderr, "interface not found\n");
        return false;
    }
    if (!ep1 || !ep2) {
        fprintf(stderr, "endpoints not found\n");
        return false;
    }

    if (usb_device_claim_interface(dev, intf->bInterfaceNumber)) {
        fprintf(stderr, "usb_device_claim_interface failed errno: %d\n", errno);
        return false;
    }

    if ((ep1->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) {
        *epRead = ep1->bEndpointAddress;
        *epWrite = ep2->bEndpointAddress;
    } else {
        *epRead = ep2->bEndpointAddress;
        *epWrite = ep1->bEndpointAddress;
    }

    return true;
}

bool AndroidAccessory::configureAndroid(void)
{
#if 0
    byte err;
    EP_RECORD inEp, outEp;

    if (!findEndpoints(1, &inEp, &outEp))
        return false;

    memset(&epRecord, 0x0, sizeof(epRecord));

    epRecord[inEp.epAddr] = inEp;
    if (outEp.epAddr != inEp.epAddr)
        epRecord[outEp.epAddr] = outEp;

    in = inEp.epAddr;
    out = outEp.epAddr;

    Serial.println(inEp.epAddr, HEX);
    Serial.println(outEp.epAddr, HEX);

    epRecord[0] = *(usb.getDevTableEntry(0, 0));
    usb.setDevTableEntry(1, epRecord);

    err = usb.setConf(1, 0, 1);
    if (err) {
        Serial.print("Can't set config to 1\n");
        return false;
    }

    usb.setUsbTaskState(USB_STATE_RUNNING);
#endif

    return true;
}

int AndoridAccessory::_cbHostDeviceAdded(const char* devname,
        void* client_data)
{
    return 0;
}

int AndroidAccessory::_cbHostDeviceRemoved(const char* devname,
        void* client_data)
{
    if (_dev && !strcmp(usb_device_get_name(_dev), devname)) {
        _disconnect();
        return 1;
    }
    return 0;
}


bool AndroidAccessory::isConnected(void)
{
    USB_DEVICE_DESCRIPTOR* devDesc = (USB_DEVICE_DESCRIPTOR*) descBuff;
    byte err;

    max.Task();
    usb.Task();

    if (!connected &&
            usb.getUsbTaskState() >= USB_STATE_CONFIGURING &&
            usb.getUsbTaskState() != USB_STATE_RUNNING) {
        Serial.print("\nDevice addressed... ");
        Serial.print("Requesting device descriptor.\n");

        err = usb.getDevDescr(1, 0, 0x12, (char*) devDesc);
        if (err) {
            Serial.print("\nDevice descriptor cannot be retrieved. Trying again\n");
            return false;
        }

        if (isAccessoryDevice(devDesc)) {
            Serial.print("found android acessory device\n");

            connected = configureAndroid();
        } else {
            Serial.print("found possible device. swithcing to serial mode\n");
            switchDevice(1);
        }
    } else if (usb.getUsbTaskState() == USB_DETACHED_SUBSTATE_WAIT_FOR_DEVICE) {
        if (connected)
            Serial.println("disconnect\n");
        connected = false;
    }

    return connected;
}

int AndroidAccessory::read(void* buff, int len, unsigned int nakLimit)
{
    return usb.newInTransfer(1, in, len, (char*)buff, nakLimit);
}

int AndroidAccessory::write(void* buff, int len)
{
    usb.outTransfer(1, out, len, (char*)buff);
    return len;
}

