#include "eggrom_internal.h"
#include "opt/midi/midi.h"

/* Converter context.
 */
 
struct eggrom_song_context {
  struct midi_file *midi;
  struct sr_encoder *dst;
  const struct romw_res *res;
  struct eggrom_song_event {
    int time; // absolute ms
    uint8_t chid,opcode,a,b; // straight off midi
  } *eventv;
  int eventc,eventa;
  int time;
  int hichidc;
  int usperqnote;
  int hasdrums;
  int drumchid;
};

static void eggrom_song_context_cleanup(struct eggrom_song_context *ctx) {
  midi_file_del(ctx->midi);
  if (ctx->eventv) free(ctx->eventv);
}

/* Add event.
 */
 
static struct eggrom_song_event *eggrom_song_add_event(struct eggrom_song_context *ctx) {
  if (ctx->eventc>=ctx->eventa) {
    int na=ctx->eventa+256;
    if (na>INT_MAX/sizeof(struct eggrom_song_event)) return 0;
    void *nv=realloc(ctx->eventv,sizeof(struct eggrom_song_event)*na);
    if (!nv) return 0;
    ctx->eventv=nv;
    ctx->eventa=na;
  }
  struct eggrom_song_event *event=ctx->eventv+ctx->eventc++;
  memset(event,0,sizeof(struct eggrom_song_event));
  return event;
}

/* Initial event filter.
 * Throw out anything we definitely won't need.
 * It's fair to manage timeless things here too (eg yoinking tempo).
 */
 
static int eggrom_song_should_keep_event(struct eggrom_song_context *ctx,const struct midi_event *event) {
  if ((event->opcode==MIDI_OPCODE_META)&&(event->a==MIDI_META_SET_TEMPO)) {
    if (event->c==3) {
      const uint8_t *v=event->v;
      ctx->usperqnote=(v[0]<<16)|(v[1]<<8)|v[2];
    }
    return 0;
  }
  if ((event->opcode==MIDI_OPCODE_META)&&(event->a==MIDI_META_EOT)) {
    // Preserve Meta EOT. We don't need the event per se, but we do want its timestamp, to avoid trimming a little at the end.
    return 1;
  }
  if (event->chid>=0x10) return 0; // Other Meta or Sysex, not interested.
  if ((event->chid==0x09)||(event->chid==0x0f)) { // Do keep MIDI channels 10 and 15, that's where drums conventionally go. We'll find a home for them.
    ctx->hasdrums=1;
    return 1;
  }
  if (event->chid>=0x08) { // We only get 8 channels in output. Flag this for a warning and discard.
    ctx->hichidc++;
    return 0;
  }
  // Throw away PRESSURE, ADJUST, and anything we don't recognize.
  // Do keep PROGRAM and CONTROL at this time. We'll need them, to assemble the output channel headers.
  switch (event->opcode) {
    case MIDI_OPCODE_NOTE_ON:
    case MIDI_OPCODE_NOTE_OFF:
    case MIDI_OPCODE_PROGRAM:
    case MIDI_OPCODE_CONTROL:
    case MIDI_OPCODE_WHEEL:
      return 1;
  }
  return 0;
}

/* Emit header and channel config.
 */
 
static int eggrom_song_emit_channel_header(struct eggrom_song_context *ctx,int chid) {
  // pid, volume, pan, zero
  uint8_t tmp[4]={0,0x80,0x80,0};
  const struct eggrom_song_event *event=ctx->eventv;
  int i=ctx->eventc;
  int notec=0;
  for (;i-->0;event++) {
    if (event->chid!=chid) continue;
    if (event->opcode==MIDI_OPCODE_PROGRAM) {
      if (tmp[0]) fprintf(stderr,"%s:WARNING: Multiple Program Change on channel %d. We will apply pid %d to the entire channel.\n",ctx->res->path,chid,event->a);
      tmp[0]|=event->a;
    } else if (event->opcode==MIDI_OPCODE_CONTROL) switch (event->a) {
      case MIDI_CONTROL_VOLUME_MSB: tmp[1]=event->b<<1; break;
      case MIDI_CONTROL_PAN_MSB: tmp[2]=event->b<<1; break;
      case MIDI_CONTROL_BANK_LSB: if (event->b&1) tmp[0]|=0x80; else tmp[0]&=~0x80; break;
    } else if (event->opcode==MIDI_OPCODE_NOTE_ON) {
      notec++;
    }
  }
  if (!notec) { // No notes, the channel is not in use.
    if (ctx->hasdrums) { // And we have events on 0x09, so put them here instead.
      for (i=ctx->eventc,event=ctx->eventv;i-->0;event++) {
        if ((event->chid!=0x09)&&(event->chid!=0x0f)) continue;
        if (event->opcode==MIDI_OPCODE_PROGRAM) {
          tmp[0]=event->a;
        } else if (event->opcode==MIDI_OPCODE_CONTROL) switch (event->a) {
          case MIDI_CONTROL_VOLUME_MSB: tmp[1]=event->b<<1; break;
          case MIDI_CONTROL_PAN_MSB: tmp[2]=event->b<<1; break;
          case MIDI_CONTROL_BANK_LSB: if (event->b&1) tmp[0]|=0x80; else tmp[0]&=~0x80; break;
        }
      }
      if (!tmp[0]) tmp[0]=0x80;
      ctx->hasdrums=0;
      ctx->drumchid=chid;
    } else { // Really empty. Make sure the volume is zero so runtime knows it's unused.
      memset(tmp,0,sizeof(tmp));
    }
  }
  return sr_encode_raw(ctx->dst,tmp,sizeof(tmp));
}
 
