#include "FileUploadRK.h"

FileUploadRK *FileUploadRK::_instance;

// [static]
FileUploadRK &FileUploadRK::instance() {
    if (!_instance) {
        _instance = new FileUploadRK();
    }
    return *_instance;
}

FileUploadRK::FileUploadRK() {
}

FileUploadRK::~FileUploadRK() {
}

void FileUploadRK::setup() {
    os_mutex_create(&mutex);

}

void FileUploadRK::loop() {
    // Put your code to run during the application thread loop here
}
