/********************************************************************
Copyright 2010-2015 K.C. Wang, <kwang@eecs.wsu.edu>
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/
#include "ucode.c"
#include "CDROM/iso.h"

extern int alignlong;
extern char *name[16];
extern char components[64];

int nnames;

char cbuf[2048]; 
char msg[128];

eatpath(filename) char *filename;
{
  int i;   
  char *cp;
  strcpy(components, filename);

  nnames = 0;
  cp = components;

  while (*cp != 0){
       while (*cp == '/')
              *cp++ = 0;       
       if (*cp != 0)
           name[nnames++] = cp; 
       while (*cp != '/' && *cp != 0)
	       cp++;       
       if (*cp != 0)       
	   *cp = 0;        
       else
           break; 
       cp++;
  }
}

struct iso_directory_record * isearch(dirp, name) 
       struct iso_directory_record *dirp; char *name;
{
   char *cp, dname[MLEN];
   struct iso_directory_record *ddp;
   ulong extent;
   long size;
   int i, loop, count;

   if (strcmp(name, "..")==0)
     return getparent(dirp);
   
   extent = *(ulong *)dirp->extent; 
   size   = *(long  *)dirp->size;
   /* 
   printf("extent=%l size=%l\n", extent, size);
   printf("step through dir records\n");
   */
   loop = 0;
   while(size){
      count = 0;
      getSector((ulong)extent, rbuf, 1);
      cp = rbuf;

      ddp = (struct iso_directory_record *)rbuf;

      //printf("isearch for %s\n", name);

      while (cp < rbuf + 2048){
         if (ddp->length==0)
            break;

         if (sVD){ // supplement VD, must translate unicode-2 name to ascii
            strcpy(dname, dirname(ddp));
         }
         else{
             strncpy(dname, ddp->name, ddp->name_len);
             dname[ddp->name_len] = 0;
	 }

         if (loop==0){ // . and .. only in the first sector
           if (count==0)
	      strcpy(dname, ".");
           if (count==1)
	      strcpy(dname, "..");
         }

         //printf("%s  ", dname);
                   
         if (strcasecmp(dname, name)==0){ // ignore case
	   //    printf(" ==> found %s : ", name);
             return ddp;
         }
         count++;
         cp += ddp->length;
         ddp = (struct iso_directory_record *)cp;
      }
      size -= 2048;
      extent++;
      loop++;
   }
   return 0;
}   


#define TRY 5
int getCDsector(sector, ubuf, nsector) u32 sector; char *ubuf; u16 nsector;
{
  int i;
  //printf("getCDsector: sector=%l\n"); getc();
  for (i=0; i<TRY; i++){
    if (getSector((u32)sector, ubuf, nsector)==0)
      break;
  }
  if (i>=TRY) return 1;
  return 0;
}

int cdError()
{
  printf("PANIC! cd read error\n");
  printf("Re-insert CD/DVD, wait until drive's light finished flashing\n");
  getc();
}

char cdbuf[4096];

/******************** KCW: iso_path_table ******************************
 At first, thought the path table contains ALL file names on a CD.
 A big surprise to find out it only contains the DIRs!!
***********************************************************************/