static int eggrom_song_emit_header(struct eggrom_song_context *ctx) {
  int err;
  if (sr_encode_raw(ctx->dst,"\xbe\xee\xeeP",4)<0) return -1;
  if (sr_encode_intbe(ctx->dst,(ctx->usperqnote+500)/1000,2)<0) return -1; // Tempo
  if (sr_encode_intbe(ctx->dst,42,2)<0) return -1; // Start Position
  if (sr_encode_intbe(ctx->dst,42,2)<0) return -1; // Loop Position
  int chid=0; for (;chid<8;chid++) {
    if ((err=eggrom_song_emit_channel_header(ctx,chid))<0) return err;
  }
  if (ctx->dst->c!=42) {
    fprintf(stderr,"%s: %s:%d: ctx->dst->c==%d, expected 42\n",ctx->res->path,__FILE__,__LINE__,ctx->dst->c);
    return -2;
  }
  return 0;
}

/* Find a Note Off event.
 */
 
static const struct eggrom_song_event *eggrom_song_find_note_off(
  const struct eggrom_song_context *ctx,
  int startp,
  uint8_t chid,uint8_t noteid
) {
  const struct eggrom_song_event *event=ctx->eventv+startp;
  int i=ctx->eventc-startp;
  for (;i-->0;event++) {
    if (event->opcode!=MIDI_OPCODE_NOTE_OFF) continue;
    if (event->chid!=chid) continue;
    if (event->a!=noteid) continue;
    return event;
  }
  return 0;
}

/* Emit events.
 */
 
static int eggrom_song_emit_events(struct eggrom_song_context *ctx) {
  if (ctx->eventc<1) {
    fprintf(stderr,"%s: Empty song.\n",ctx->res->path);
    return -2;
  }
  int dsttime=0;
  const struct eggrom_song_event *event=ctx->eventv;
  int eventp=0;
  for (;eventp<ctx->eventc;eventp++,event++) {
  
    // Only NOTE_ON and WHEEL cause output events.
    if ((event->opcode!=MIDI_OPCODE_NOTE_ON)&&(event->opcode!=MIDI_OPCODE_WHEEL)) continue;
    
    // Emit DELAY as needed, 63 ms at a time.
    while (dsttime<event->time) {
      int delaytime=event->time-dsttime;
      if (delaytime>63) delaytime=63;
      if (sr_encode_u8(ctx->dst,delaytime)<0) return -1;
      dsttime+=delaytime;
    }
    
    // WHEEL is straightforward.
    if (event->opcode==MIDI_OPCODE_WHEEL) {
      int v=event->a|(event->b<<7);
      if (sr_encode_u8(ctx->dst,0xa0|event->chid)<0) return -1;
      if (sr_encode_u8(ctx->dst,v>>6)<0) return -1;
      continue;
    }
    
    /* NOTE_ON for drums becomes FIREFORGET on a channel we discovered earlier.
     */
    if ((event->opcode==MIDI_OPCODE_NOTE_ON)&&((event->chid==0x09)||(event->chid==0x0f))) {
      if (sr_encode_u8(ctx->dst,0x90|((event->b>>3)&0x0c)|(ctx->drumchid>>1))<0) return -1;
      if (sr_encode_u8(ctx->dst,(ctx->drumchid<<7)|event->a)<0) return -1;
      continue;
    }
    
    /* NOTE_ON normally, we need to find the corresponding NOTE_OFF, and then there's two options:
     * 1000vvvv cccnnnnn nntttttt NOTE. duration=(t<<5)ms (~2s max)
     * 1001vvcc cnnnnnnn          FIREFORGET. Same as NOTE but duration zero (and coarser velocity).
     */
    if (event->opcode==MIDI_OPCODE_NOTE_ON) {
      const struct eggrom_song_event *end=eggrom_song_find_note_off(ctx,eventp,event->chid,event->a);
      int duration=end?(end->time-event->time):0;
      duration>>=5; // Phrased in 32ms units.
      if (duration>63) duration=63;
      if (duration) { // NOTE
        if (sr_encode_u8(ctx->dst,0x80|(event->b>>3))<0) return -1;
        if (sr_encode_u8(ctx->dst,(event->chid<<5)|(event->a>>2))<0) return -1;
        if (sr_encode_u8(ctx->dst,(event->a<<6)|duration)<0) return -1;
      } else { // FIREFORGET
        if (sr_encode_u8(ctx->dst,0x90|((event->b>>3)&0x0c)|(event->chid>>1))<0) return -1;
        if (sr_encode_u8(ctx->dst,(event->chid<<7)|event->a)<0) return -1;
      }
    }
  }
  // One more run of DELAY, up to the last event's timestamp.
  int endtime=ctx->eventv[ctx->eventc-1].time;
  while (dsttime<endtime) {
    int delaytime=endtime-dsttime;
    if (delaytime>63) delaytime=63;
    if (sr_encode_u8(ctx->dst,delaytime)<0) return -1;
    dsttime+=delaytime;
  }
  return 0;
}

