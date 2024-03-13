import * as egg from "egg";

egg.log("js:1 outer module");

function dumpResource(tid, qual, rid) {
  const serial = egg.res_get(tid, qual, rid);
  egg.log("dumpResource(%d,%d,%d): type=%s length=%d",tid, qual, rid, typeof(serial), serial?.byteLength);
  if ((serial instanceof ArrayBuffer) && (serial.byteLength >= 8)) {
    const u = new Uint8Array(serial);
    egg.log(">> %02x %02x %02x %02x %02x %02x %02x %02x...", u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8]);
  }
}

function egg_client_quit() {
  egg.log("(js)egg_client_quit");
}

function egg_client_init() {
  try {
    egg.log("(js)egg_client_init");
    const [w, h] = egg.video_get_size();
    egg.log("video size: %d x %d",w,h);
    const eventEnablement = egg.event_enable(8, 3); // MMOTION,ENABLE. These are compile-time constants; I don't plan to export at runtime.
    egg.log("Enable mmotion? %d", eventEnablement);
    egg.audio_play_song(1, false, true);
    egg.audio_play_sound(2, 0.5, -0.125);
    dumpResource(5, 0x656e, 2); // string:en:2, should be "Hello world"
    dumpResource(99, 0, 1);
    egg.log("Languages: %s",JSON.stringify(egg.get_user_languages()));
    egg.log("Current local time: %s",JSON.stringify(egg.time_get()));
  } catch (e) {
    egg.log("Caught error during egg_client_init: %s",e.toString());
    return -1;
  }
  return 0;
}

function egg_client_update(elapsed) {
  //egg.log("(js)egg_client_update %f",elapsed);
  for (const event of egg.event_next()) {
    //egg.log("event: %s", JSON.stringify(event));
  }
}

function egg_client_render() {
  //egg.log("(js)egg_client_render");
}

exportModule({
  egg_client_init,
  egg_client_quit,
  egg_client_update,
  egg_client_render,
});