int path_table()
{
   struct iso_supplementary_descriptor *spd;
   u32 psector;
   long psize;
   struct path_table *path;
   int i, r; 
   char *cp, pname[MLEN];

   printf("List Little-Endian path table\n");
   printf("get primary Volume Descriptor at sector 16\n");
 
   if (getCDsector((u32)16, cdbuf, NSECTOR)){ // error if nonZero
     printf("getCDsector error\n");
     return -1;
   }

   pd = (struct iso_primary_descriptor *)cdbuf;

   pd->id[5] = 0; // make it a string for printing
   printf("type=%d  id=%s ==> ", pd->type, pd->id); 

   if (pd->type==1){
     printf("confirm: primary VD is at sector 16\n");
   }

   psector = *(u32 *)pd->type_l_path_table;
   psize =   *(u32 *)pd->path_table_size;
   printf("psector=%l psize=%l\n", psector, psize);

   if (getCDsector((u32)psector, rbuf, NSECTOR)){
     printf("getCDsector error\n");
     return -1;
   }
   cp = rbuf;
   path = (struct path_table *)rbuf;

   //while(cp < rbuf+2048){
   while(cp < rbuf+NSECTOR*2048){
     printf("[%d %l %d ", path->name_len, path->extent, path->parent);
     strncpy(temp, path->name, path->name_len);
     temp[path->name_len] = 0;
     printf("%s]\n", temp);

     cp += path->name_len + 8;
     if (path->name_len % 2)      // if name_len is ODD, has a pad byte
        cp++;

     path = (struct path_table *)cp;
     if (cp >= rbuf+(u16)psize)
       break;
   }

   if (sVD){ // if has supplement VD, print its path table also
     if (getCDsector((u32)ssector, cdbuf, NSECTOR)){
        printf("getCDsector error\n");
        return -1;
     }

     spd = (struct iso_suplement_descriptor *)cdbuf;

     spd->id[5] = 0; // make it a string for printing
     printf("type=%d  id=%s ==> ",spd->type, spd->id); 

     psector = *(u32 *)pd->type_l_path_table;
     psize =   *(u32 *)pd->path_table_size;
      printf("psector=%l psize=%l\n", psector, psize);

      if (getCDsector((u32)psector, rbuf, NSECTOR)){
         printf("getCDsector error\n");
         return -1;
      }
     cp = rbuf;
     path = (struct path_table *)rbuf;

     //while(cp < rbuf+2048){
     while(cp < rbuf+NSECTOR*2048){
        printf("[%d %l %d ", path->name_len, path->extent, path->parent);

        for (i=0; i<path->name_len; i+=2){
            temp[i/2] = path->name[i+1];
        }
        temp[path->name_len/2] = 0;
        printf("%s]\n", temp);

        cp += path->name_len + 8;
        if (path->name_len % 2)      // if name_len is ODD, has a pad byte
           cp++;

        path = (struct path_table *)cp;
        if (cp >= rbuf+(ushort)psize)
            break;
     }
   }
}

struct iso_directory_record * getdir(filename) char *filename;
{
   int i;
   struct iso_directory_record *dirp;

   if (filename[0]=='/')
      dirp = root;
   else
     dirp = cwd;

   eatpath(filename);

   for (i=0; i<nnames; i++){
      dirp = isearch(dirp, name[i]);
      if (dirp == 0){
	printf("no such name %s\n", name[i]);
        return 0;
      }
      // check DIR type
      if (i<nnames-1){
        if ((dirp->flags & 0x02) == 0){
            printf("%s is not a DIR\n", name[i]);
            return 0;
        }
      }
   }
   return dirp;
}


char line[64];
char cmd[16], filename[32], dest[32];
char *token;

int main(int argc, char *argv[ ])
{
   char *cp;
   int i, count, r, client; 
   struct iso_supplementary_descriptor *spd;
   ulong sector;

   //printf("cdserver: Insert a CDROM to /dev/hdc\n");
   
   chuid(1); // set uid to 1

   root = (struct iso_directory_record *)root_dir;
   cwd  = (struct iso_directory_record *)cwd_dir;

   //printf("show volume descriptors\n");

   sVD = 0; 
   ssector = 0;
   sector = 16;
 
   for (i=0; i<4; i++){
     if (getCDsector((u32)sector, cdbuf, 1)){
       printf("start CDSERVER daemon error: Insert a CD/DVD and reboot\n");
       return -1;
     }
     pd = (struct iso_primary_descriptor *)cdbuf;
     pd->id[5] = 0;

     //printf("sector=%l type=%d  id=%s\n", sector, pd->type, pd->id); 
     if (pd->type==2){ // set sVD flag and get ssector
      //printf("found suppplementary volume descriptor in sector %l\n", sector);
       sVD = 1;
       ssector = sector;
     }
     if (pd->type==255)
       break;
     sector += 1;
   }
   /******
   if (sVD)
     // printf("Use Supplement Volume Descriptor at sector=%l\n", ssector);
   else{
     sector = 16;
     //printf("Use primary VD at sector sector=16\n");
   }
   *********/
   if (!sVD)
     sector = 16;
   //printf("get VD at sector %l\n", ssector);

   if (getCDsector((u32)ssector, cdbuf, NSECTOR)){
     printf("CD error\n");
     return -1;
   }
   pd = (struct iso_primary_descriptor *)cdbuf;

   pd->id[5] = 0; // make it a string for printing
   //printf("Volume Desc type=%d  id=%s ==> ",pd->type, pd->id);
  
   //printf("get root dir record in Volume Desc\n");

   dirp = (struct iso_directory_record *)pd->root_directory_record;

   // copy root DIR structs so that they do not tie up cdbuf[ ] 
   *root = *dirp; 
   *cwd  = *dirp;
   // now we have both root and cwd DIRs

   // load path_table into ptable[ ] and keep it there
   if (getCDsector((u32)ssector, cdbuf, NSECTOR)){
     printf("CD error\n");
     return -1;
   }

   spd = (struct iso_suplement_descriptor *)cdbuf;

   spd->id[5] = 0; // make it a string for printing
   //printf("Volume Desc type=%d  id=%s ==> ",spd->type, spd->id); 

   psector = *(u32 *)spd->type_l_path_table;
   psize   = *(long  *)spd->path_table_size;
   //printf("path table : sector=%l   size=%l\n", psector, psize);

   if (getCDsector((u32)psector, patable, NSECTOR)){
     printf("CD error\n");
     return -1;
   }
   //printf("CDSERVER ready\n");


   while(1){
     //printf("CDSERVER %d: commands = ls|cd|pwd|cat|cp|quit\n", getpid());
     printf("CDSERVER %d: waiting for request message\n", getpid());

     client = recv(msg);

     printf("CDSERVER: got a request message=[%s] from %d\n", msg, client);
     strcpy(line, msg);

     if (line[0]==0)
       continue;

     filename[0]=0;  dest[0]=0;
     strcpy(cmd, strtok(line, " "));
     token = strtok(0, " ");
     if (token)
        strcpy(filename, token);
     token = strtok(0, " ");
     if (token) 
        strcpy(dest, token);
     //printf("cmd=%s   filename=%s   dest=%s\n", cmd, filename,dest);
  
     if (!strcmp(cmd, "ls"))
       r = ls();
     else if (!strcmp(cmd, "cd"))
        r = cdchdir();
     else if (!strcmp(cmd, "cat"))
        r = cat();
     else if (!strcmp(cmd, "path"))
        r = ptable();
     else if (!strcmp(cmd, "pwd"))
        r = cdpwd();

     else if (!strcmp(cmd, "cp"))
        r = cdcp();

     else if (!strcmp(cmd, "quit")){
       printf("client quit, CDSERVER continue : ");
       cdchdir("/");       
       continue;
     }
     else{
        printf("invalid command\n");
        r = -1;
     }
     strcpy(msg, "OK");     
     if (r<0)
       strcpy(msg, "BAD");
     //printf("CDSERVER: send reply [%s] to %d\n", msg, client);
     send(msg, client);
   }
}


