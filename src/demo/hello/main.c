#include "egg/egg.h"
#include "GLES2/gl2.h"

struct vertex {
  GLfloat x,y;
  uint8_t r,g,b,a;
};

static GLuint glprogram=0;

void egg_client_quit() {
  egg_log("egg_client_quit");
}

static int compile_shader(int type,const char *src) {
  int shader=glCreateShader(type);
  int srcc=0; while (src[srcc]) srcc++;
  singleShaderSource(shader,1,src,srcc);
  glCompileShader(shader);
  glAttachShader(glprogram,shader);
  glDeleteShader(shader);
  return 0;
}

int egg_client_init() {
  egg_log("egg_client_init");
  
  if (!(glprogram=glCreateProgram())) return -1;
  if (compile_shader(GL_VERTEX_SHADER,
    "attribute vec2 aposition;\n"
    "attribute vec4 acolor;\n"
    "varying vec4 vcolor;\n"
    "void main() {\n"
      "gl_Position=vec4(aposition,0.0,1.0);\n"
      "vcolor=acolor;\n"
    "}\n"
  )<0) return -1;
  if (compile_shader(GL_FRAGMENT_SHADER,
    "varying vec4 vcolor;\n"
    "void main() {\n"
      "gl_FragColor=vcolor;\n"
    "}\n"
  )<0) return -1;
  glLinkProgram(glprogram);
  GLint status=0;
  glGetProgramiv(glprogram,GL_LINK_STATUS,&status);
  if (!status) {
    egg_log("GLSL LINK FAILED");
    char infolog[1024];
    GLsizei infologa=sizeof(infolog),infologc=0;
    glGetProgramInfoLog(glprogram,infologa,&infologc,infolog);
    if ((infologc<0)||(infologc>sizeof(infolog))) infologc=0;
    egg_log("%.*s",infologc,infolog);
    return -1;
  }
  glBindAttribLocation(glprogram,0,"aposition");
  glBindAttribLocation(glprogram,1,"acolor");
  
  return 0;
}

/**/
static void on_key(int keycode,int value) {
  if (value) switch (keycode) {
    case 0x00070029: egg_request_termination(); break; // ESC
    case 0x0007003a: { // F1
        if (egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_DISABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_MMOTION,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MBUTTON,EGG_EVTSTATE_ENABLED);
          egg_event_enable(EGG_EVENT_MWHEEL,EGG_EVTSTATE_ENABLED);
        }
      } break;
    case 0x0007003b: { // F2
        if (egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_QUERY)==EGG_EVTSTATE_ENABLED) {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_DISABLED);
        } else {
          egg_event_enable(EGG_EVENT_TEXT,EGG_EVTSTATE_ENABLED);
        }
      } break;
  }
}

static void on_input_connect(int devid) {
  egg_log("CONNECT %d",devid);
  char name[256];
  int namec=egg_input_device_get_name(name,sizeof(name),devid);
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  int vid=0,pid=0,version=0;
  egg_input_device_get_ids(&vid,&pid,&version,devid);
  egg_log("name='%.*s' vid=%04x pid=%04x version=%04x",namec,name,vid,pid,version);
  int btnix=0; for (;;btnix++) {
    int btnid=0,hidusage=0,lo=0,hi=0,value=0;
    egg_input_device_get_button(&btnid,&hidusage,&lo,&hi,&value,devid,btnix);
    if (!btnid) break;
    egg_log("  %d 0x%08x %d..%d =%d",btnid,hidusage,lo,hi,value);
  }
}

void egg_client_update(double elapsed) {
  //egg_log("egg_client_update %f",elapsed);
  //egg_request_termination();
  struct egg_event eventv[32];
  int eventc=egg_event_next(eventv,sizeof(eventv)/sizeof(eventv[0]));
  const struct egg_event *event=eventv;
  for (;eventc-->0;event++) switch (event->type) {
    case EGG_EVENT_INPUT: egg_log("INPUT %d.%d=%d",event->v[0],event->v[1],event->v[2]); break;
    case EGG_EVENT_CONNECT: on_input_connect(event->v[0]); break;
    case EGG_EVENT_DISCONNECT: egg_log("DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_HTTP_RSP: {
        egg_log("HTTP_RSP reqid=%d status=%d length=%d zero=%d",event->v[0],event->v[1],event->v[2],event->v[3]);
        char body[1024];
        if (event->v[2]<=sizeof(body)) {
          if (egg_http_get_body(body,sizeof(body),event->v[0])==event->v[2]) {
            egg_log(">> %.*s",event->v[2],body);
          }
        }
      } break;
    case EGG_EVENT_WS_CONNECT: egg_log("WS_CONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_DISCONNECT: egg_log("WS_DISCONNECT %d",event->v[0]); break;
    case EGG_EVENT_WS_MESSAGE: {
        char msg[1024];
        if (event->v[3]<=sizeof(msg)) {
          if (egg_ws_get_message(msg,sizeof(msg),event->v[0],event->v[1])==event->v[3]) {
            egg_log(">> %.*s",event->v[3],msg);
          }
        }
      } break;
    case EGG_EVENT_MMOTION: egg_log("MMOTION %d,%d",event->v[0],event->v[1]); break;
    case EGG_EVENT_MBUTTON: egg_log("MBUTTON %d=%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_MWHEEL: egg_log("MWHEEL +%d,%d @%d,%d",event->v[0],event->v[1],event->v[2],event->v[3]); break;
    case EGG_EVENT_KEY: on_key(event->v[0],event->v[1]); break;
    case EGG_EVENT_TEXT: egg_log("TEXT U+%x",event->v[0]); break;
    case EGG_EVENT_RESIZE: egg_log("RESIZE %d,%d",event->v[0],event->v[1]); break;
    default: egg_log("UNKNOWN EVENT: %d[%d,%d,%d,%d]",event->type,event->v[0],event->v[1],event->v[2],event->v[3]);
  }
}
/**/

void egg_client_render() {
  int screenw=0,screenh=0;
  egg_video_get_size(&screenw,&screenh);
  glViewport(0,0,screenw,screenh);
  glClearColor(0.5f,0.25f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  {
    static struct vertex vertexv[]={
      {0.0f,0.5f,0xff,0x00,0x00,0xff}, // red on top
      {-0.5f,-0.5f,0x00,0xff,0x00,0xff}, // green lower left
      {0.5f,-0.5f,0x00,0x00,0xff,0xff}, // blue lower right
    };
    glUseProgram(glprogram);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,0,sizeof(struct vertex),&vertexv[0].x);
    glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct vertex),&vertexv[0].r);
    glDrawArrays(GL_TRIANGLES,0,sizeof(vertexv)/sizeof(vertexv[0]));
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);
  }
}
