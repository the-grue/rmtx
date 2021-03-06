#********************* mk script: mk partition# **************************
#!/bin/bash
if [ $# \< 2 ]; then echo Usage: mk partition# qemu OR vmware; exit; fi

if [ $2 = qemu ]; then
   VMDISK="/root/vmware/Other/vdisk"
   UMOUNT="umount"

   SECTOR=$(ptable $VMDISK $1)
   OFFSET=$(expr $SECTOR \* 512)
   echo sector=$SECTOR offset=$OFFSET
#   read dummy
   mount -o loop,offset=$OFFSET $VMDISK /mnt
fi

if [ $2 = vmware ]; then
   VMDISK="/root/vmware/Other/Other.vmdk" 
   UMOUNT="vmware-mount -d"
   vmware-mount $VMDISK $1 /mnt
fi

echo compiling .....
as86 -o ts.o kernel/ts.s
bcc -c -ansi -DMK  kernel/kernel.c
bcc -c -ansi -DMK  fs/fs.c
bcc -c -ansi -DMK  driver/driver.c

echo linking .......
ld86 -i ts.o kernel/kernel.o driver/driver.o fs/fs.o /usr/lib/bcc/libc.a

ls -l a.out

# read header; patch DS, dsize, bsize into MTX: 2,4,6
header/h a.out
dd if=a.out of=hdmtx bs=32 skip=1

(cd SETUP; make clean; make)

dd if=SETUP/boot    of=mtximage
dd if=SETUP/setup   of=mtximage bs=512  seek=1 count=1 conv=notrunc
dd if=SETUP/apentry of=mtximage bs=1024 seek=1 count=1 conv=notrunc
dd if=hdmtx of=mtximage bs=1024 seek=2 conv=notrunc
ls -l mtximage

cp mtximage /mnt/boot/mtx
$UMOUNT /mnt

(cd USER; mkallu $1 $2)

rm *.o
echo done

