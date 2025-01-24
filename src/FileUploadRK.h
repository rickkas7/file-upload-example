#ifndef __FILEUPLOADRK_H
#define __FILEUPLOADRK_H

#include "Particle.h"

#ifndef SYSTEM_VERSION_630
#error "This library requires Device OS 6.3.0 or later"
#endif

/**
 * This class is a singleton; you do not create one as a global, on the stack, or with new.
 * 
 * From global application setup you must call:
 * FileUploadRK::instance().setup();
 * 
 * From global application loop you must call:
 * FileUploadRK::instance().loop();
 */
class FileUploadRK {
public:
    /**
     * @brief Gets the singleton instance of this class, allocating it if necessary
     * 
     * Use FileUploadRK::instance() to instantiate the singleton.
     */
    static FileUploadRK &instance();

    /**
     * @brief Set the event name to use for uploads (default: fileUpload)
     * 
     * @param eventName 
     * @return FileUploadRK& 
     */
    FileUploadRK &withEventName(const char *eventName) { this->eventName = eventName; return *this; };

    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * You typically use FileUploadRK::instance().setup();
     */
    void setup();

    /**
     * @brief Perform application loop operations; call this from global application loop()
     * 
     * You typically use FileUploadRK::instance().loop();
     */
    void loop();

    /**
     * @brief Enqueue a file to upload
     * 
     * @param path 
     * @return int 
     */
    int queueFileToUpload(const char *path);

    /**
     * @brief Locks the mutex that protects shared resources
     * 
     * This is compatible with `WITH_LOCK(*this)`.
     * 
     * The mutex is not recursive so do not lock it within a locked section.
     */
    void lock() { os_mutex_lock(mutex); };

    /**
     * @brief Attempts to lock the mutex that protects shared resources
     * 
     * @return true if the mutex was locked or false if it was busy already.
     */
    bool tryLock() { return os_mutex_trylock(mutex); };

    /**
     * @brief Unlocks the mutex that protects shared resources
     */
    void unlock() { os_mutex_unlock(mutex); };


protected:
    class UploadState {
    };

    /**
     * @brief The constructor is protected because the class is a singleton
     * 
     * Use FileUploadRK::instance() to instantiate the singleton.
     */
    FileUploadRK();

    /**
     * @brief The destructor is protected because the class is a singleton and cannot be deleted
     */
    virtual ~FileUploadRK();

    /**
     * This class is a singleton and cannot be copied
     */
    FileUploadRK(const FileUploadRK&) = delete;

    /**
     * This class is a singleton and cannot be copied
     */
    FileUploadRK& operator=(const FileUploadRK&) = delete;

    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
    os_mutex_t mutex = 0;

    /**
     * @brief Event name to use for uploads
     */
    String eventName = "fileUpload";

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static FileUploadRK *_instance;

};

#endif // __FILEUPLOADRK_H
