# PZA999
**WARNING:** spoilers!

This challenge presented players with a vulnerable network driver, a made-up emulated networking device (the PZA999, implemented in QEMU), and a small TFTP server. Source for the network driver was provided during the game, while QEMU and the TFTP server were given as non-stripped binaries. The goal was to compromise the remote kernel and achieve enough code execution to read /root/flag. Players could interact with the remote instance through a TFTP server listening on port 5556, this TFTP server was running as root, so compromising this service effectively would allow the player to read the flag.

- Challenge release file: `service/pza999.tgz`
- Reference exploit: `interaction/x.py`
- PZA999 driver source: `service/src/linux/drivers/net/ethernet/pza/`
- PZA999 emulator source: `service/src/qemu/hw/net/pza999.c`
- zTFTPd source: `service/src/ztftpd/`

## Intended Bug
A number of issues came together to produce a bug in the packet reassmebly logic of the driver. If a packet was sent to the device that was larger than the current descriptor the remainder of the packet data would populate the following descriptors. The last packet in the chain of descriptors would be marked with an RXEOPK flag to inform the driver that the RXEOPK descriptor represents the last descriptor in a packet. When the driver encounters a packet without the RXEOPK marked it will crawl the descriptor list until it finds a descriptor with RXEOPK, so that it can assemble all the descriptor data into an SKB (Linux sk_buff) and send it up the network stack.

However, the device logic did not stop populating the descriptor list when the driver masked the RX interrupt, as a device should (see e1000's can_receive method). The device also did not respect if a descriptor had already been populated, but had not been read by the CPU yet, allowing the device to happily write over unprocessed descriptors.

This behavior could be abused to cause races between the device and the driver processing the descriptor list. The intended race has the player create a descriptor list layout such that there are no RXEOPK packets in the ring buffer while starting a packet reassembly. This leads the reassembly to get stuck in an indefinite loop trying to calculate a new length for the resultant SKB. To terminate the loop, the player must send in new packet to inject an RXEOPK into the ring buffer. This stops the looping of the driver, but results in most likely an incredibly large length being calculated that the kernel in unable to allocate. To handle this the driver resets the SKB back to its original size, but after reseting the SKB, it fails to update the length of the new descriptor. This allows the device to overflow the new, shrunken SKB the next time it receives network data and attempts to populate that descriptor.

## Intended Exploitation
To exploit this I overwrote the `dataref` of the shared info struct following the SKB data. This dataref is shared between all clones of the SKB, so by setting this to 0, the data pointer will be free'd after processing. This occurs even if the corrupted SKB is cloned as all SKB clones share the same underlying data region, to track this data region among the clones, a `shared_info` struct is appended to each data region containing a reference count among other things. By setting the reference count to zero pre-clone, we can get the networking stack to free the data region giving us a more convenient use-after-free in our descriptor list.

To exploit the use-after-free, the TFTP server makes use of scatter-gather IO, ie IO vectors. Players can then experiment with the windowsize and blksize TFTP options to allocate an iovec that TFTP will read file upload data to (via a PUT request). When the kernel receives an iovec (passed into readv) it will allocate it's own copy of the iovec in kernel memory and refer to it when determining where to write incoming data.

This allows the player to allocate a kernel iovec over the free'd region the descriptor list refers to. They can then use this to gain an arbitrary write in TFTP by overwriting the iovec with pointers anywhere into TFTP's memory space (I believe they can also point into kernel here too if they wanted, not sure if this has been hardened yet). Then they fulfill the readv by sending data over the UDP socket established during their initial PUT request.

If you find alternative ways to exploit this bug, let me know :)