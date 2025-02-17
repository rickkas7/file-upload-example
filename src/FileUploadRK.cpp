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
        nextFileId = (unsigned int) random();
    }

    WITH_LOCK(*this) {
        if (uploadQueue.size() == 0) {
            // Queue is empty. nothing to do
            return;
        }

        const char *path = uploadQueue.front()->path.c_str();

        _log.trace("%s: processing file %s", stateName, path);

        fd = open(path, O_RDONLY);
        if (fd == -1) {
            _log.error("error opening %s %d (discarding)", path, errno);
            uploadQueue.pop_front();
            return;
        }

        sb.st_size = 0;
        fstat(fd, &sb);

        if (sb.st_size == 0) {
            _log.info("file is empty %s (discarding)", path);
            uploadQueue.pop_front();
            return;
        }
        fileSize = (size_t) sb.st_size;

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
        _log.trace("%s: fileId=%u size=%d hash=%s", stateName, fileId, (int)fileSize, hash.c_str());

        // Generate JSON data
        eventOffset = 0;
        cloudEvent.clear();

        FirstHeader *fh = (FirstHeader *)&buffer[eventOffset];
        eventOffset += sizeof(FirstHeader);

        fh->firstHeaderMarker = 0;
        fh->version = kProtocolVersion;
        fh->firstHeaderSize = (uint8_t)sizeof(FirstHeader);
        fh->chunkHeaderSize = (uint8_t)sizeof(ChunkHeader);
        // fh->jsonSize is filled out below after determining the size

        char *jsonData = (char *)&buffer[eventOffset];
        size_t jsonMaxDataSize = bufferSize - eventOffset - sizeof(ChunkHeader) - 1;

        Variant v;
        v.set("s", Variant(fileSize));
        v.set("h", Variant(hash.c_str()));
        v.set("id", Variant(fileId));
        v.set("m", uploadQueue.front()->meta);

        {
            String json = v.toJSON();
            if (json.length() > jsonMaxDataSize) {
                _log.error("meta data too large %s (discarding)", path);
                uploadQueue.pop_front();
                return;
            }
            memcpy(jsonData, json.c_str(), json.length());
            fh->jsonSize = (uint16_t) json.length();

            jsonData[fh->jsonSize] = 0;
            eventOffset += fh->jsonSize;
            _log.trace("%s: jsonData %s", stateName, jsonData);
        }

        chunkOffset = 0;
        chunkIndex = 0;

        // FirstHeader, JSON data
        cloudEvent.name(eventName);
        cloudEvent.contentType(ContentType::BINARY);
        cloudEvent.write(buffer, eventOffset);
        
        // First chunk
        // Version (2 bytes, little endian, unsigned)
        // JSON size (2 bytes, little endian, unsigned)

        // Header chunk (JSON)
        // File size (s), SHA1 hash (h), filename (n), file id (id)
        // Metadata (m)

        // Data chunk header (fixed size, depends on version)
        // Chunk index (2 bytes, little endian, unsigned)
        // Chunk size (2 bytes, little endian, unsigned
        // file id (4 bytes, little endian, unsigned)

        // Data follows data chunk header (variable size)


        stateHandler = &FileUploadRK::stateSendChunk;
    }

}


void FileUploadRK::stateSendChunk() {
    static const char *stateName = "stateSendChunk";

    if (chunkOffset >= fileSize) {
        _log.trace("%s file send complete", stateName);
        stateHandler = &FileUploadRK::stateStart;
        uploadQueue.pop_front();
        return;
    }

    if (!CloudEvent::canPublish(maxEventSize)) {
        return;
    }

    size_t chunkSize = fileSize - chunkOffset;
    size_t maxChunkSize = maxEventSize - eventOffset - sizeof(ChunkHeader);
    if (chunkSize > maxChunkSize) {
        chunkSize = maxChunkSize;
    }

    // Add the chunk header
    {
        ChunkHeader ch;
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


    _log.trace("%s publishing chunkOffset=%d fileSize=%d", stateName, (int)chunkOffset,(int)fileSize);
    Particle.publish(cloudEvent);

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

    
    stateHandler = &FileUploadRK::stateSendChunk;
}

void FileUploadRK::stateWaitBeforeRetry() {
    static const char *stateName = "stateWaitBeforeRetry";

    if (millis() - stateTime < retryWaitMs) {
        return;
    }

    stateHandler = &FileUploadRK::stateStart;
}

