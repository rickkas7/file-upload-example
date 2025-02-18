#include "FileUploadRK.h"
#include "SequentialFileRK.h"

#include <fcntl.h>
#include <sys/stat.h>

SYSTEM_MODE(AUTOMATIC);

// System thread defaults to on in 6.2.0 and later and this line is not required
#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED);
#endif

SerialLogHandler logHandler(LOG_LEVEL_TRACE);

const std::chrono::milliseconds publishPeriod = 5min;
unsigned long lastPublish = 0;
CloudEvent event;

// Random test data

const uint8_t testData1[256] = {
    0x7b, 0xa0, 0xab, 0xab, 0xda, 0x0c, 0x97, 0xfb, 0x5d, 0xcc, 0x15, 0xe1, 0xeb, 0x7e, 0xd3, 0xbb, 0x87, 0xca, 0xc9, 0x64, 
    0xce, 0xda, 0x97, 0x03, 0x7e, 0x9f, 0xfe, 0x27, 0x34, 0xba, 0x38, 0xce, 0x78, 0xcf, 0xca, 0xc1, 0xaa, 0x78, 0xf1, 0x65, 
    0xd6, 0x5b, 0x54, 0x49, 0xc1, 0x5e, 0xb3, 0x59, 0x0f, 0x31, 0x7f, 0x65, 0x9a, 0x73, 0x5a, 0xb7, 0xd3, 0xd9, 0x4e, 0x16, 
    0xa5, 0xc6, 0xac, 0xdd, 0x79, 0x8d, 0x70, 0x92, 0xf1, 0x74, 0xd1, 0x24, 0xd1, 0x8c, 0x4b, 0x35, 0xf1, 0x0f, 0x35, 0x2e, 
    0xa3, 0x41, 0xc2, 0x9d, 0x8d, 0x2c, 0xc2, 0xe6, 0x49, 0xcc, 0x92, 0x3e, 0xd9, 0xb6, 0x27, 0xdd, 0xd4, 0xcd, 0x61, 0x96, 
    0x16, 0xdd, 0x21, 0xa5, 0xf0, 0x96, 0xe9, 0x1a, 0xaf, 0xfa, 0x02, 0x52, 0x28, 0x87, 0xe7, 0xa6, 0x2a, 0x9f, 0x0d, 0x5a, 
    0xf8, 0x46, 0xdc, 0x30, 0x58, 0x61, 0xc5, 0x21, 0x22, 0x17, 0x83, 0xd7, 0x0a, 0xb9, 0x90, 0x47, 0x8a, 0xe4, 0x65, 0x2c, 
    0x7d, 0xdc, 0x85, 0xd7, 0x12, 0x25, 0x79, 0x7f, 0x8d, 0x08, 0x08, 0x83, 0x54, 0xd1, 0x67, 0xbf, 0x9e, 0x8d, 0x0c, 0xf5, 
    0x48, 0x70, 0x23, 0xb9, 0xee, 0x0f, 0x65, 0x33, 0x5a, 0xff, 0x23, 0xe2, 0x2d, 0x4f, 0xcd, 0x4b, 0xd3, 0xea, 0x2d, 0xa9, 
    0x24, 0xcc, 0xe9, 0xba, 0x07, 0x29, 0x91, 0x49, 0xd8, 0x43, 0xa0, 0x2b, 0x98, 0xd0, 0x48, 0x5b, 0x0a, 0xae, 0x13, 0x85, 
    0x7e, 0xd5, 0x0d, 0x90, 0x26, 0xa6, 0x1d, 0xca, 0x46, 0xfc, 0x02, 0x6b, 0xda, 0xb9, 0x20, 0x6b, 0xfb, 0xc7, 0xea, 0x5a, 
    0x4a, 0x0f, 0x1a, 0x74, 0xec, 0xa0, 0x6e, 0x68, 0x58, 0x4b, 0xa5, 0x4d, 0x78, 0xeb, 0x01, 0xc9, 0x0f, 0x08, 0xe4, 0xa2, 
    0x31, 0x9e, 0xdd, 0x2b, 0x29, 0x98, 0xaa, 0xe6, 0xc5, 0x9e, 0x9e, 0x5b, 0x69, 0x70, 0x8c, 0xd8};
    
const char *testData1_hash = "46fbef86972609910f999ff39645443a8fae5fc4";
    