/* Egg Song from MIDI, in context.
 */
 
static int eggrom_song_compile_midi_inner(struct eggrom_song_context *ctx) {
  for (;;) {
    struct midi_event srcevt={0};
    int err=midi_file_next(&srcevt,ctx->midi);
    if (err<0) break;
    if (err) {
      if (midi_file_advance(ctx->midi,err)<0) {
        fprintf(stderr,"%s: Error reading MIDI file.\n",ctx->res->path);
        return -2;
      }
      ctx->time+=err;
    } else if (eggrom_song_should_keep_event(ctx,&srcevt)) {
      struct eggrom_song_event *dstevt=eggrom_song_add_event(ctx);
      if (!dstevt) return -1;
      dstevt->time=ctx->time;
      dstevt->chid=srcevt.chid;
      dstevt->opcode=srcevt.opcode;
      dstevt->a=srcevt.a;
      dstevt->b=srcevt.b;
    }
  }
  if (!midi_file_is_finished(ctx->midi)) {
    fprintf(stderr,"%s: Error reading MIDI file.\n",ctx->res->path);
    return -2;
  }
  if (ctx->hichidc) {
    fprintf(stderr,"%s:WARNING: Discarding %d events from channels >=8; you only get 8 channels in Egg.\n",ctx->res->path,ctx->hichidc);
  }
  int err=eggrom_song_emit_header(ctx);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error outputting header.\n",ctx->res->path);
    return -2;
  }
  if ((err=eggrom_song_emit_events(ctx))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error processing events.\n",ctx->res->path);
    return -2;
  }
  return 0;
}

/* Egg Song from MIDI.
 */
 
static int eggrom_song_compile_midi(struct sr_encoder *dst,const struct romw_res *res) {
  struct eggrom_song_context ctx={
    .dst=dst,
    .res=res,
    .usperqnote=500000,
    .drumchid=-1,
  };
  if (!(ctx.midi=midi_file_new(res->serial,res->serialc,1000))) {
    fprintf(stderr,"%s: Error decoding %d-byte MIDI file.\n",res->path,res->serialc);
    eggrom_song_context_cleanup(&ctx);
    return -2;
  }
  int err=eggrom_song_compile_midi_inner(&ctx);
  eggrom_song_context_cleanup(&ctx);
  return err;
}

/* Reencode song.
 */
 
int eggrom_song_compile(struct sr_encoder *dst,const struct romw_res *res) {
  fprintf(stderr,"%s %s\n",__func__,res->path);
  if ((res->serialc>=4)&&!memcmp(res->serial,"MThd",4)) {
    return eggrom_song_compile_midi(dst,res);
  } else if ((res->serialc>=4)&&!memcmp(res->serial,"\xbe\xee\xeeP",4)) {
    // Already in Egg Song format, keep it.
    return 0;
  } else {
    fprintf(stderr,"%s: Unknown song format. Expected MIDI or Egg Song.\n",res->path);
    return -2;
  }
}