int cdcp()
{
  struct iso_directory_record * dirp;
  ulong sector; 
  long size;
  int i, fd, count, n;

  if (filename[0]==0)
    return 0;
  if (dest[0]==0){
    printf("cdcp: need target filename\n");
    return -1;
  }

  dirp = getdir(filename);
  if (dirp == 0)
    return -1;

  if (dirp->flags & 0x02){
    printf("%s is a DIR\n", filename);
    return -1;
  }

  sector = *(u32 *)dirp->extent; 
  size   = *(long  *)dirp->size;
 
  fd = open(dest, O_WRONLY|O_CREAT);
  if (fd<0){
    printf("open %s fro WRITE error\n");
    return -1;
  }

  // printf("\ncdcp: sector=%l size=%l\n", sector, size);

  count = 0;
  while(size>0){
    if (getCDsector((u32)sector, rbuf, NSECTOR)){
      printf("CD error\n");
      return -1;
    }

    if (size > NSECTOR*2048)
        rbuf[NSECTOR*2048] = 0;
    else
        rbuf[size] = 0;

    n = strlen(rbuf);
    write(fd, rbuf, n);
    count += n;

    size -= NSECTOR*2048;
    sector += NSECTOR;
  }
  printf("cdcp: %d bytes copied to %s\n", count, dest);
  close(fd);
}


int ls( )
{
  struct iso_directory_record * dirp;
  ulong sector;
  long size;

  if (filename[0]==0){
     printf("ls cwd\n");
     dirp = cwd;
  }
  else{
    dirp = getdir(filename);
    if (dirp==0){
      printf("no such DIR %s\n", filename);      
      return -1;
    }
  }
  // now list this DIR
  if (dirp->flags & 0x02)
     list_dir(dirp);
}

copy64(from, to) char *from, *to;
{
  int i;
  for (i=0; i<64; i++){
    *to = *from;
    to++; from++;
  }
}

