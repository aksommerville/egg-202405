#include "ossmidi_internal.h"

/* Delete.
 */
 
void ossmidi_device_del(struct ossmidi_device *device) {
  if (!device) return;
  if (device->fd>=0) close(device->fd);
  free(device);
}

/* New.
 */
 
struct ossmidi_device *ossmidi_device_new() {
  struct ossmidi_device *device=calloc(1,sizeof(struct ossmidi_device));
  if (!device) return 0;
  device->fd=-1;
  return device;
}

/* Trivial accessors.
 */
 
void ossmidi_device_set_devid(struct ossmidi_device *device,int devid) {
  device->devid=devid;
}

int ossmidi_device_get_devid(const struct ossmidi_device *device) {
  return device->devid;
}

int ossmidi_device_get_fd(const struct ossmidi_device *device) {
  return device->fd;
}

int ossmidi_device_get_kid(const struct ossmidi_device *device) {
  return device->kid;
}

/* Send event.
 */

int ossmidi_device_send(struct ossmidi_device *device,const struct midi_event *event) {
  if (!device||!event) return -1;
  if (device->fd<0) return -1;
  uint8_t serial[3];
  int serialc=0;
  switch (event->opcode) {
    #define A serial[0]=(event->opcode|event->chid); serial[1]=event->a; serialc=2; break;
    #define AB serial[0]=(event->opcode|event->chid); serial[1]=event->a; serial[2]=event->b; serialc=3; break;
    case MIDI_OPCODE_NOTE_OFF: AB
    case MIDI_OPCODE_NOTE_ON: AB
    case MIDI_OPCODE_NOTE_ADJUST: AB
    case MIDI_OPCODE_CONTROL: AB
    case MIDI_OPCODE_PROGRAM: A
    case MIDI_OPCODE_PRESSURE: A
    case MIDI_OPCODE_WHEEL: AB
    #undef A
    #undef AB
    default: if (event->opcode>=0xf0) { // realtime
        serial[0]=event->opcode;
        serialc=1;
      } break;
  }
  if (serialc<1) return -1;
  return write(device->fd,serial,serialc);
}

/* Update.
 */
 
int ossmidi_device_update(struct ossmidi *ossmidi,struct ossmidi_device *device) {
  if (!ossmidi||!device||(device->fd<0)) return 0;
  char tmp[256];
  int tmpc=read(device->fd,tmp,sizeof(tmp));
  if (tmpc<=0) {
    // Will wrap up at next update.
    close(device->fd);
    device->fd=-1;
    return 0;
  }
  if (!ossmidi->delegate.cb_event) return 0; // huh?
  if (midi_stream_receive(&device->stream,tmp,tmpc)<0) {
    close(device->fd);
    device->fd=-1;
    return 0;
  }
  struct midi_event event={0};
  int err;
  while ((err=midi_stream_next(&event,&device->stream))>0) {
    ossmidi->delegate.cb_event(ossmidi,device,&event);
  }
  if (err<0) {
    close(device->fd);
    device->fd=-1;
    return 0;
  }
  return 0;
}
