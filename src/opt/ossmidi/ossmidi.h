/* ossmidi.h
 * Required: midi serial fs
 * Take MIDI input from /dev/midi0 et al.
 * It's an OSS convention but is emulated by modern ALSA.
 */
 
#ifndef OSSMIDI_H
#define OSSMIDI_H

struct ossmidi;
struct ossmidi_device;
struct midi_event;
struct pollfd;

struct ossmidi_delegate {
  void *userdata;
  void (*cb_connect)(struct ossmidi *ossmidi,struct ossmidi_device *device);
  void (*cb_disconnect)(struct ossmidi *ossmidi,struct ossmidi_device *device);
  void (*cb_event)(struct ossmidi *ossmidi,struct ossmidi_device *device,const struct midi_event *event);
};

void ossmidi_del(struct ossmidi *ossmidi);

struct ossmidi *ossmidi_new(const struct ossmidi_delegate *delegate);

void *ossmidi_get_userdata(const struct ossmidi *ossmidi);

/* Regular update to have ossmidi do its own poll(), or list_files/update_file to poll on your own.
 * We perform additional routine maintenance during ossmidi_list_files.
 */
int ossmidi_update(struct ossmidi *ossmidi,int toms);
int ossmidi_list_files(struct pollfd *pollfdv,int pollfda,struct ossmidi *ossmidi);
int ossmidi_update_file(struct ossmidi *ossmidi,int fd);

/* Normally we pick up new devices via inotify.
 * To force a manual rescan at the next update, call this any time.
 */
void ossmidi_rescan_soon(struct ossmidi *ossmidi);

struct ossmidi_device *ossmidi_device_by_index(const struct ossmidi *ossmidi,int p);
struct ossmidi_device *ossmidi_device_by_devid(const struct ossmidi *ossmidi,int devid);
struct ossmidi_device *ossmidi_device_by_fd(const struct ossmidi *ossmidi,int fd);
struct ossmidi_device *ossmidi_device_by_kid(const struct ossmidi *ossmidi,int kid);

/* Manual disconnection does not fire a callback via delegate.
 * It does delete the device. Make sure you're not using it after.
 */
void ossmidi_disconnect(struct ossmidi *ossmidi,struct ossmidi_device *device);

/* (devid) is yours to assign. Zero if you don't.
 * (fd,kid) are constant for a device's life, and both are unique.
 * The device pointer itself is also stable as long as the device is connected, but try not to depend on that.
 */
void ossmidi_device_set_devid(struct ossmidi_device *device,int devid);
int ossmidi_device_get_devid(const struct ossmidi_device *device);
int ossmidi_device_get_fd(const struct ossmidi_device *device);
int ossmidi_device_get_kid(const struct ossmidi_device *device);

/* Encode and send a MIDI event.
 * Blocks until I/O complete.
 * Currently we will not send Sysex events.
 */
int ossmidi_device_send(struct ossmidi_device *device,const struct midi_event *event);

/* Search for more info around this device.
 * This involves reading and parsing files outside of /dev.
 */
#define OSSMIDI_DEVICE_NAME_LIMIT 64
struct ossmidi_device_details {
  char name[OSSMIDI_DEVICE_NAME_LIMIT];
  int namec;
};
int ossmidi_device_lookup_details(struct ossmidi_device_details *details,const struct ossmidi_device *device);

#endif
