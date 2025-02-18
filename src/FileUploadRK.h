#ifndef __FILEUPLOADRK_H
#define __FILEUPLOADRK_H

#include "Particle.h"

#ifndef SYSTEM_VERSION_630
#error "This library requires Device OS 6.3.0 or later"
#endif

#include <deque>
#include <sys/stat.h>

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
     * @brief Structure that precedes data in an event
     */
    struct ChunkHeader { // 16 bytes
        uint8_t version; //!< Version number (kProtocolVersion = 1)
        uint8_t flags; //!< Various flags
        uint16_t reserved; //!< Reserved for future use
        uint16_t chunkIndex; //!< 0-based index for which chunk this is
        uint16_t chunkSize; //!< size of this chunk in bytes
        uint32_t chunkOffset; //!< offset in the file
        uint32_t fileId; //!< fileId of this chunk
    };

    /**
     * @brief Structure to hold a file to upload. This is passed to the completionHandler
     */
    class UploadQueueEntry {
        public:
            String path; //!< Path to file on the POSIX flash file system.
            Variant meta; //!< VariantMap of additional data to include. This must be serializable to JSON (no buffers).
        };
    
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
     * @brief Set the maximum event chunk to use. Default is 16384, but can be made smaller
     * 
     * @param maxEventSize 
     * @return FileUploadRK& 
     * 
     * Event size should be a multiple of 1024 bytes for efficiency. Because the event size must
     * be stored in RAM, it may be desirable to set it smaller on nRF52 devices with more limited
     * RAM.
     * 
     * This must be set before calling setup()!
     */
    FileUploadRK &withMaxEventSize(size_t maxEventSize) { this->maxEventSize = maxEventSize; return  *this; };


    /**
     * @brief Set the function to call when a file has been successfully sent
     * 
     * @param fn 
     * @return FileUploadRK& 
     */
    FileUploadRK &withCompletionHandler(std::function<void(const UploadQueueEntry *queueEntry)> fn) { this->completionHandler = fn; return *this; };

    /**
     * @brief Perform setup operations; call this from global application setup()
     * 
     * You typically use FileUploadRK::instance().setup();
     * 
     * Returns true if the operation succeeded or false if setup failed, typically out of memory.
     */
    bool setup();

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
    int queueFileToUpload(const char *path, Variant meta = {});

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

    static const uint16_t kProtocolVersion = 1; //!< Version number of the file upload protocol


    static const uint8_t kFlagTrailer = 0x01; //!< Chunk is the trailer, not actually a chunk

protected:

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
     * @brief State handler. This state is entered upon loop() starting.
     */
    void stateStart();

    /**
     * @brief State handler. Send a chunk of data, or the trailer.
     */
    void stateSendChunk();

    /**
     * @brief State handler. Wait for the publish to complete
     * 
     * Next state is:
     * - stateStart if done (trailer sent)
     * - stateSendChunk if chunk was sent successfully
     * - stateWaitBeforeRetry if an error occurred
     */
    void stateWaitPublishComplete();

    /**
     * @brief StateHandler Wait for retryWaitMs before going back to stateStart
     * 
     * If an error occurs while publishing, this state is entered from stateWaitPublishComplete.
     */
    void stateWaitBeforeRetry();

    /**
     * @brief Mutex to protect shared resources
     * 
     * This is initialized in setup() so make sure you call the setup() method from the global application setup.
     */
    os_mutex_t mutex = 0;

    /**
     * @brief Event name to use for uploads. Default is "fileUpload".
     * 
     * Set using withEventName() prior to calling setup().
     */
    String eventName = "fileUpload";

    std::function<void(FileUploadRK&)> stateHandler = &FileUploadRK::stateStart; //!< State handler, called from loop()

    std::deque<UploadQueueEntry *>uploadQueue; //!< Queue of files to upload (in RAM only)

    int fd = -1; //!< File system file descriptor (from open()) for file being sent
    static const size_t bufferSize = 512; //!< Internal buffer size, used for reading from the file system
    uint8_t buffer[bufferSize]; //!< Buffer using for reading from the file system
    String hash; //!< SHA-1 hash of file 
    uint32_t fileId = 0; //!< fileId, used to identify which file when the event is received by the cloud
    size_t chunkOffset = 0; //!< Offset in file for the chunk being sent
    size_t chunkIndex = 0; //!< Which chunk will be sent next
    size_t eventOffset = 0; //!< Offset in the event to write to next
    size_t fileSize = 0; //!< Size of the file
    unsigned long stateTime = 0; //!< millis value when we entered the state (used for stateWaitBeforeRetry)
    unsigned long fileStartTime = 0; //!< millis value when the file started being processed
    bool trailerSent = false; //!< Whether the trailer has been sent yet
    CloudEvent cloudEvent; //!< Event

    size_t maxEventSize = 16384; //!< Maximum size of the event to send
    uint32_t nextFileId = 0; //!< Next fileId to send, initialized to random value after cloud connection

    unsigned long retryWaitMs = 120000;//!< How long to wait in stateWaitBeforeRetry state

    std::function<void(const UploadQueueEntry *queueEntry)> completionHandler = 0; //!< Function to call when file has been sent

    /**
     * @brief Singleton instance of this class
     * 
     * The object pointer to this class is stored here. It's NULL at system boot.
     */
    static FileUploadRK *_instance;

};

#endif // __FILEUPLOADRK_H
