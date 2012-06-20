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

#ifndef __AndroidAccessory_h__
#define __AndroidAccessory_h__

#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif

// TODO: Isn't this limit of Max3421e?
#define USB_NAK_LIMIT       32000

class AndroidAccessory {
public:
    AndroidAccessory(const char* manufacturer,
                     const char* model,
                     const char* description,
                     const char* version,
                     const char* uri,
                     const char* serial);
    ~AndroidAccessory(void);

    void powerOn(void);

    bool isConnected(void);
    int read(void* buff, int len, unsigned int nakLimit = USB_NAK_LIMIT);
    int write(void* buff, int len);

private:
    const char* _manufacturer;
    const char* _model;
    const char* _description;
    const char* _version;
    const char* _uri;
    const char* _serial;

    bool _inited;
    bool _connected;

    struct usb_device *_dev; 

    int _epRead;
    int _epWrite;

    void* _context;

    EP_RECORD epRecord[8];

    uint8_t descBuff[256];

    void _disconnect(void);
    bool _isAccessoryDevice(struct usb_device *dev);
    void _sendString(struct usb_device* dev, int index, const char* str);
    int  _getProtocol(struct usb_device* dev);
    bool _switchDevice(struct usb_device* dev);
    bool _findEndpoints(struct usb_device* dev, int* epRead, int* epWrite);
    bool configureAndroid(void);

    int _cbHostDeviceAdded(const char* devname, void* client_data);
    int _cbHostDeviceRemoved(const char* devname, void* client_data);

};

#endif /* __AndroidAccessory_h__ */