const uint8_t testData2[4500] = {
    0x4e, 0x38, 0xd9, 0xf3, 0xff, 0x35, 0xf8, 0x89, 0x93, 0x48, 0x98, 0xc9, 0x7d, 0xe2, 0x24, 0xee, 0x8a, 0xcf, 0x85, 0x41, 
    0x10, 0xb5, 0x57, 0xb5, 0x2a, 0x9a, 0x9d, 0x22, 0x78, 0x06, 0xe6, 0xe5, 0xf6, 0x85, 0xda, 0x38, 0xbe, 0x85, 0x2f, 0xe8, 
    0xdc, 0x26, 0xa2, 0x40, 0x23, 0xc6, 0x8e, 0xf7, 0xd4, 0xc5, 0xaa, 0xbb, 0x6b, 0x02, 0x72, 0x86, 0x0b, 0xd4, 0x16, 0xc0, 
    0x04, 0x8c, 0xef, 0x6b, 0x54, 0xe4, 0xb3, 0x5b, 0x25, 0xbb, 0xaf, 0xe1, 0x59, 0x12, 0x84, 0x7f, 0xb4, 0x7e, 0xc2, 0x5c, 
    0x15, 0x33, 0x32, 0x2e, 0xb6, 0x71, 0x81, 0x27, 0xb0, 0x24, 0x06, 0x43, 0x1a, 0xc2, 0x44, 0x62, 0x2e, 0xfc, 0x4f, 0x28, 
    0x46, 0x34, 0x2b, 0xf6, 0x0f, 0x7e, 0xb8, 0xb8, 0x7d, 0x9f, 0xbf, 0x8b, 0xb7, 0xf8, 0x0f, 0xbe, 0x87, 0x38, 0xc7, 0xf9, 
    0x2a, 0x66, 0x42, 0x57, 0x42, 0xf6, 0xa8, 0x90, 0x0a, 0x21, 0x13, 0x9e, 0x48, 0x5d, 0x68, 0x00, 0x80, 0x92, 0xb4, 0xca, 
    0xb8, 0x7d, 0xea, 0xff, 0x54, 0xb1, 0x40, 0x26, 0xdc, 0x1d, 0xdf, 0x1b, 0xf1, 0xc3, 0x2e, 0x95, 0x46, 0x0e, 0x0c, 0x9c, 
    0x22, 0x16, 0xe2, 0x66, 0x34, 0x0c, 0x69, 0xda, 0x51, 0x7d, 0x60, 0x83, 0xe0, 0xcc, 0xbb, 0x8d, 0x07, 0x33, 0xb3, 0xd0, 
    0x25, 0x5d, 0xd8, 0xa6, 0xff, 0x85, 0xf5, 0x6e, 0x42, 0xf5, 0x41, 0x7a, 0x4b, 0xd7, 0x1e, 0xc0, 0x9f, 0x0a, 0x97, 0x27, 
    0xed, 0x55, 0x90, 0x70, 0x38, 0x29, 0x48, 0xb5, 0x30, 0xc7, 0x27, 0xfd, 0x44, 0x7f, 0x88, 0x77, 0x7c, 0x69, 0x56, 0xd0, 
    0x87, 0xd2, 0x3f, 0x49, 0x8a, 0xa0, 0x31, 0x76, 0x67, 0x5b, 0x85, 0x93, 0x4b, 0xb2, 0x85, 0xb0, 0xaa, 0x5d, 0xb6, 0x45, 
    0xc6, 0x19, 0x7b, 0x5e, 0xf9, 0x5f, 0x6e, 0x66, 0x37, 0xa1, 0x1b, 0x50, 0x75, 0x25, 0xb7, 0x9c, 0xfa, 0x56, 0x1b, 0x52, 
    0xb3, 0xf6, 0xf9, 0x3e, 0xac, 0x53, 0x2c, 0xfd, 0xe8, 0x7b, 0xe3, 0x48, 0x10, 0xba, 0x21, 0x26, 0x14, 0xfe, 0xaf, 0xb0, 
    0x5a, 0x5b, 0x9b, 0x47, 0xe9, 0x88, 0xa3, 0xbe, 0x30, 0x5b, 0xc0, 0xfb, 0xb7, 0x0e, 0x70, 0x81, 0x72, 0x64, 0x18, 0xd4, 
    0x1b, 0x84, 0x05, 0xd7, 0x13, 0x76, 0x1d, 0x04, 0xda, 0x71, 0xf0, 0xdd, 0x7a, 0x00, 0x7e, 0x2b, 0x5c, 0xe8, 0x30, 0x99, 
    0x78, 0x98, 0xa1, 0x07, 0x6d, 0x4d, 0x92, 0x48, 0xc1, 0x68, 0xaa, 0xf3, 0x80, 0xa7, 0x7a, 0x2c, 0x38, 0xe2, 0x2c, 0xf5, 
    0x5f, 0xd0, 0x3e, 0xc0, 0xb6, 0x69, 0xfd, 0x07, 0xd6, 0xed, 0xee, 0xeb, 0x69, 0x03, 0xa6, 0x45, 0x8d, 0x49, 0xd5, 0x0d, 
    0xeb, 0x52, 0x31, 0x44, 0xff, 0x80, 0xdc, 0xb0, 0x69, 0xb9, 0x72, 0xb5, 0xcd, 0x80, 0xd0, 0xdc, 0x40, 0xdf, 0x14, 0x71, 
    0x2c, 0x75, 0x77, 0xa6, 0x1a, 0x90, 0x0f, 0xf6, 0x43, 0x63, 0x3e, 0xfc, 0xdc, 0x53, 0x2c, 0x1a, 0x5b, 0x7c, 0x7e, 0x5d, 
    0x6f, 0xd4, 0xf7, 0xcc, 0xbe, 0x28, 0x24, 0xaf, 0x93, 0xa3, 0x08, 0xd6, 0x29, 0x2f, 0x80, 0x01, 0xfe, 0xc6, 0x66, 0xb8, 
    0xef, 0x36, 0x7e, 0xd4, 0xb1, 0x19, 0x38, 0x01, 0x1d, 0xe3, 0xef, 0xc6, 0xb1, 0xaa, 0x82, 0x1f, 0x38, 0x62, 0x08, 0x60, 
    0xfd, 0x3a, 0x28, 0x97, 0xe7, 0xde, 0x9a, 0xeb, 0xca, 0xb3, 0x51, 0x8f, 0xa1, 0x57, 0x57, 0xca, 0x6d, 0x5b, 0xc6, 0xa5, 
    0x18, 0xa3, 0x0a, 0x5c, 0x02, 0x1d, 0xc3, 0x5d, 0xc0, 0xed, 0x2f, 0x5f, 0xd8, 0xe1, 0x9b, 0x0b, 0x01, 0xcf, 0x99, 0xf4, 
    0x7e, 0xed, 0xe0, 0xe6, 0xe9, 0xdd, 0x7b, 0xe8, 0xef, 0x7c, 0x2c, 0xb4, 0x42, 0x5c, 0x32, 0x5b, 0xa2, 0xd5, 0x35, 0x9b, 
    0x25, 0xd8, 0x0b, 0xf0, 0x53, 0x29, 0xc9, 0xe4, 0x28, 0x49, 0x6d, 0x86, 0x4a, 0x7c, 0x45, 0xc3, 0x90, 0x2e, 0xd1, 0x9f, 
    0xbb, 0x7b, 0x01, 0xfe, 0xfe, 0x60, 0x6c, 0x18, 0xc9, 0xdd, 0xa4, 0x02, 0x1f, 0xe8, 0x9d, 0x07, 0x5c, 0x13, 0x7f, 0x8b, 
    0x25, 0xc1, 0xb5, 0x1e, 0xff, 0x9e, 0xde, 0x39, 0x29, 0x13, 0xf3, 0x58, 0x83, 0x67, 0x56, 0x79, 0xaa, 0xbe, 0x91, 0xa1, 
    0x07, 0xcd, 0x74, 0x89, 0x5d, 0x6b, 0xeb, 0x96, 0xc1, 0x06, 0x2e, 0x3c, 0x27, 0xc4, 0x90, 0xa3, 0x5b, 0x94, 0x84, 0x36, 
    0xec, 0x36, 0x7e, 0x47, 0x71, 0x48, 0x82, 0xa1, 0xf2, 0x89, 0xdd, 0x90, 0x6f, 0x90, 0xdf, 0xca, 0xfd, 0x70, 0x0f, 0x71, 
    0xe0, 0x40, 0x7a, 0x1b, 0x8f, 0x9c, 0x6b, 0x10, 0xda, 0x2f, 0x22, 0x6f, 0x34, 0x88, 0x8c, 0x4b, 0xfe, 0x30, 0xb0, 0xf7, 
    0xa1, 0x3e, 0xc6, 0xe6, 0xea, 0x87, 0x82, 0x84, 0x2b, 0x41, 0xfc, 0x12, 0x2a, 0x78, 0x99, 0xcf, 0xc4, 0x25, 0x71, 0xd5, 
    0x50, 0x21, 0xc7, 0xa8, 0x4b, 0x1d, 0xaa, 0xe4, 0x39, 0x8e, 0xef, 0x1d, 0x14, 0x89, 0xb6, 0xe5, 0x77, 0xf8, 0xf0, 0x25, 
    0xb8, 0xc0, 0x14, 0xf6, 0xd6, 0xdb, 0x7d, 0x6e, 0x00, 0x0c, 0xa4, 0xbd, 0x70, 0x39, 0x15, 0xce, 0x45, 0x21, 0x42, 0x12, 
    0xc0, 0x52, 0x7c, 0x84, 0xb3, 0xa9, 0x37, 0x5b, 0x6f, 0x66, 0xa0, 0x3a, 0x13, 0xa3, 0x37, 0xe4, 0x4a, 0x53, 0xde, 0x10, 
    0x86, 0x6e, 0xcc, 0x6b, 0xc9, 0x79, 0x87, 0xab, 0x45, 0x4d, 0xa6, 0xc6, 0x7d, 0x8b, 0x41, 0x85, 0x82, 0x8d, 0x2f, 0xb9, 
    0x4f, 0x9e, 0x8a, 0xfb, 0x85, 0xa2, 0xb3, 0xc9, 0x55, 0x1d, 0xae, 0xd4, 0x94, 0xe5, 0x84, 0x4d, 0x44, 0x3f, 0xdc, 0x8a, 
    0xae, 0x16, 0x2c, 0x6a, 0x11, 0x76, 0x5e, 0xb6, 0x02, 0xe6, 0x76, 0x35, 0xa4, 0x3f, 0x2b, 0x89, 0x87, 0xbb, 0x7b, 0x86, 
    0x14, 0x4a, 0xb4, 0xb1, 0x7f, 0x0f, 0x92, 0xc0, 0x62, 0xba, 0x00, 0x57, 0x37, 0x10, 0xfc, 0x87, 0x10, 0x5d, 0x8c, 0x9b, 
    0xd1, 0xf1, 0x3b, 0x73, 0xa8, 0x58, 0x21, 0x0d, 0x1c, 0xfa, 0xf1, 0x35, 0xb6, 0x4a, 0xa2, 0x22, 0x0b, 0x26, 0x6e, 0x28, 
    0x5c, 0x77, 0xf8, 0x3b, 0x16, 0x23, 0x1c, 0x2d, 0x04, 0x99, 0x44, 0xce, 0xf0, 0xd2, 0x61, 0x5a, 0xbe, 0xc2, 0x76, 0xbc, 
    0x9e, 0xaf, 0x46, 0xe0, 0x46, 0x9a, 0xee, 0x5e, 0xf5, 0x96, 0xa8, 0xab, 0xe9, 0xe6, 0x92, 0xde, 0x4e, 0xaf, 0xbd, 0x63, 
    0x9c, 0x41, 0xb1, 0x8e, 0x98, 0x8f, 0x91, 0xd1, 0x9e, 0x10, 0x20, 0x78, 0x49, 0x2a, 0x31, 0x22, 0xba, 0x59, 0xe1, 0x05, 
    0x8a, 0x9a, 0xa0, 0xb1, 0x52, 0x65, 0x86, 0x34, 0x70, 0xcf, 0x4a, 0x04, 0xb9, 0x0e, 0xe7, 0xc5, 0xc5, 0x2e, 0x6c, 0xdc, 
    0x8c, 0x89, 0x11, 0x19, 0x3f, 0x76, 0x75, 0xa2, 0x8a, 0x3e, 0xbc, 0xbd, 0x49, 0xb4, 0x83, 0x33, 0x5b, 0xf0, 0x5e, 0x21, 
    0x8f, 0xa1, 0x9a, 0x0a, 0x82, 0x2a, 0x90, 0xa0, 0xbc, 0xdf, 0x80, 0xb0, 0x71, 0xaf, 0xa2, 0xdb, 0x2c, 0xa2, 0x54, 0x14, 
    0xbc, 0xb3, 0x59, 0x8a, 0xab, 0xe6, 0x69, 0x41, 0xb4, 0x75, 0xda, 0x40, 0xaa, 0xb4, 0x0a, 0x02, 0x9c, 0xb1, 0x80, 0xca, 
    0x33, 0x30, 0x1d, 0x8b, 0x9b, 0x9b, 0x3c, 0x65, 0xd4, 0x88, 0x6a, 0xc9, 0x59, 0xb1, 0xbc, 0x59, 0x12, 0xef, 0xe9, 0x2a, 
    0xb8, 0x5f, 0x6c, 0xc6, 0x6e, 0xdc, 0xf4, 0xa7, 0xe9, 0xa3, 0x1b, 0x0d, 0x24, 0x15, 0x43, 0x9f, 0xc3, 0xf3, 0xc2, 0xd6, 
    0xc6, 0x6f, 0xe2, 0x1c, 0xc9, 0x23, 0x9d, 0x35, 0x17, 0xa0, 0x43, 0x8d, 0x94, 0xbc, 0x7e, 0x58, 0xf2, 0x57, 0xc8, 0x09, 
    0xea, 0xae, 0xb8, 0xc8, 0xd8, 0x18, 0x38, 0xa1, 0xbc, 0x25, 0x66, 0xb3, 0x48, 0x4d, 0x9f, 0x92, 0x66, 0x2b, 0x56, 0x44, 
    0xec, 0x26, 0x53, 0xad, 0xc5, 0x29, 0xdb, 0xdc, 0xfc, 0xcd, 0x1d, 0x04, 0x04, 0x18, 0x2a, 0x38, 0x4a, 0xd8, 0x44, 0x4e, 
    0x0d, 0x8e, 0xb1, 0xe9, 0xa5, 0xf8, 0x3c, 0x43, 0x8a, 0xb8, 0xd7, 0x87, 0x8d, 0x13, 0xc2, 0xed, 0xf4, 0x44, 0xdc, 0x25, 
    0x63, 0xba, 0xeb, 0x89, 0xdc, 0xf5, 0x97, 0xee, 0x47, 0x1a, 0x45, 0xec, 0x04, 0x17, 0xff, 0x32, 0x85, 0x2a, 0xf6, 0xb5, 
    0xa9, 0xd4, 0xa5, 0x6c, 0xe7, 0x2f, 0xa1, 0x26, 0x94, 0x45, 0x46, 0x69, 0xdf, 0x81, 0x6c, 0x7d, 0x10, 0x7e, 0xe1, 0x5f, 
    0x5b, 0xd2, 0xee, 0x9e, 0xff, 0x79, 0xce, 0xbe, 0xac, 0x17, 0xa1, 0x88, 0x60, 0xb0, 0x82, 0x43, 0x2f, 0x98, 0xb0, 0x2d, 
    0xd6, 0x6e, 0x5b, 0xe6, 0x55, 0xe1, 0xe0, 0x88, 0x89, 0x2b, 0x0d, 0x72, 0x56, 0x67, 0xe8, 0xc2, 0x99, 0x3a, 0x9a, 0x93, 
    0xdd, 0xe2, 0x2d, 0x44, 0x6f, 0x24, 0x33, 0x61, 0xd7, 0xad, 0xca, 0xdd, 0xd6, 0xf9, 0x72, 0xb1, 0x95, 0x3d, 0x3a, 0xb4, 
    0x30, 0xde, 0xd7, 0xd2, 0x81, 0x19, 0x8b, 0xc1, 0x19, 0x3a, 0x2d, 0x8b, 0x0d, 0x72, 0x35, 0x84, 0x88, 0x97, 0x5d, 0x04, 
    0x53, 0xad, 0x30, 0xa4, 0xdd, 0x3f, 0x15, 0xc7, 0x70, 0x89, 0xe6, 0x00, 0xb2, 0x86, 0x83, 0xd7, 0x2a, 0x60, 0x74, 0xf0, 
    0xd7, 0x7b, 0x0e, 0x26, 0x2a, 0x27, 0xb0, 0xd4, 0xe0, 0x6d, 0x55, 0x8b, 0x48, 0x6a, 0x35, 0x60, 0xcd, 0x7d, 0x0e, 0x18, 
    0x60, 0xa4, 0x20, 0x3e, 0xd0, 0x78, 0xfc, 0xa2, 0x03, 0x70, 0xad, 0xa8, 0x34, 0xc9, 0xb1, 0x21, 0x70, 0x90, 0xd6, 0xb5, 
    0xd0, 0xf4, 0x76, 0x7a, 0xfc, 0x0b, 0x0e, 0x12, 0x27, 0xc2, 0x93, 0xb0, 0x3c, 0x8e, 0xfa, 0xbe, 0xa8, 0xc1, 0x58, 0x07, 
    0x04, 0x2e, 0xf6, 0x9f, 0x7e, 0xb6, 0x57, 0xc1, 0x6d, 0xa4, 0x0e, 0x55, 0x94, 0x67, 0xa7, 0x87, 0x67, 0x2b, 0x37, 0x95, 
    0x9d, 0x39, 0xe9, 0x10, 0xef, 0xb0, 0x0d, 0xf0, 0x1e, 0xa5, 0xe8, 0xe9, 0x58, 0xc0, 0x43, 0xb2, 0x14, 0x51, 0x56, 0xb1, 
    0x89, 0x5f, 0xf8, 0x0e, 0xb0, 0x54, 0x3b, 0x67, 0xd3, 0x1a, 0x63, 0x4c, 0x10, 0x5d, 0xd8, 0xc9, 0x36, 0x39, 0x0d, 0x8c, 
    0x7d, 0xb5, 0x04, 0x4c, 0xb4, 0x16, 0x87, 0xee, 0xfc, 0xec, 0x22, 0xd8, 0x26, 0x91, 0x93, 0x34, 0x4d, 0x16, 0x15, 0x8f, 
    0x51, 0x7e, 0x03, 0x1d, 0x23, 0x36, 0x6a, 0xb6, 0x45, 0xb5, 0xfe, 0xfd, 0xc9, 0x31, 0x76, 0x16, 0x15, 0x74, 0x71, 0x91, 
    0x23, 0x79, 0x00, 0x48, 0xd5, 0xeb, 0xa9, 0xa8, 0xbc, 0x10, 0xc7, 0x84, 0x51, 0x63, 0x24, 0x06, 0x1b, 0x84, 0x29, 0xe3, 
    0x6a, 0xb6, 0x64, 0x8e, 0x10, 0x47, 0x76, 0xfd, 0xa7, 0x25, 0xcc, 0x4a, 0xb0, 0x6d, 0x89, 0xdb, 0xa7, 0x9f, 0xd7, 0x09, 
    0x38, 0xfe, 0x41, 0x26, 0x7f, 0xef, 0xea, 0x19, 0x02, 0xc3, 0xfe, 0xb7, 0xae, 0xa9, 0xf2, 0x15, 0xbc, 0xea, 0xab, 0xe9, 
    0xec, 0x4c, 0xf1, 0xe1, 0x82, 0xe4, 0xd3, 0xa6, 0x7d, 0xfe, 0xd3, 0xcc, 0x9c, 0xa1, 0x62, 0x23, 0xef, 0xb4, 0x1d, 0x19, 
    0xd9, 0xac, 0x79, 0xfc, 0xc3, 0x8b, 0xd9, 0xda, 0x69, 0x32, 0x6c, 0x19, 0x05, 0xf1, 0x27, 0x35, 0x36, 0x5f, 0xe2, 0xa6, 
    0xaf, 0x9e, 0xaa, 0xae, 0x68, 0xba, 0x23, 0xfc, 0x44, 0x67, 0xd6, 0x5a, 0x27, 0x01, 0xc1, 0x2a, 0xbe, 0x5c, 0xd0, 0x6d, 
    0xe2, 0xb9, 0x7d, 0x62, 0xb6, 0x27, 0xa8, 0x91, 0xe0, 0x1c, 0xbb, 0x3a, 0x26, 0xaf, 0xb7, 0x41, 0x8a, 0x37, 0x91, 0x0e, 
    0x6f, 0x48, 0xfe, 0x48, 0xff, 0x38, 0x56, 0x7c, 0x81, 0xb9, 0xc7, 0xbb, 0x07, 0x3e, 0xf1, 0xb2, 0x23, 0x46, 0xe9, 0x45, 
    0x71, 0x20, 0xea, 0xef, 0x3b, 0x55, 0xfa, 0xe4, 0xb0, 0x7a, 0xc7, 0x50, 0x39, 0x75, 0x2e, 0x0b, 0xd0, 0x83, 0xe3, 0x86, 
    0x55, 0x16, 0x1f, 0xa0, 0xe7, 0xe7, 0xe8, 0xf1, 0xc7, 0x12, 0x66, 0x30, 0xb5, 0x8a, 0x32, 0xfe, 0x4d, 0xeb, 0x6e, 0x64, 
    0x4f, 0x6c, 0x17, 0x67, 0x0a, 0xbb, 0xfa, 0x78, 0x25, 0xd1, 0xd4, 0x70, 0xb4, 0xee, 0xd3, 0x98, 0xcd, 0x98, 0xe7, 0x60, 
    0x95, 0x8c, 0x8e, 0xb3, 0xf4, 0xde, 0x5d, 0xc1, 0x38, 0xc3, 0xb4, 0x65, 0xc2, 0xbc, 0x9b, 0xb7, 0x55, 0x1d, 0x7a, 0xdb, 
    0x5f, 0x15, 0x94, 0x3b, 0x16, 0xbf, 0x73, 0x2c, 0x1d, 0x0c, 0x0c, 0x48, 0x4a, 0x49, 0xbc, 0x03, 0xbf, 0xd1, 0x82, 0x8d, 
    0xa1, 0xde, 0xab, 0xcf, 0x7c, 0xf3, 0xa2, 0xdc, 0x23, 0x6b, 0xa5, 0x7a, 0xb8, 0xd4, 0x68, 0xfb, 0x16, 0x5a, 0x70, 0x7e, 
    0xad, 0x20, 0x4b, 0x97, 0x06, 0x9e, 0x63, 0x81, 0xcd, 0x0d, 0xf0, 0xc8, 0xab, 0xd3, 0xe4, 0xea, 0xe6, 0x77, 0x12, 0x3e, 
    0x35, 0xb7, 0xa3, 0x63, 0xf3, 0x66, 0x63, 0xac, 0xf7, 0xc6, 0xd0, 0x3a, 0x90, 0x29, 0x83, 0x50, 0xea, 0xe6, 0x27, 0xe1, 
    0x0f, 0x7f, 0xc3, 0x7e, 0x5f, 0x5a, 0x8a, 0xb4, 0x36, 0x6f, 0xdc, 0xf2, 0xfa, 0xf5, 0xe9, 0xb8, 0x89, 0x8d, 0xcf, 0xac, 
    0x74, 0x9d, 0x70, 0xce, 0xb8, 0xd6, 0x96, 0x0a, 0x3a, 0xcb, 0xfd, 0xd8, 0x79, 0xf3, 0xd1, 0xe6, 0x4a, 0x42, 0x97, 0x62, 
    0x2e, 0x21, 0xe0, 0x84, 0xee, 0xe3, 0x99, 0xad, 0xd7, 0x7f, 0x51, 0xe0, 0xdd, 0x52, 0xa2, 0xbe, 0xbb, 0xc4, 0xe6, 0x70, 
    0xd9, 0xc7, 0x0e, 0x35, 0x75, 0xb5, 0x91, 0x0f, 0xce, 0x53, 0xe6, 0xff, 0x02, 0xd5, 0x76, 0xac, 0x0d, 0x7a, 0xe1, 0xe6, 
    0x1b, 0x67, 0xb4, 0x82, 0xc3, 0x75, 0x33, 0x4c, 0x56, 0xbb, 0xfe, 0x64, 0xad, 0x4d, 0xff, 0x80, 0x38, 0x00, 0x76, 0x20, 
    0x6c, 0x57, 0x36, 0x9f, 0x45, 0xc2, 0x83, 0x52, 0x84, 0xee, 0x0d, 0xdb, 0x17, 0x07, 0xbb, 0x8c, 0x1b, 0x6b, 0xfa, 0x1f, 
    0xf0, 0x94, 0xbb, 0x22, 0xe9, 0x98, 0xe0, 0x01, 0xf7, 0xfe, 0x43, 0xec, 0xa0, 0x9d, 0xa1, 0x65, 0xe0, 0x34, 0x88, 0x57, 
    0x47, 0x5e, 0xa6, 0x34, 0xa6, 0x53, 0x78, 0x52, 0xb5, 0x02, 0x5a, 0x83, 0x18, 0xae, 0x15, 0x45, 0x6e, 0x68, 0xc4, 0x72, 
    0x0d, 0xb3, 0x25, 0xd0, 0xbb, 0xf8, 0xb5, 0x35, 0x3c, 0x55, 0x6f, 0x90, 0x4b, 0xdd, 0x2c, 0xf0, 0xbe, 0xe9, 0x4d, 0x3f, 
    0x28, 0xbe, 0x83, 0x8c, 0x29, 0x62, 0x28, 0x71, 0x05, 0x20, 0x9b, 0x5a, 0xbe, 0xaf, 0x8a, 0x30, 0x54, 0xa8, 0x63, 0xd5, 
    0x83, 0x99, 0xe7, 0xa8, 0xe7, 0x8a, 0xf9, 0x4f, 0x69, 0xa8, 0x1e, 0x5a, 0x2e, 0x19, 0x7a, 0x14, 0xb4, 0x9e, 0x59, 0xc7, 
    0x01, 0x14, 0x3f, 0x9c, 0x0f, 0x03, 0xa0, 0x21, 0xb1, 0x5c, 0x2d, 0x5c, 0xc5, 0x88, 0x74, 0xd0, 0xb7, 0x80, 0xca, 0x51, 
    0xe0, 0xfe, 0x07, 0x0e, 0x15, 0x3e, 0xce, 0x99, 0x68, 0x89, 0x9f, 0xbc, 0x3b, 0xc4, 0x84, 0xdf, 0xeb, 0xc9, 0x4d, 0x1b, 
    0x19, 0xe6, 0xc4, 0xfc, 0x83, 0x1c, 0x05, 0x18, 0x3a, 0x0d, 0xb8, 0x10, 0xbd, 0x26, 0xc2, 0xbd, 0xa2, 0xd2, 0x79, 0x46, 
    0x4d, 0x91, 0x1b, 0xc3, 0x8b, 0x2f, 0xd3, 0x67, 0x5d, 0x74, 0xa1, 0x89, 0x8f, 0x96, 0xb2, 0x76, 0xd8, 0x68, 0xa3, 0xa6, 
    0x2a, 0xf1, 0xf2, 0x34, 0xf5, 0x67, 0x7e, 0xfe, 0x6b, 0xe5, 0xe3, 0x24, 0x09, 0x44, 0xd3, 0xcc, 0x59, 0xc2, 0x82, 0xa0, 
    0x56, 0xde, 0x09, 0xb1, 0x7d, 0xf3, 0xb1, 0x2b, 0x3e, 0xac, 0xac, 0xce, 0xec, 0x88, 0x7f, 0xbc, 0x67, 0x08, 0x5c, 0x6b, 
    0x7a, 0x09, 0x42, 0x16, 0x45, 0x52, 0xe5, 0x99, 0x7c, 0x74, 0x03, 0xa7, 0xc1, 0xaa, 0x25, 0x92, 0xd3, 0x46, 0x27, 0xff, 
    0x0c, 0x4e, 0x81, 0x2e, 0x7d, 0xd3, 0xc3, 0xe3, 0xae, 0x4c, 0x17, 0xbc, 0xd1, 0xe0, 0xbe, 0x44, 0xd1, 0x19, 0x4f, 0x00, 
    0x29, 0xda, 0xf2, 0x8e, 0x73, 0xc3, 0xcd, 0x55, 0x8a, 0x6e, 0xfb, 0xc3, 0x04, 0xcf, 0xd2, 0x7a, 0xd2, 0x5b, 0x24, 0xb9, 
    0x38, 0x34, 0xb5, 0x76, 0xed, 0xe9, 0x89, 0xf0, 0x7c, 0x44, 0x69, 0x9d, 0xc8, 0x98, 0x05, 0x28, 0xfe, 0x04, 0x14, 0xf5, 
    0x28, 0x7c, 0xe2, 0xea, 0xc9, 0x5d, 0x34, 0xfa, 0x03, 0xcd, 0x0a, 0x51, 0xdf, 0x6f, 0x2e, 0xb7, 0x15, 0x00, 0x5c, 0xed, 
    0xe1, 0xe0, 0x04, 0x16, 0xf3, 0xe4, 0x9e, 0xa7, 0xdc, 0xc7, 0x7e, 0x7b, 0x31, 0xc5, 0xb8, 0xce, 0x78, 0xc2, 0xbe, 0x24, 
    0xbc, 0xe0, 0x92, 0xf2, 0x19, 0x0b, 0xa0, 0x88, 0xbd, 0xa9, 0xdf, 0x6b, 0xdc, 0x86, 0x72, 0x19, 0x49, 0xb1, 0x8d, 0xb8, 
    0xa6, 0xfc, 0xe9, 0x19, 0x40, 0x06, 0x2f, 0x17, 0xe6, 0xf7, 0x09, 0x8a, 0x63, 0xbe, 0x0f, 0x04, 0xb8, 0x5f, 0xe9, 0x5d, 
    0xa3, 0x94, 0xcf, 0x89, 0x5d, 0x51, 0xb9, 0x10, 0x9b, 0xf3, 0xb0, 0x55, 0x1b, 0x7b, 0xce, 0x91, 0xe6, 0xd3, 0x90, 0x7a, 
    0x0b, 0xb5, 0x5f, 0x6f, 0xa7, 0x4f, 0xbf, 0xc6, 0x75, 0xb4, 0x23, 0xf9, 0xcc, 0x4b, 0x35, 0x57, 0xf8, 0xf4, 0xfb, 0x6d, 
    0xd8, 0xed, 0x11, 0x47, 0x9f, 0xf6, 0xf1, 0xda, 0x84, 0x94, 0xd9, 0x23, 0x50, 0xb5, 0xff, 0x35, 0x78, 0x62, 0x3e, 0x9a, 
    0x5c, 0x8b, 0xbe, 0xa5, 0x4e, 0x73, 0x8b, 0xf1, 0xd6, 0x54, 0xdc, 0xf5, 0x73, 0x1b, 0x27, 0xcf, 0x01, 0x0e, 0x12, 0xb6, 
    0x0b, 0xb7, 0x94, 0x68, 0xcc, 0xe2, 0x4c, 0xe6, 0x30, 0x76, 0xbe, 0x71, 0x03, 0xd2, 0x06, 0x27, 0xff, 0x3e, 0xa4, 0x31, 
    0xbe, 0x75, 0x28, 0xab, 0x6e, 0x65, 0x97, 0xaa, 0x8c, 0xbe, 0xfc, 0xd8, 0x9f, 0xb1, 0xd0, 0x02, 0x2f, 0x96, 0xf2, 0x53, 
    0x29, 0xd7, 0xf1, 0x1a, 0xcb, 0x66, 0xe6, 0xf1, 0x84, 0x61, 0xab, 0x21, 0xe7, 0x91, 0x93, 0xae, 0xb6, 0xff, 0x4e, 0xcc, 
    0xd1, 0x5a, 0x69, 0x12, 0x26, 0xcc, 0x63, 0x7d, 0x66, 0xdc, 0x5b, 0x1b, 0x12, 0xd4, 0x66, 0x41, 0x8b, 0x65, 0xc8, 0x16, 
    0x13, 0x97, 0x67, 0x95, 0xa5, 0x56, 0x4e, 0xa7, 0x91, 0x63, 0x53, 0x1b, 0x48, 0x68, 0x4e, 0xe5, 0xa8, 0x69, 0xb9, 0x7b, 
    0x10, 0x1f, 0x39, 0x8a, 0x19, 0xfb, 0x80, 0xe9, 0x75, 0x79, 0x9e, 0x40, 0x47, 0x98, 0x2b, 0x3a, 0xaa, 0xb3, 0xe6, 0x74, 
    0x60, 0x5e, 0x66, 0xa4, 0xd9, 0x17, 0x0c, 0xd4, 0xa7, 0x6d, 0x4d, 0xac, 0xc2, 0x74, 0x1d, 0xb1, 0x99, 0xed, 0xce, 0x5a, 
    0x75, 0x06, 0x85, 0xbd, 0x09, 0x46, 0xda, 0x13, 0x24, 0xf2, 0x86, 0x1b, 0x69, 0x37, 0xc3, 0x6b, 0x5b, 0x84, 0xbe, 0x57, 
    0x10, 0xc6, 0x19, 0x8e, 0xc1, 0x29, 0xab, 0x91, 0x05, 0x5c, 0xa5, 0x45, 0x40, 0x3d, 0x66, 0x08, 0x72, 0x52, 0xea, 0x31, 
    0xfc, 0xbe, 0x13, 0xd3, 0x1e, 0x06, 0xb8, 0xf2, 0x19, 0x39, 0x44, 0x5f, 0xd6, 0x99, 0x3b, 0x67, 0x71, 0xe8, 0x43, 0xfb, 
    0x76, 0xe3, 0x4c, 0xa9, 0x01, 0xa3, 0xd6, 0x38, 0xc2, 0xb3, 0x5d, 0x8e, 0x75, 0x63, 0x87, 0x97, 0x27, 0x7b, 0x4f, 0x7a, 
    0x62, 0x42, 0x8a, 0x44, 0x6e, 0x35, 0x40, 0xc6, 0xae, 0xa0, 0x37, 0xeb, 0x62, 0xa3, 0x24, 0xc9, 0x88, 0x54, 0xa9, 0x6a, 
    0xc0, 0x91, 0xf6, 0x68, 0x59, 0x14, 0xda, 0x2f, 0xf4, 0x4d, 0x69, 0x14, 0x11, 0xb8, 0x48, 0xec, 0xba, 0xbb, 0x64, 0xb5, 
    0xa0, 0x61, 0xb9, 0x45, 0x1e, 0xd2, 0x62, 0xae, 0x4e, 0xdc, 0x99, 0x66, 0x32, 0xa8, 0x91, 0x58, 0x53, 0xde, 0x5b, 0x90, 
    0xbe, 0x12, 0xd8, 0x8c, 0x99, 0x84, 0x04, 0x3f, 0x7e, 0xa0, 0xa4, 0x0b, 0x7d, 0x7d, 0xe2, 0xd7, 0x7c, 0xa6, 0x20, 0x27, 
    0x84, 0xbc, 0xe8, 0xf3, 0x94, 0xd2, 0x6d, 0x60, 0x84, 0x35, 0x69, 0xd8, 0xd9, 0xef, 0x62, 0xcc, 0xd7, 0xc9, 0x81, 0xcf, 
    0xa0, 0xa0, 0x0c, 0x58, 0xf5, 0xd1, 0x93, 0x36, 0xa3, 0xff, 0xc3, 0xf3, 0xb4, 0x7b, 0x7b, 0x7e, 0xaf, 0xcc, 0x3e, 0x4a, 
    0x73, 0xa6, 0xae, 0x00, 0x33, 0xee, 0x3d, 0x88, 0xc5, 0x5c, 0xfe, 0xca, 0x0f, 0xce, 0x26, 0x02, 0x81, 0x9d, 0x4a, 0x87, 
    0x05, 0xad, 0x58, 0xd7, 0xc3, 0xba, 0x39, 0xa6, 0x3a, 0xac, 0x13, 0x7c, 0x0a, 0x07, 0x0f, 0xef, 0x00, 0x4a, 0x61, 0xfd, 
    0x53, 0xc5, 0x02, 0xb5, 0x6a, 0x5a, 0x22, 0x62, 0x31, 0x17, 0xe6, 0x8f, 0x6f, 0x2d, 0x6c, 0xa3, 0x5e, 0xe4, 0x1f, 0x75, 
    0x08, 0x20, 0x4f, 0x22, 0xf6, 0x29, 0xe7, 0x38, 0x85, 0x7a, 0x98, 0x54, 0x31, 0x4a, 0x04, 0x48, 0x4f, 0xb9, 0x8c, 0x08, 
    0x34, 0x45, 0x06, 0x51, 0x5c, 0x90, 0xeb, 0x0f, 0x8e, 0x5e, 0x13, 0x29, 0x0d, 0x36, 0xfb, 0x08, 0xe6, 0x46, 0x52, 0x8d, 
    0x7f, 0x91, 0x4b, 0x49, 0x46, 0xd8, 0xcb, 0x56, 0x8a, 0x27, 0x98, 0x7b, 0x40, 0x52, 0x66, 0xda, 0xac, 0xc4, 0xb9, 0xba, 
    0xea, 0xd9, 0x1e, 0x19, 0xcc, 0x94, 0x6f, 0x23, 0xe8, 0x09, 0xa8, 0xda, 0xdb, 0x99, 0xfb, 0xb6, 0x4b, 0x11, 0xa4, 0x41, 
    0x1a, 0x55, 0xdf, 0x4b, 0x83, 0x9f, 0x99, 0xd1, 0xbd, 0x85, 0x86, 0x54, 0xa1, 0x85, 0xaa, 0x71, 0x74, 0xfb, 0x19, 0xe5, 
    0x64, 0xd0, 0xf9, 0x5b, 0x34, 0xc1, 0x18, 0x00, 0x8a, 0x54, 0x1e, 0xf9, 0x63, 0xd3, 0x34, 0xc3, 0x67, 0x30, 0x56, 0x94, 
    0x99, 0x5c, 0x1b, 0x42, 0xd3, 0xde, 0xd5, 0xd7, 0x1c, 0x42, 0xb8, 0xe9, 0xc5, 0x34, 0xbf, 0x53, 0x46, 0xc7, 0xec, 0xfd, 
    0xdd, 0xaf, 0xe8, 0x23, 0x85, 0xfa, 0x94, 0x05, 0x5a, 0x35, 0x72, 0x46, 0xb3, 0x5c, 0x94, 0x6c, 0x09, 0x6a, 0xc6, 0x64, 
    0xc2, 0x29, 0xc6, 0xa9, 0x5a, 0xb7, 0xa5, 0xf3, 0x5c, 0xdc, 0x39, 0x25, 0xa2, 0x8e, 0xfe, 0x27, 0xab, 0x8a, 0x21, 0xc3, 
    0xc5, 0x5c, 0x38, 0x5e, 0x97, 0xe2, 0x06, 0x0f, 0x73, 0x50, 0x71, 0xfb, 0x89, 0xe6, 0xe8, 0x27, 0xc2, 0xc9, 0x58, 0x62, 
    0x80, 0xdb, 0x66, 0xf6, 0xfe, 0xa5, 0x0d, 0x6a, 0xd2, 0x68, 0x88, 0x04, 0x22, 0xea, 0xe5, 0x12, 0x3a, 0x8a, 0xd4, 0x86, 
    0xbb, 0xc5, 0xf9, 0x3f, 0xd0, 0x64, 0x31, 0x1e, 0x5f, 0x6e, 0xd4, 0x46, 0x44, 0x49, 0x52, 0x06, 0x76, 0xc6, 0x82, 0x51, 
    0x52, 0xc8, 0xab, 0xc9, 0x31, 0x58, 0x8e, 0x7c, 0xd8, 0x0d, 0x46, 0x04, 0x66, 0xff, 0x23, 0xd8, 0xda, 0x79, 0x5c, 0x94, 
    0xc6, 0xce, 0x0b, 0xb3, 0x25, 0xe6, 0xb0, 0xe1, 0x61, 0x81, 0x41, 0x7f, 0x4f, 0x13, 0x4d, 0x63, 0xef, 0xba, 0xad, 0xbe, 
    0x4b, 0xdc, 0x48, 0x21, 0x0d, 0x34, 0xea, 0x99, 0x82, 0xb3, 0x73, 0xea, 0x67, 0xa4, 0x56, 0xd8, 0x88, 0xbb, 0xec, 0xaf, 
    0xe7, 0x4c, 0xf9, 0x91, 0x69, 0x47, 0x04, 0x76, 0x58, 0x03, 0xa4, 0xe1, 0xcc, 0xb2, 0x72, 0xf8, 0x1a, 0xe4, 0x5f, 0x84, 
    0x2d, 0x45, 0xc7, 0x47, 0xc5, 0x3e, 0x6a, 0xd6, 0xb7, 0xff, 0x03, 0x29, 0x2f, 0x52, 0x7d, 0xed, 0x7d, 0xae, 0x60, 0xec, 
    0xc2, 0x71, 0x15, 0x01, 0x55, 0x38, 0x17, 0x4f, 0x80, 0xd2, 0x1c, 0xc5, 0xe0, 0xf4, 0xee, 0x02, 0x53, 0xac, 0x49, 0xc4, 
    0x49, 0x9d, 0x8f, 0x3d, 0xf3, 0x3e, 0x6b, 0x73, 0xab, 0xbb, 0xa9, 0x7e, 0xb3, 0xa6, 0x93, 0x38, 0xb2, 0x24, 0xd8, 0x74, 
    0xb5, 0x7a, 0xd4, 0x97, 0xf4, 0xd7, 0x48, 0xa0, 0x21, 0xbc, 0x7d, 0x0b, 0xb7, 0xe2, 0x81, 0x5a, 0x63, 0xa6, 0x69, 0xd3, 
    0x9e, 0xb3, 0xf4, 0x3a, 0x7e, 0x80, 0xc0, 0xed, 0xc5, 0x53, 0x0f, 0xc3, 0x02, 0x30, 0xe2, 0xbc, 0xbc, 0xa1, 0xb7, 0xca, 
    0xe5, 0xb4, 0x83, 0x5d, 0xe6, 0x34, 0x8d, 0x74, 0xa6, 0x0c, 0x86, 0x24, 0x90, 0x27, 0x3b, 0x73, 0x39, 0x5d, 0x55, 0x07, 
    0xd3, 0xae, 0x9f, 0xaf, 0x3d, 0x46, 0x1b, 0xfb, 0x2f, 0xdd, 0x56, 0xb7, 0xdc, 0x08, 0x48, 0x7f, 0x8e, 0x08, 0x11, 0x1a, 
    0xe7, 0x80, 0x7f, 0xaa, 0x02, 0x49, 0xb4, 0x37, 0x9d, 0x24, 0x76, 0x99, 0xc1, 0xd5, 0x0e, 0xcf, 0x6d, 0x97, 0x41, 0x57, 
    0x02, 0xa5, 0xc4, 0x80, 0xdc, 0x8a, 0xc4, 0xcf, 0x81, 0x17, 0xd9, 0xed, 0x1b, 0xb7, 0x7d, 0x09, 0x9b, 0x66, 0x7e, 0xc2, 
    0xfc, 0x42, 0xd9, 0x2d, 0x65, 0xca, 0xf9, 0xf2, 0x6a, 0xfd, 0x75, 0x85, 0xe7, 0xaf, 0x89, 0x18, 0x69, 0xe7, 0x21, 0x2c, 
    0x2a, 0xb3, 0x83, 0xdf, 0x8a, 0x0e, 0x31, 0x84, 0xdc, 0x29, 0xfc, 0xc8, 0x62, 0xbd, 0x44, 0xb4, 0x4e, 0xac, 0x27, 0x59, 
    0x6a, 0xe8, 0xaa, 0x68, 0x84, 0x29, 0xe1, 0x73, 0x8d, 0x9c, 0x32, 0x8b, 0x28, 0x62, 0x2c, 0xb2, 0x6c, 0x9f, 0x14, 0x70, 
    0x74, 0xcb, 0x48, 0x7d, 0x5d, 0x41, 0x0a, 0x49, 0xe1, 0xbb, 0x8d, 0x32, 0x27, 0x47, 0x5d, 0x7c, 0xb3, 0xe0, 0x91, 0x8d, 
    0xdc, 0x84, 0x47, 0x58, 0xe7, 0x9c, 0x35, 0x92, 0x5b, 0xc2, 0xba, 0x11, 0xfa, 0xf8, 0x8c, 0x5f, 0x0a, 0x78, 0xc5, 0x63, 
    0x1a, 0xa4, 0xc3, 0xd8, 0x41, 0x7a, 0x5b, 0xa2, 0xe3, 0x02, 0x57, 0x04, 0x34, 0xf3, 0x2e, 0x03, 0x0e, 0xf6, 0x18, 0xd3, 
    0xe1, 0x04, 0x80, 0x5b, 0xfe, 0x63, 0x48, 0x63, 0x66, 0xbb, 0xb9, 0x2d, 0xf9, 0x96, 0x0c, 0xcc, 0x04, 0x3a, 0x77, 0x56, 
    0x57, 0x05, 0xaf, 0xc6, 0x0c, 0x8d, 0xc9, 0xdb, 0xd8, 0x71, 0x80, 0x1f, 0xa5, 0x7d, 0xc3, 0x23, 0x58, 0xba, 0x9d, 0xf4, 
    0xc5, 0xc7, 0x4b, 0x45, 0x1f, 0xad, 0x37, 0x78, 0x8b, 0xe2, 0x42, 0xea, 0xf9, 0xc5, 0x84, 0xd1, 0x21, 0x94, 0x82, 0xb9, 
    0x40, 0x5e, 0x40, 0xd3, 0x7b, 0xc5, 0x59, 0x06, 0x23, 0x43, 0x73, 0xb9, 0xdf, 0xd5, 0xcd, 0xea, 0x62, 0xa7, 0x4e, 0xc8, 
    0xaa, 0xf3, 0x52, 0xb5, 0x33, 0xd7, 0x07, 0x20, 0x0c, 0x93, 0xbd, 0x1f, 0x8c, 0x12, 0xd3, 0x1c, 0xcf, 0xb5, 0x10, 0xfd, 
    0x92, 0x1c, 0xd7, 0xcf, 0x23, 0xfb, 0xc2, 0x5a, 0x97, 0x6f, 0x5c, 0xa8, 0xf2, 0x84, 0x8a, 0x62, 0x26, 0x24, 0x90, 0x26, 
    0xe0, 0x61, 0x85, 0xde, 0x94, 0x22, 0xc3, 0x12, 0xdb, 0xa9, 0x4e, 0x22, 0xb2, 0xa3, 0xc9, 0x41, 0xd9, 0x8f, 0x3c, 0xe3, 
    0xe8, 0xc1, 0x67, 0x3d, 0x16, 0xca, 0x8c, 0x69, 0x26, 0xf1, 0xa6, 0xd7, 0xa9, 0xd7, 0x82, 0x47, 0xa7, 0x06, 0xd7, 0x80, 
    0xd4, 0x6f, 0xd9, 0x9d, 0xa8, 0xf9, 0x33, 0xa3, 0x2d, 0x53, 0x12, 0xac, 0x2b, 0xe3, 0x5d, 0xd6, 0xf4, 0x38, 0x87, 0xde, 
    0x77, 0xbb, 0x08, 0xe3, 0xc9, 0x00, 0x5e, 0x15, 0x56, 0x34, 0x8e, 0x25, 0x13, 0x70, 0x02, 0x52, 0x8f, 0x3d, 0x55, 0xbc, 
    0x0f, 0x5a, 0x6c, 0xa0, 0xe6, 0xf6, 0xe0, 0xa6, 0xfc, 0xb4, 0xfe, 0x0e, 0xf7, 0x16, 0x13, 0xf9, 0x37, 0x68, 0x6d, 0xad, 
    0x0e, 0xa0, 0x04, 0xfd, 0x3a, 0x3d, 0x48, 0x50, 0x95, 0x2c, 0xf0, 0x07, 0xb5, 0x4f, 0xa9, 0x7d, 0x20, 0xf1, 0x29, 0x0f, 
    0x06, 0x69, 0x0b, 0x82, 0xd8, 0xd9, 0x7f, 0x33, 0x58, 0x36, 0xc6, 0xeb, 0x59, 0x3c, 0x5d, 0x6f, 0x64, 0xa5, 0x2c, 0x47, 
    0xe3, 0xcf, 0x20, 0x4a, 0x7b, 0x4a, 0x02, 0x25, 0x61, 0xeb, 0x1a, 0xbe, 0x65, 0xf3, 0x0d, 0x95, 0x54, 0x60, 0xa4, 0x23, 
    0x17, 0xc8, 0xc6, 0x8c, 0xe2, 0xa8, 0x7a, 0x7f, 0x2e, 0xff, 0x54, 0xdd, 0x90, 0xcc, 0x29, 0xe4, 0xf6, 0x14, 0x3e, 0xff, 
    0xf6, 0x7d, 0x5f, 0xc4, 0x2a, 0x67, 0x37, 0x29, 0xd2, 0x2f, 0x8b, 0xa9, 0x45, 0x01, 0xd4, 0xa3, 0xc1, 0xdc, 0xd6, 0x06, 
    0x6b, 0x05, 0x83, 0xf8, 0x66, 0x1b, 0x4d, 0xd8, 0x71, 0xa4, 0xdb, 0x20, 0x48, 0x8d, 0xbf, 0x50, 0x34, 0x1f, 0x7d, 0x23, 
    0x85, 0x66, 0x47, 0xf1, 0xca, 0x09, 0xef, 0x96, 0xb3, 0xbe, 0x57, 0x7d, 0x6c, 0x36, 0x77, 0x16, 0x97, 0xd7, 0xf2, 0x6b, 
    0xbb, 0x06, 0xba, 0x70, 0x66, 0x08, 0x51, 0xa9, 0xc8, 0xe0, 0x84, 0xfb, 0x9b, 0xa6, 0x1c, 0xf0, 0x99, 0x95, 0x4d, 0xad, 
    0x93, 0xfb, 0xe3, 0x91, 0xfb, 0xd3, 0x69, 0x33, 0xd4, 0xbd, 0xb0, 0x90, 0x6f, 0x76, 0x68, 0xe9, 0x51, 0x7e, 0x17, 0x6f, 
    0x4c, 0x5f, 0xb1, 0xdf, 0x36, 0xa7, 0xa7, 0x38, 0x9f, 0x33, 0x9b, 0x32, 0x73, 0xff, 0x31, 0x65, 0x4c, 0xb1, 0xf8, 0xe9, 
    0x82, 0x47, 0x0b, 0x16, 0xd2, 0xd4, 0x55, 0xce, 0x4b, 0xfb, 0xc9, 0x52, 0x2f, 0xf7, 0x55, 0xbc, 0x4b, 0xf7, 0x7a, 0xc5, 
    0x1e, 0xaa, 0x37, 0xf9, 0x3b, 0x6c, 0x1b, 0x5a, 0x1d, 0x0c, 0x14, 0x43, 0x16, 0x79, 0x7d, 0xcf, 0x1e, 0xcd, 0x40, 0x78, 
    0x91, 0x35, 0x0a, 0x3e, 0x5e, 0xc0, 0xe2, 0x28, 0xa0, 0x06, 0x31, 0x66, 0xa7, 0xcf, 0x78, 0xaf, 0xc5, 0xf0, 0x12, 0xc0, 
    0x0c, 0x78, 0x22, 0x8e, 0xe8, 0xf3, 0xb2, 0x5d, 0x2e, 0x87, 0x1c, 0xea, 0x04, 0x95, 0x71, 0xd7, 0xec, 0x99, 0xe6, 0x3b, 
    0xaa, 0xb4, 0x9a, 0x4e, 0xd7, 0x95, 0x2f, 0x00, 0x33, 0xe5, 0x2c, 0x26, 0x22, 0x9e, 0x43, 0x09, 0x2d, 0x76, 0xa8, 0x19, 
    0xec, 0x72, 0x5c, 0xdc, 0x2e, 0x51, 0xcc, 0x17, 0xea, 0x72, 0x2c, 0xe2, 0xe0, 0xed, 0x1b, 0x8f, 0xfd, 0x4f, 0xe4, 0x3a, 
    0x1f, 0x50, 0xf8, 0xd3, 0xaf, 0x9a, 0x60, 0xf4, 0x0b, 0xcf, 0xf4, 0xd0, 0xf2, 0xb4, 0x70, 0xa7, 0x87, 0x93, 0xd5, 0xc7, 
    0x17, 0x99, 0xd4, 0x79, 0x6f, 0xd3, 0xa7, 0x85, 0x92, 0x3b, 0x08, 0x84, 0x95, 0xec, 0x79, 0x11, 0x77, 0x3d, 0xd2, 0x64, 
    0xef, 0x51, 0x60, 0x95, 0x9a, 0x57, 0xa6, 0x34, 0xb3, 0xe3, 0xc3, 0x20, 0x75, 0xa4, 0x8f, 0x4f, 0xfe, 0xb3, 0x1c, 0xd3, 
    0x60, 0x33, 0x15, 0x0f, 0x97, 0x27, 0x56, 0x41, 0x36, 0xe7, 0x8f, 0x0f, 0xff, 0xdd, 0xb3, 0x87, 0xc4, 0x6e, 0x28, 0xa2, 
    0x52, 0xc9, 0x60, 0x27, 0x90, 0xe7, 0xa6, 0xc9, 0xe2, 0xb0, 0x92, 0x16, 0x91, 0x8c, 0xa2, 0x78, 0xb2, 0x7c, 0x9b, 0x05, 
    0x93, 0x41, 0x27, 0xef, 0xc1, 0x37, 0x52, 0x39, 0xba, 0x3f, 0xa3, 0x0e, 0xf4, 0x8e, 0x65, 0x66, 0xae, 0x85, 0x88, 0xf7, 
    0x36, 0x1f, 0x8e, 0xbe, 0xbc, 0xa4, 0x1b, 0xa3, 0x0a, 0x6e, 0x3c, 0x1b, 0x25, 0x82, 0x0e, 0xf6, 0x29, 0x38, 0x65, 0xa1, 
    0xac, 0x3b, 0x04, 0xbd, 0xad, 0x2a, 0x8b, 0x7b, 0x3a, 0x9e, 0x24, 0x82, 0xd9, 0x62, 0xfb, 0x1a, 0x59, 0x97, 0xc1, 0x70, 
    0xc2, 0x01, 0x48, 0x8a, 0x1a, 0xf5, 0x07, 0xc0, 0xa6, 0x83, 0xe3, 0xb0, 0x8d, 0xfa, 0x39, 0x8d, 0x6e, 0x20, 0x78, 0xe0, 
    0xfc, 0x4a, 0x54, 0x8c, 0x29, 0x09, 0x2f, 0x50, 0x4e, 0xdf, 0x61, 0xce, 0x4a, 0x4e, 0xde, 0x70, 0x5f, 0x38, 0x95, 0xf1, 
    0x58, 0x58, 0x82, 0x19, 0xbe, 0x3f, 0x6a, 0xa3, 0xa8, 0x9d, 0xf6, 0x6b, 0x66, 0x33, 0x08, 0x17, 0x38, 0x80, 0xce, 0xa3, 
    0xd2, 0xb3, 0xc6, 0x1d, 0xc8, 0x57, 0xab, 0x84, 0x89, 0xef, 0x81, 0x1e, 0xe0, 0x2c, 0x20, 0x26, 0x0e, 0x77, 0xa3, 0xf1, 
    0x48, 0xf0, 0x81, 0x83, 0x2d, 0x8b, 0xab, 0xd9, 0xc8, 0x1a, 0x6a, 0xcd, 0x54, 0xe9, 0xd8, 0xb5, 0x2e, 0x72, 0xb3, 0xad, 
    0xd6, 0xb8, 0x97, 0xbb, 0x6b, 0xf7, 0xc5, 0x1a, 0x8f, 0xec, 0x04, 0x3f, 0xdb, 0xdb, 0x48, 0x39, 0x8e, 0x3c, 0x83, 0xef, 
    0x90, 0x9f, 0x89, 0x97, 0x36, 0x00, 0xe2, 0xc6, 0xf7, 0x68, 0xb6, 0xd9, 0x46, 0xbe, 0x18, 0xb1, 0x28, 0x44, 0xaf, 0xc2, 
    0x43, 0x53, 0x28, 0x27, 0x16, 0xe4, 0x41, 0xa8, 0xcb, 0xad, 0xe5, 0x0d, 0xbc, 0x9e, 0x34, 0x56, 0x89, 0x70, 0x61, 0xea, 
    0xb8, 0x2e, 0xd7, 0xab, 0x72, 0xf9, 0x3c, 0xfb, 0x84, 0xba, 0xcd, 0x63, 0xe4, 0xc6, 0xb6, 0xe6, 0x19, 0x7f, 0x63, 0xec, 
    0x77, 0x85, 0xa4, 0x41, 0x6d, 0x00, 0xcb, 0x82, 0xe0, 0xdc, 0xf6, 0x39, 0x41, 0x87, 0x38, 0x01, 0x76, 0x5b, 0xdd, 0x1b, 
    0x5b, 0x1f, 0x9d, 0x22, 0x19, 0xdd, 0xb8, 0x0a, 0x01, 0xfd, 0x37, 0xc3, 0xb9, 0x20, 0x0d, 0x30, 0x50, 0x26, 0x72, 0x6d, 
    0x67, 0x3b, 0x6b, 0x99, 0x74, 0x03, 0xac, 0x8e, 0x6d, 0xa0, 0x91, 0xb2, 0x9e, 0x9c, 0x86, 0x8a, 0xe4, 0xcc, 0x9c, 0x34, 
    0xae, 0xc9, 0xc4, 0x1f, 0x94, 0xa7, 0x72, 0xdd, 0x86, 0x9a, 0x7e, 0x66, 0xa4, 0x70, 0xf3, 0xce, 0x47, 0x0f, 0x65, 0x28, 
    0x43, 0x80, 0x81, 0x51, 0x76, 0xd6, 0x78, 0x27, 0x89, 0x2c, 0xa4, 0x2d, 0xc9, 0xb3, 0x76, 0xe1, 0x2f, 0x56, 0xde, 0x62, 
    0x0c, 0x7d, 0xb9, 0xda, 0x4b, 0x11, 0x6e, 0x4e, 0xbf, 0x6f, 0x05, 0xab, 0x0e, 0x56, 0xad, 0xd1, 0x39, 0xa0, 0x34, 0x07, 
    0x46, 0x67, 0x89, 0x4e, 0x68, 0xa5, 0xb2, 0x17, 0x03, 0xcd, 0x45, 0x4a, 0xda, 0xf8, 0x6e, 0xac, 0x29, 0x70, 0xec, 0x06, 
    0x24, 0x1c, 0xb8, 0x48, 0xe9, 0x1d, 0xe7, 0x76, 0x6a, 0x6e, 0xb5, 0xf3, 0x37, 0x52, 0xa0, 0x8b, 0x6f, 0xc1, 0xaf, 0x54, 
    0x77, 0x70, 0x62, 0x68, 0xe2, 0x26, 0x5a, 0x50, 0x68, 0x6c, 0x62, 0x13, 0xfc, 0x66, 0xd1, 0xd8, 0x73, 0x34, 0x51, 0x13, 
    0x07, 0xba, 0xe4, 0xab, 0x8f, 0x58, 0xf1, 0x6d, 0x97, 0x56, 0xab, 0x3c, 0x68, 0x9a, 0xbb, 0x6f, 0xbc, 0x78, 0x08, 0x46, 
    0x49, 0x6a, 0x62, 0x00, 0xbc, 0xb9, 0xa6, 0x9a, 0x95, 0xe4, 0x0b, 0x34, 0xf7, 0x09, 0x55, 0xfa, 0x2b, 0x0b, 0xec, 0x6a, 
    0x30, 0x23, 0x4f, 0x55, 0x7c, 0x30, 0x52, 0xf8, 0xb2, 0xb2, 0xd8, 0xe3, 0x0a, 0x04, 0x67, 0x3d, 0x21, 0xbb, 0xab, 0xda, 
    0xe5, 0x28, 0xc9, 0x51, 0x2a, 0x6e, 0xa3, 0x0a, 0x37, 0x1b, 0x39, 0x98, 0x22, 0x06, 0x9c, 0x1e, 0x09, 0xe6, 0xd9, 0x08, 
    0x5d, 0x17, 0xf7, 0x3a, 0x62, 0x85, 0x6d, 0x81, 0xca, 0xb0, 0xce, 0x34, 0x00, 0x84, 0xdd, 0xfc, 0xa2, 0x29, 0xdc, 0x09, 
    0x3a, 0xc2, 0xd8, 0xd2, 0x6a, 0x85, 0x60, 0xdc, 0x4f, 0x36, 0xb3, 0x47, 0xb8, 0x9a, 0xbb, 0x7d, 0x2f, 0xf7, 0x14, 0x80, 
    0xca, 0x89, 0xba, 0xa2, 0x0b, 0xee, 0xf8, 0xc8, 0x41, 0x94, 0x61, 0x10, 0x69, 0x8b, 0xea, 0xa5, 0xff, 0x03, 0x7e, 0xf4, 
    0x9f, 0x77, 0x31, 0x85, 0xa1, 0x65, 0xc6, 0x08, 0x3d, 0x14, 0x37, 0x85, 0x8d, 0x5d, 0x0d, 0xbd, 0xc1, 0x12, 0xec, 0x08};

