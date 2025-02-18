# file-upload-example

- Github: https://github.com/rickkas7/file-upload-example
- License: MIT

This is an example of using large events in Device OS 6.3.0 and later for uploading files to the cloud
and processing them using Logic. It's not a standalone library because there are some limitations of
this method that make it a little less useful than you'd think, but you may want to use some of the 
techniques in this example in your code.

In Device OS 6.3.0 and later, events can be up to 16,384 bytes, and it's no longer necessary to wait
1 second between publishes. The rate limit is based on the amount of data waiting to be uploaded, and 
is roughly limited to 32K bytes. This makes it far more viable than before. Also the data can be binary,
which saves a lot of space in encoding (Base 64 or Base 85).

Caveat 1: Each 1K of data uploaded counts as 1 data operation, so you can potentially use a lot of them.

The uploaded files are stored in a cloud Ledger, device-scoped. This essentially means that you can 
only upload one file per device, though using the Ledger API you can get some historical versions.

Caveat 2: One file per device (mostly)

The file is stored in Ledger as Base 85, because Ledger doesn't support storing binary data. And cloud ledgers
are limited to 1 Mbyte. However there's also overhead, and it's possible that your logic block processor
may run out of memory with very large files.

Caveat 3: File size is limited

The other reason this is just an example is there are an unlimited number of features you could possibly add.
For example, the code validates the uploaded file but just discards it if the upload is missing chunks. A
system to request missing chunks could be added, but it's unclear that this will ever occur in practice.

Caveat 4: Limited feature set

This is more of a proof of concept, not a fully tested library. It seems to work, however.

Caveat 5: Limited testing

Are you still onboard? If so, here's how it works:

## Testing

### Create Ledgers

This example requires two cloud ledgers, device-scoped:

- `rick-file-upload-temp` contains the chunks of the file as the events are received
- `rick-file-upload` contains the completed file after verification

### Create Logic Block

The logic block is located in `scripts/file-upload.js`. The example firmware uses the event `fileUpload` but this can be easily changed in file-upload-example.cpp:

```cpp
.withEventName("fileUpload")
```

Remember that logic blocks require an exact name match, not a prefix match.

### Firmware

There's also firmware in the `src` directory that must be flashed to a device. The device must be running 6.3.0, and it's not intended
to be used with the Tracker One or Monitor One as it does not contain the Edge software, but would work if merged with Edge.

### Run Test

The test firmware sits and waits for a command using a function call. For example 

```
particle call testDevice1 test 20000
```

This sends a file from `testDevice1` in my account, with random contents, and a length of 20000 bytes.

There are numerous messages in the USB serial debug log while it's doing this.

Once the upload is complete, you can refresh the run log for the logic block to example the logs from that side. You can also 
view the ledger once a file has been uploaded successfully.

## Theory

The basic goal is to split a file into chunks and publish each chunk. Since events are not guaranteed to be delivered in order, the chunks need some header information to indicate which chunk it is, so they can be reassembled properly. The header is 16 bytes, and the remainder of the 16384 byte payload is binary data from the file.

The logic block receives these publishes. The chunk header indicates which file the chunks belong to using a unique number, so files sent at close to the same time and whose packets arrive out of order will not be corrupted. And the ledger is per-device, so different devices won't interfere with each other.

After the chunks a trailer block, which contains JSON data, is appended if it fits in the last publish, or is published in a separate
event if it would not fit. This contains basic information like the file size, and also the SHA-1 hash of the file. This is used to determine if the file was successfully received without corruption. Additionally the code allows you to pass your own meta data 
which is included in this block.

While this script stores the data in a second ledger, you could alternatively reassemble the parts and send the data out via a webhook.
This works because the Logic to webhook path is not limited to 16 Kbytes so the fully reassembled file can be sent in one piece if desired.

