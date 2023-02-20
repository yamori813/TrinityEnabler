/*
    Copyright (c) 2017 Jean THOMAS.

    Permission is hereby granted, free of charge, to any person obtaining
    a copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the Software
    is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
    OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#if defined(__FreeBSD__)
#include <libusb.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#endif
#include "main.h"

UInt8 disableplugin_value= 0xba;

int main(int argc, char *argv[]) {
  int i, ret;
  enum TrinityAvailablePower availablePower;
#if defined(__FreeBSD__)
  libusb_context *ctx = NULL;
  libusb_device_handle *deviceInterface;
#else
  IOUSBDeviceInterface300** deviceInterface;
#endif

  /* Reading power delivery capacity */
  availablePower = POWER_NULL;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--power-500") == 0) {
      printf("Audio device set to 500mA\n");
      availablePower = POWER_500MA;
    } else if (strcmp(argv[i], "--power-1500") == 0) {
      printf("Audio device set to 1500mA\n");
      availablePower = POWER_1500MA;
    } else if (strcmp(argv[i], "--power-3000") == 0) {
      printf("Audio device set to 3000mA\n");
      availablePower = POWER_3000MA;
    } else if (strcmp(argv[i], "--power-4000") == 0) {
      printf("Audio device set to 4000mA\n");
      availablePower = POWER_4000MA;
    }
  }

  if (availablePower == POWER_NULL) {
    printf("Available power settings :\n");
    printf("\t--power-500\t500mA\n");
    printf("\t--power-1500\t1500mA\n");
    printf("\t--power-3000\t3000mA\n");
    printf("\t--power-4000\t4000mA\n");
    
    return EXIT_SUCCESS;
  }

#if defined(__FreeBSD__)
  /* libusb initialize*/
  if (libusb_init(&ctx) < 0) {
    perror("libusb_init\n");
    return EXIT_FAILURE;
  }

  deviceInterface = open_device(ctx, 0x05AC, 0x1101);
  if (deviceInterface == -1) {
    printf("Device not found\n");
    return EXIT_FAILURE;
  }
#else
  /* Getting USB device interface */
  deviceInterface = usbDeviceInterfaceFromVIDPID(0x05AC,0x1101);
  if (deviceInterface == NULL) {
    return EXIT_FAILURE;
  }

  /* Opening USB device interface */
  ret = (*deviceInterface)->USBDeviceOpen(deviceInterface);
  if (ret == kIOReturnSuccess) {
    
  } else if (ret == kIOReturnExclusiveAccess) {
    printf("HID manager has already taken care of this device. Let's try anyway.\n");
  } else {
    printf("Could not open device. Quitting…\n");
    return EXIT_FAILURE;
  }
#endif
  
  if (disablePlugin(deviceInterface) != kIOReturnSuccess) {
    printf("Error while disabling plugin.\n");
    return EXIT_FAILURE;
  }
  if (downloadEQ(deviceInterface, availablePower) != kIOReturnSuccess) {
    printf("Error while downloading EQ to Trinity audio device.\n");
    return EXIT_FAILURE;
  }
  if (downloadPlugin(deviceInterface) != kIOReturnSuccess) {
    printf("Error while downloading plugin to Trinity audio device.\n");
    return EXIT_FAILURE;
  }
  if (enablePlugin(deviceInterface) != kIOReturnSuccess) {
    printf("Error while enabling plugin.\n");
    return EXIT_FAILURE;
  }

  /* Closing the USB device */
#if defined(__FreeBSD__)
  libusb_close(deviceInterface);
  libusb_exit(ctx);
#else
  (*deviceInterface)->USBDeviceClose(deviceInterface);
#endif

  return EXIT_SUCCESS;
}

#if defined(__FreeBSD__)
libusb_device_handle* open_device(libusb_context *ctx, int vid, int pid) {
  struct libusb_device_handle *devh = NULL;
  libusb_device *dev;
  libusb_device **devs;

  int r = 1;
  int i = 0;
  int cnt = 0;

//  libusb_set_debug(ctx, 3);

  if ((libusb_get_device_list(ctx, &devs)) < 0) {
//    perror("no usb device found");
//    exit(1);
    return -1;
  }

  /* check every usb devices */
  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      perror("failed to get device descriptor\n");
    }
    /* count how many device connected */
    if (desc.idVendor == vid && desc.idProduct == pid) {
      cnt++;
    }
  }

  /* device not found */
  if (cnt == 0) {
//    fprintf(stderr, "device not connected or lack of permissions\n");
//    exit(1);
    return -1;
  }

  if (cnt > 1) {
//    fprintf(stderr, "multi device is not implemented yet\n");
//    exit(1);
    return -1;
  }


  /* open device */
  if ((devh = libusb_open_device_with_vid_pid(ctx, vid, pid)) < 0) {
    perror("can't find device\n");
//    close_device(ctx, devh);
//    exit(1);
    return -1;
  }

  /* detach kernel driver if attached. */
  r = libusb_kernel_driver_active(devh, 3);
  if (r == 1) {
    /* detaching kernel driver */
    r = libusb_detach_kernel_driver(devh, 3);
    if (r != 0) {
//      perror("detaching kernel driver failed");
//      exit(1);
      return -1;
    }
  }

  r = libusb_claim_interface(devh, 0);
  if (r < 0) {
//    fprintf(stderr, "claim interface failed (%d): %s\n", r, strerror(errno));
//    exit(1);
    return -1;
  }

  return devh;
}
#else
IOUSBDeviceInterface300** usbDeviceInterfaceFromVIDPID(SInt32 vid, SInt32 pid) {
  CFMutableDictionaryRef matchingDict;
  io_iterator_t usbRefIterator;
  io_service_t usbRef;
  IOCFPlugInInterface** plugin;
  IOUSBDeviceInterface300** deviceInterface;
  SInt32 score;

  /* Creating a matching dictionary to match the device's PID and VID */
  matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
  CFDictionaryAddValue(matchingDict,
		       CFSTR(kUSBVendorID),
		       CFNumberCreate(kCFAllocatorDefault,
				      kCFNumberSInt32Type,
				      &vid));
  CFDictionaryAddValue(matchingDict,
		       CFSTR(kUSBProductID),
		       CFNumberCreate(kCFAllocatorDefault,
				      kCFNumberSInt32Type,
				      &pid));

  /* Getting all the devices that are matched by the matching dictionary */
  IOServiceGetMatchingServices(kIOMasterPortDefault,
			       matchingDict,
			       &usbRefIterator);

  /* We only use the first USB device */
  usbRef = IOIteratorNext(usbRefIterator);
  IOObjectRelease(usbRefIterator);

  if (usbRef == 0) {
    printf("%s : Can't find suitable USB audio device\n", __PRETTY_FUNCTION__);
    return NULL;
  } else {
    IOCreatePlugInInterfaceForService(usbRef,
				      kIOUSBDeviceUserClientTypeID,
				      kIOCFPlugInInterfaceID,
				      &plugin,
				      &score);
    IOObjectRelease(usbRef);
    (*plugin)->QueryInterface(plugin,
			      CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID300),
			      (LPVOID)&deviceInterface);
    (*plugin)->Release(plugin);

    return deviceInterface;
  }
}
#endif