// SHA1 hash of the data
const char *testData2_hash = "c11b6385aed1547766e51e704eb23921b22b589b";

const char *testPath = "/usr/uploadTest";
SequentialFile testFiles;

void publishData1();
void publishData2();
void publishDataRandom(int numBytes);
void completionHandler(const FileUploadRK::UploadQueueEntry *queueEntry);

int testHandler(String cmd);


void setup() {
    Particle.function("test", testHandler);

    unlink("/usr/fileTest1"); // TEMPORARY

    testFiles
        .withDirPath(testPath)
        .scanDir();


    testFiles.removeAll(false);

    FileUploadRK::instance().withCompletionHandler(completionHandler);

    FileUploadRK::instance().setup();

}


void loop() {
    FileUploadRK::instance().loop();


    if (Particle.connected()) {
        if (lastPublish == 0 || millis() - lastPublish >= publishPeriod.count()) {
            lastPublish = millis();
            // publishData1();
        }
    }

}


void publishData1() {
    String testPath = testFiles.getPathForFileNum(testFiles.reserveFile());
    
    int fd = open(testPath, O_RDWR | O_CREAT | O_TRUNC);
    if (fd != -1) {
        write(fd, testData1, sizeof(testData1));
        close(fd);
    }

    FileUploadRK::instance().queueFileToUpload(testPath);
}

