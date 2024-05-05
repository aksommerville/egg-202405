#ifndef SYNTHWERK_INTERNAL_H
#define SYNTHWERK_INTERNAL_H

#include "opt/midi/midi.h"
#include "opt/ossmidi/ossmidi.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "opt/hostio/hostio_audio.h"
#include "opt/synth/synth.h"
#include "opt/synth/synth_channel.h"
#include "opt/http/http.h"
#include "opt/romr/romr.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/inotify.h>

// Toggle runtime features here. TODO command line.
#define SW_USE_MIDI_IN 0 /* Take input directly from all attached MIDI devices. */
#define SW_USE_AUDIO 1 /* Native audio output, synthesizer, and dynamically reload an instrument per inotify. */
#define SW_USE_HTTP  1 /* Serve the web app. */

#define SW_INSTRUMENT_FILE "mid/synthwerk.ins"
#define SW_INSTRUMENT_DIR "mid"
#define SW_INSTRUMENT_BASE "synthwerk.ins"
#define SW_HTTP_PORT 8081

extern struct sw {
  volatile int sigc;
  struct ossmidi *ossmidi;
  struct hostio_audio *audio;
  struct synth *synth;
  struct http_context *http;
  struct synth_builtin ins;
  int infd;
  struct http_websocket *ws;
  struct romr romr; // Only required for drums.
} sw;

int synthwerk_load_file();

int sw_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata);

#endif
