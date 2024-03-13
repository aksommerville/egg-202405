#include "ossmidi_internal.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

/*
andy@nuc:~/proj/bits$ cat /proc/asound/oss/sndstat
Sound Driver:3.8.1a-980706 (ALSA emulation code)
Kernel: Linux nuc 6.5.0-25-generic #25~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue Feb 20 16:09:15 UTC 2 x86_64
Config options: 0

Installed drivers: 
Type 10: ALSA emulation

Card config: 
HDA Intel PCH at 0x603d1a8000 irq 201
Akai MPK225 at usb-0000:00:14.0-7, full speed

Audio devices: NOT ENABLED IN CONFIG

Synth devices: NOT ENABLED IN CONFIG

Midi devices:
1: MPK225

Timers:
31: system timer

Mixers: NOT ENABLED IN CONFIG
*/
/*
We're interested in this bit:

Midi devices:
1: MPK225

Where "1" is the kid: "/dev/midi1"
*/

/* Find details in text.
 */
 
static int ossmidi_device_lookup_details_inner(
  struct ossmidi_device_details *details,
  const struct ossmidi_device *device,
  const char *src,int srcc
) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=0,linec,in_midi_block=0;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    
    if (in_midi_block) {
      if (!linec) break;
      
      int linep=0;
      const char *kidtoken=line;
      int kidtokenc=0;
      while ((linep<linec)&&(line[linep++]!=':')) kidtokenc++;
      while (kidtokenc&&((unsigned char)kidtoken[kidtokenc-1]<=0x20)) kidtokenc--;
      int kid;
      if (sr_int_eval(&kid,kidtoken,kidtokenc)<2) continue;
      if (kid!=device->kid) continue;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      const char *name=line+linep;
      int namec=linec-linep;
      if (namec>=OSSMIDI_DEVICE_NAME_LIMIT) {
        namec=OSSMIDI_DEVICE_NAME_LIMIT-1;
        while (namec&&((unsigned char)name[namec-1]<=0x20)) namec--;
      }
      memcpy(details->name,name,namec);
      details->name[namec]=0;
      details->namec=namec;
      int i=namec; while (i-->0) {
        if ((details->name[i]<0x20)||(details->name[i]>0x7e)) details->name[i]='?';
      }
      return 0;
      
    } else if ((linec==13)&&!memcmp(line,"Midi devices:",13)) {
      in_midi_block=1;
    }
  }
  return -1;
}

/* Lookup details for one device.
 */
 
int ossmidi_device_lookup_details(struct ossmidi_device_details *details,const struct ossmidi_device *device) {
  if (!details||!device) return -1;
  const char *path="/proc/asound/oss/sndstat";
  char *src=0;
  int srcc=file_read_seekless(&src,path);
  if (srcc<0) return -1;
  int err=ossmidi_device_lookup_details_inner(details,device,src,srcc);
  free(src);
  return err;
}