void publishData2() {
    String testPath = testFiles.getPathForFileNum(testFiles.reserveFile());
    
    int fd = open(testPath, O_RDWR | O_CREAT | O_TRUNC);
    if (fd != -1) {
        write(fd, testData2, sizeof(testData2));
        close(fd);
    }

    FileUploadRK::instance().queueFileToUpload(testPath);
}

void publishDataRandom(int numBytes) {
    String testPath = testFiles.getPathForFileNum(testFiles.reserveFile());

    int fd = open(testPath, O_RDWR | O_CREAT | O_TRUNC);
    if (fd != -1) {
        uint8_t buf[256];
        int offset = 0;
        while(offset < numBytes) {
            int count = numBytes - offset;
            if (count > (int)sizeof(buf)) {
                count = sizeof(buf);
            } 
            for(int ii = 0; ii < count; ii++) {
                buf[ii] = (uint8_t) rand();
            }
            write(fd, buf, count);

            offset += count;
        }

        close(fd);
    }

    Variant meta;
    meta.set("numBytes", numBytes);
    meta.set("path", testPath.c_str());

    FileUploadRK::instance().queueFileToUpload(testPath, meta);

}

void completionHandler(const FileUploadRK::UploadQueueEntry *queueEntry) {
    Log.info("file sent %s", queueEntry->path.c_str());

    unlink(queueEntry->path.c_str());
}


int testHandler(String cmd) {
    int n = cmd.toInt();
    Log.info("test %d", n);

    switch(n) {
    case 1:
        publishData1();
        break;

    case 2:
        publishData2();
        break;

    default:
        publishDataRandom(n);
        break;
    }

    return 0;
}
