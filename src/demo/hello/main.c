void egg_log(const char *fmt,...);

void egg_client_quit() {
  egg_log("egg_client_quit");
}

/*
int egg_client_init() {
  egg_log("egg_client_init");
  return 0;
}

void egg_client_update(double elapsed) {
  //egg_log("egg_client_update %f",elapsed);
}
*/

void egg_client_render() {
  //egg_log("string=%s int=%d float=%f char=%c",__func__,123,-543.21,'!');
}
