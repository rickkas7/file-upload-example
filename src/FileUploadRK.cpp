#include "FileUploadRK.h"

#include <fcntl.h>
#include <dirent.h>

#include "SHA1_RK.h"

FileUploadRK *FileUploadRK::_instance;

static Logger _log("app.fileUpload");


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

bool FileUploadRK::setup() {
    os_mutex_create(&mutex);
    
    return true;
}


void FileUploadRK::loop() {
    stateHandler(*this);
}


int FileUploadRK::queueFileToUpload(const char *path, Variant meta) {
    UploadQueueEntry *uploadQueueEntry = new UploadQueueEntry();
    if (!uploadQueueEntry) {
        return SYSTEM_ERROR_NO_MEMORY;        
    }
    uploadQueueEntry->path = path;
    uploadQueueEntry->meta = meta;

    WITH_LOCK(*this) {
        uploadQueue.push_back(uploadQueueEntry);
    }

    return SYSTEM_ERROR_NONE; // 0
}



void FileUploadRK::stateStart() {
    static const char *stateName = "stateStart";

    if (!Particle.connected()) {
        // stay in stateStart until connected to the cloud
        return;
    }

    if (nextFileId == 0) {
        nextFileId = (uint32_t) random();
    }

    WITH_LOCK(*this) {
        if (uploadQueue.size() == 0) {
            // Queue is empty. nothing to do
            return;
        }

        const char *path = uploadQueue.front()->path.c_str();

        _log.trace("%s: processing file %s", stateName, path);
        fileStartTime = millis();

        fd = open(path, O_RDONLY);
        if (fd == -1) {
            _log.error("%s rror opening %s %d (discarding)", stateName, path, errno);
            uploadQueue.pop_front();
            return;
        }

        {
            struct stat sb;
            sb.st_size = 0;
            fstat(fd, &sb);
    
            if (sb.st_size == 0) {
                _log.info("%s file is empty %s (discarding)", stateName, path);
                uploadQueue.pop_front();
                return;
            }
            fileSize = (size_t) sb.st_size;    
        }

        // Calculate the SHA1 hash
        {
            SHA1_CTX ctx;
            SHA1Init(&ctx);
            for(size_t ii = 0; ii < fileSize; ii += bufferSize) {
                size_t count = fileSize - ii;
                if (count > bufferSize) {
                    count = bufferSize;
                }
                read(fd, buffer, count);
                
                SHA1Update(&ctx, (const unsigned char *)buffer, count);
            }   
            lseek(fd, 0, SEEK_SET);
            
            unsigned char digest[20];
            SHA1Final(digest, &ctx);

            hash = "";
            hash.reserve(sizeof(digest) * 2);
            for(size_t ii = 0; ii < sizeof(digest); ii++) {
                hash += String::format("%02x", (unsigned int)digest[ii]);
            }
        }
        fileId = nextFileId++;
        _log.trace("%s: fileId=%lu size=%d hash=%s", stateName, fileId, (int)fileSize, hash.c_str());

        chunkOffset = 0;
        chunkIndex = 0;
        eventOffset = 0;
        trailerSent = false;

        stateHandler = &FileUploadRK::stateSendChunk;
    }

}


void FileUploadRK::stateSendChunk() {
    static const char *stateName = "stateSendChunk";

    if (!CloudEvent::canPublish(maxEventSize)) {
        return;
    }

    size_t chunkSize = fileSize - chunkOffset;
    size_t maxChunkSize = maxEventSize - eventOffset - sizeof(ChunkHeader);
    if (chunkSize > maxChunkSize) {
        chunkSize = maxChunkSize;
    }
    bool sendTrailer = (chunkOffset >= fileSize);

    cloudEvent.clear();
    cloudEvent.name(eventName);
    cloudEvent.contentType(ContentType::BINARY);
    
    eventOffset = 0;

    if (!sendTrailer) {
        // Add the chunk header
        {
            ChunkHeader ch = {0};
            ch.version = kProtocolVersion;
            ch.flags = 0;
            ch.chunkIndex = (uint16_t) chunkIndex++;
            ch.chunkSize = (uint16_t) chunkSize;
            ch.chunkOffset = (uint32_t) chunkOffset;
            ch.fileId = fileId;

            cloudEvent.write((uint8_t *) &ch, sizeof(ChunkHeader));
            eventOffset += sizeof(ChunkHeader);
        }

        while(eventOffset < maxEventSize) {
            size_t count = fileSize - chunkOffset;
            if (count == 0) {
                break;
            }
    
            size_t spaceLeft = maxEventSize - eventOffset;
            if (count > spaceLeft) {
                count = spaceLeft;
            }
            if (count > bufferSize) {
                count = bufferSize;
            }
        
            _log.trace("%s read chunkOffset=%d chunkIndex=%d count=%d", stateName, (int)chunkOffset,(int)chunkIndex, (int)count);
        
            read(fd, buffer, count);
            cloudEvent.write(buffer, count);
        
            chunkOffset += count;
            eventOffset += count;
        }

    }

    sendTrailer = (chunkOffset >= fileSize);
    if (sendTrailer) {
        // Generate JSON data 
        Variant v;
        v.set("s", Variant(fileSize));
        v.set("h", Variant(hash.c_str()));
        v.set("id", Variant(fileId));
        v.set("n", chunkIndex);
        v.set("e", millis() - fileStartTime);
        v.set("m", uploadQueue.front()->meta);

        String json = v.toJSON();
        size_t jsonSize = json.length();

        if ((eventOffset + sizeof(ChunkHeader) + jsonSize) <= maxEventSize) {
            // Update chunk header
            ChunkHeader ch = {0};
            ch.version = kProtocolVersion;
            ch.flags = kFlagTrailer;
            ch.chunkSize = (uint16_t) jsonSize;
            ch.fileId = fileId;
    
            cloudEvent.write((uint8_t *) &ch, sizeof(ChunkHeader));
            eventOffset += sizeof(ChunkHeader);
            
            // JSON data will fit at the end of the event
            cloudEvent.write(json.c_str(), jsonSize);
            eventOffset += jsonSize;

            _log.trace("%s: trailer %s", stateName, json.c_str());

            trailerSent = true;
        }
        else {
            _log.trace("%s JSON will be sent in the next event", stateName);
        }
    }

    _log.trace("%s publishing chunkOffset=%d fileSize=%d", stateName, (int)chunkOffset,(int)fileSize);
    Particle.publish(cloudEvent);

    stateHandler = &FileUploadRK::stateWaitPublishComplete;

}

void FileUploadRK::stateWaitPublishComplete() {
    static const char *stateName = "stateWaitPublishComplete";

    if (cloudEvent.isSending()) {
        return;
    }

    int err = cloudEvent.error();
    if (err) {
        _log.trace("%s publish failed %d at chunkOffset=%d fileSize=%d", stateName, err, (int)chunkOffset,(int)fileSize);

        stateTime = millis();
        stateHandler = &FileUploadRK::stateWaitBeforeRetry;
        return;
    }

    if (!trailerSent) {
        stateHandler = &FileUploadRK::stateSendChunk;
    } else {
        if (completionHandler) {
            completionHandler(uploadQueue.front());
        }
        uploadQueue.pop_front();
        stateHandler = &FileUploadRK::stateStart;
    }
}


void FileUploadRK::stateWaitBeforeRetry() {
    // static const char *stateName = "stateWaitBeforeRetry";

    if (millis() - stateTime < retryWaitMs) {
        return;
    }

    stateHandler = &FileUploadRK::stateStart;
}

