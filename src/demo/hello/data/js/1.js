import { log } from "egg";

log("js:1 outer module");
exportModule({
  egg_client_init: () => {},
  egg_client_update: v => {},
});