#if defined(__FreeBSD__)
IOReturn xdfpSetMem(struct libusb_device_handle *deviceInterface, UInt8 *buf, UInt16 length, UInt16 xdfpAddr) {
  int r;

  r = libusb_control_transfer(deviceInterface,
    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
    kMicronasSetMemReq, 0, xdfpAddr, buf, length, 1000);
  return r < 0 ? kIOReturnError : kIOReturnSuccess;
}
#else
IOReturn xdfpSetMem(IOUSBDeviceInterface300** deviceInterface, UInt8 *buf, UInt16 length, UInt16 xdfpAddr) {
  IOUSBDevRequest devReq;

  devReq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
  devReq.bRequest = kMicronasSetMemReq;
  devReq.wValue = 0;
  devReq.wIndex = xdfpAddr;
  devReq.wLength = length;
  devReq.pData = buf;

  return (*deviceInterface)->DeviceRequest(deviceInterface, &devReq);
}
#endif

#if defined(__FreeBSD__)
IOReturn xdfpWrite(struct libusb_device_handle *deviceInterface, UInt16 xdfpAddr, SInt32 value) {
#else
IOReturn xdfpWrite(IOUSBDeviceInterface300** deviceInterface, UInt16 xdfpAddr, SInt32 value) {
#endif
  static UInt8 xdfpData[5];

    if (value < 0) value += 0x40000;
    xdfpData[0] = (value >> 10) & 0xff;
    xdfpData[1] = (value >> 2) & 0xff;
    xdfpData[2] = value & 0x03;
    xdfpData[3] = (xdfpAddr >> 8) & 0x03;
    xdfpData[4] = xdfpAddr & 0xff;

    return xdfpSetMem(deviceInterface, xdfpData, 5, V8_WRITE_START_ADDR);
}

#if defined(__FreeBSD__)
IOReturn downloadEQ(struct libusb_device_handle *deviceInterface, enum TrinityAvailablePower availablePower) {
#else
IOReturn downloadEQ(IOUSBDeviceInterface300** deviceInterface, enum TrinityAvailablePower availablePower) {
#endif
  UInt16 xdfpAddr;
  UInt32 eqIndex;
  IOReturn ret;
  static SInt32 *eqSettings;

  switch (availablePower) {
  case POWER_4000MA:
    eqSettings = power4AEQSettings;
    break;
  case POWER_3000MA:
    eqSettings = power3AEQSettings;
    break;
  case POWER_1500MA:
    eqSettings = power1500mAEQSettings;
    break;
  default:
  case POWER_500MA:
    eqSettings = power500mAEQSettings;
    break;
  }
  
  for (eqIndex = 0, xdfpAddr = XDFP_STARTING_EQ_ADDR; eqIndex < EQ_TABLE_SIZE; eqIndex++, xdfpAddr++) {
    ret = xdfpWrite(deviceInterface, xdfpAddr, eqSettings[eqIndex]);
    if (ret != kIOReturnSuccess) {
      return ret;
    }
    
    nanosleep((const struct timespec[]){{0, 3000000L}}, NULL);
  }

  return ret;
}

#if defined(__FreeBSD__)
IOReturn disablePlugin(struct libusb_device_handle *deviceInterface) {
#else
IOReturn disablePlugin(IOUSBDeviceInterface300** deviceInterface) {
#endif
  return xdfpSetMem(deviceInterface, &disableplugin_value, 1, V8_PLUGIN_START_ADDR);
}

#if defined(__FreeBSD__)
int enablePlugin(struct libusb_device_handle *deviceInterface) {
#else
IOReturn enablePlugin(IOUSBDeviceInterface300** deviceInterface) {
#endif
  return xdfpSetMem(deviceInterface, pluginBinary, 1, V8_PLUGIN_START_ADDR);
}

#if defined(__FreeBSD__)
IOReturn downloadPlugin(struct libusb_device_handle *deviceInterface) {
#else
IOReturn downloadPlugin(IOUSBDeviceInterface300** deviceInterface) {
#endif
  return xdfpSetMem(deviceInterface, &pluginBinary[1], sizeof(pluginBinary), V8_PLUGIN_START_ADDR+1);
}