int cdchdir()
{
  struct iso_directory_record * dirp;
  int i; char myname[MLEN];
  ulong mysector;
  char *cp, *cq;

  if (filename[0]==0 || strcmp(filename, "/")==0){
    printf("cd to root\n");
    //*cwd = *root;
    copy64(root, cwd);

    return 0;
  }  
  dirp = getdir(filename);
  if (!dirp){
    printf("no such DIR %s\n", filename);
    return -1;
  }
  if ((dirp->flags & 0x02) == 0){
    printf("%s is not a DIR\n", filename);
    return -1;
  }

  //*cwd = *dirp;
  copy64(dirp, cwd);

  for (i=0; i <= cwd->name_len; i+=2){
       myname[i/2] = cwd->name[i+1];
  }
  myname[cwd->name_len/2] = 0;
  mysector = *(u32 *)cwd->extent;
  //printf("cwd name=%s sector=%l\n", myname, mysector);

  printf("cd %s OK\n", filename);
  return 0;
}

int ptable()
{
  printf("print path_tablee\n");
  path_table();
}

int cat()
{
  struct iso_directory_record * dirp;
  ulong sector; 
  long size;
  int i;

  if (filename[0]==0)
    return 0;
  dirp = getdir(filename);

  if (dirp == 0)
    return -1;

  if (dirp->flags & 0x02){
    printf("%s is a DIR\n", filename);
    return -1;
  }

  sector = *(u32 *)dirp->extent; 
  size   = *(long  *)dirp->size;
 
  /*********
  printf("\nCAT: sector=%l size=%l\n", sector, size);
  getc();
  *********/

  while(size>0){
    if (getCDsector((u32)sector, rbuf, NSECTOR)){
      printf("CD error\n");
      return -1;
    }

    //printf("cat: file=%s size=%l\n", filename, size);
    //getc();

    if (size > NSECTOR*2048)
        rbuf[NSECTOR*2048] = 0;
    else
        rbuf[size] = 0;
    /*****************************
    for (i=0; i<NSECTOR*2048; i++){
      if (rbuf[i]==0)
	break;
      putc(rbuf[i]);
    }
    *******************************/
    printf(rbuf);
    size -= NSECTOR*2048;
    sector += NSECTOR;
  }
}

int list_dir(dirp) struct iso_directory_record *dirp;
{
  u32 sector;
  long  size,dsize;
  
  int i, loop, count;
  char *cp, pname[MLEN];

  //printf("list_dir "); getc();

  sector = *(u32  *)dirp->extent; 
  size   = *(long *)dirp->size;
  loop = 0;

  alignlong = 1;   // printl(x) Right align x

  while(size>0){
     printf("reading root dir's sector=%l\n", sector);
     if (getCDsector((u32)sector, rbuf, NSECTOR)){
       printf("CD error\n");
       return -1;
     }

     dirp = (struct iso_directory_record *)rbuf;

     count=0;
     cp = rbuf;
     
     while(cp < rbuf+NSECTOR*2048){     // break if length=0
        if (dirp->length==0)
            break;

        if (sVD){ // SUPPLEMENT : translate UCS-2 filename to ASCII
           strcpy(pname, dirname(dirp));
	}
        else{
            strncpy(pname, dirp->name, dirp->name_len);
            pname[dirp->name_len] = 0;
	}
        if (dirp->flags & 0x02)
	   strcat(pname, "/");

        /*********************** KCW ************************
         iso dir does NOT have . and ..; they are 0x00 and 0x01
         with name_len=1. ==> it's not possible to get .. from
         a DIR ==> that's why my getparent() is rather complex.
	*****************************************************/

        if (loop==0){
	   if (count==0)
              strcpy(pname, ".");
           else if (count==1)
              strcpy(pname, "..");
	}

        dsize = *(long *)(dirp->size);

        printf("%l  %s\n", dsize, pname);

	if (strcmp(pname, "vmlinuz")==0) 
	  break;

        cp += dirp->length;
        dirp = (struct iso_directory_record *)cp;
        count++;
     }
     size -= NSECTOR*2048;
     sector += NSECTOR;
     loop++;
  }
  alignlong = 0;
}

 

int rpwd(dp) struct iso_directory_record *dp;
{
  struct iso_directory_record *pdp;
  char localdir[64];
  char pname[MLEN];
  
  pdp = getparent(dp);

  copy64(pdp, localdir);
  strcpy(pname, dirname(pdp));

  if (pdp == root){
      printf("/");
  }
  else{ 
      rpwd((struct iso_directory_record *)localdir);
      printf("%s/", pname);
  }
}

int cdpwd()
{
  char myname[MLEN];
  ulong mysector;

  strcpy(myname, dirname(cwd));
  mysector = dirsector(cwd);

  // printf("cwd name=%s sector=%l\n", myname, mysector);
  //  getc();

  printf("\ncwd=");
   rpwd(cwd);
  printf("%s\n", myname);
}  

