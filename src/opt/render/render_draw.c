#include "render_internal.h"

/* raw
 */
 
static const char render_raw_vsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform vec2 screensize;\n"
  "attribute vec2 apos;\n"
  "attribute vec4 acolor;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "npos.y=-npos.y;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vcolor=acolor;\n"
  "}\n"
"";

static const char render_raw_fsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "gl_FragColor=vcolor;\n"
  "}\n"
"";

/* decal
 */
 
static const char render_decal_vsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform vec2 screensize;\n"
  "attribute vec2 apos;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "npos.y=-npos.y;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vtexcoord=atexcoord;\n"
  "}\n"
"";

static const char render_decal_fsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "uniform float alpha;\n"
  "uniform vec4 tint;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "gl_FragColor=texture2D(sampler,vtexcoord);\n"
    "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n"
  "}\n"
"";

/* tile
 */
 
static const char render_tile_vsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform vec2 screensize;\n"
  "uniform float pointsize;\n"
  "attribute vec2 apos;\n"
  "attribute float atileid;\n"
  "attribute float axform;\n"
  "varying vec2 vsrcp;\n"
  "varying mat2 vmat;\n"
  "void main() {\n"
    "vec2 npos=(apos*2.0)/screensize-1.0;\n"
    "npos.y=-npos.y;\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vsrcp=vec2(\n"
      "mod(atileid,16.0),\n"
      "floor(atileid/16.0)\n"
    ")/16.0;\n"
         "if (axform<0.5) vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" // no xform
    "else if (axform<1.5) vmat=mat2(-1.0, 0.0, 0.0, 1.0);\n" // XREV
    "else if (axform<2.5) vmat=mat2( 1.0, 0.0, 0.0,-1.0);\n" // YREV
    "else if (axform<3.5) vmat=mat2(-1.0, 0.0, 0.0,-1.0);\n" // XREV|YREV
    "else if (axform<4.5) vmat=mat2( 0.0, 1.0, 1.0, 0.0);\n" // SWAP
    "else if (axform<5.5) vmat=mat2( 0.0, 1.0,-1.0, 0.0);\n" // SWAP|XREV
    "else if (axform<6.5) vmat=mat2( 0.0,-1.0, 1.0, 0.0);\n" // SWAP|YREV
    "else if (axform<7.5) vmat=mat2( 0.0,-1.0,-1.0, 0.0);\n" // SWAP|XREV|YREV
                    "else vmat=mat2( 1.0, 0.0, 0.0, 1.0);\n" // invalid; use identity
    "gl_PointSize=pointsize;\n"
  "}\n"
"";

static const char render_tile_fsrc[]=
  "#version 100\n"
  "precision mediump float;\n"
  "uniform sampler2D sampler;\n"
  "uniform float alpha;\n"
  "uniform vec4 tint;\n"
  "varying vec2 vsrcp;\n"
  "varying mat2 vmat;\n"
  "void main() {\n"
    "vec2 texcoord=gl_PointCoord;\n"
    "texcoord=vmat*(texcoord-0.5)+0.5;\n"
    "texcoord=vsrcp+texcoord/16.0;\n"
    "gl_FragColor=texture2D(sampler,texcoord);\n"
    "gl_FragColor=vec4(mix(gl_FragColor.rgb,tint.rgb,tint.a),gl_FragColor.a*alpha);\n"
  "}\n"
"";

/* Compile half of one program.
 * <0 for error.
 */
 
static int render_program_compile(struct render *render,const char *name,int pid,int type,const GLchar *src,GLint srcc) {
  GLint sid=glCreateShader(type);
  if (!sid) return -1;
  glShaderSource(sid,1,&src,&srcc);
  glCompileShader(sid);
  GLint status=0;
  glGetShaderiv(sid,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(pid,sid);
    glDeleteShader(sid);
    return 0;
  }
  GLuint loga=0,logged=0;
  glGetShaderiv(sid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetShaderInfoLog(sid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to compile %s shader for program '%s':\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to compile %s shader for program '%s', no further detail available.\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name);
  glDeleteShader(sid);
  return -1;
}

/* Initialize one program.
 * Zero for error.
 */
 
static int render_program_init(
  struct render *render,const char *name,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc
) {
  GLint pid=glCreateProgram();
  if (!pid) return 0;
  if (render_program_compile(render,name,pid,GL_VERTEX_SHADER,vsrc,vsrcc)<0) return 0;
  if (render_program_compile(render,name,pid,GL_FRAGMENT_SHADER,fsrc,fsrcc)<0) return 0;
  glLinkProgram(pid);
  GLint status=0;
  glGetProgramiv(pid,GL_LINK_STATUS,&status);
  if (status) return pid;
  GLuint loga=0,logged=0;
  glGetProgramiv(pid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetProgramInfoLog(pid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to link GLSL program '%s':\n%.*s\n",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to link GLSL program '%s', no further detail available.\n",name);
  glDeleteProgram(pid);
  return 0;
}

/* Initialize programs.
 */
 
int render_init_programs(struct render *render) {
  #define PGM1(tag,...) \
    if (!(render->pgm_##tag=render_program_init( \
      render,#tag, \
      render_##tag##_vsrc,sizeof(render_##tag##_vsrc)-1, \
      render_##tag##_fsrc,sizeof(render_##tag##_fsrc)-1 \
    ))) return -1;
  PGM1(raw)
  PGM1(decal)
  PGM1(tile)
  #undef PGM1
  
  glUseProgram(render->pgm_raw);
  render->u_raw_screensize=glGetUniformLocation(render->pgm_raw,"screensize");
  glBindAttribLocation(render->pgm_raw,0,"apos");
  glBindAttribLocation(render->pgm_raw,1,"acolor");
  
  glUseProgram(render->pgm_decal);
  render->u_decal_screensize=glGetUniformLocation(render->pgm_decal,"screensize");
  render->u_decal_sampler=glGetUniformLocation(render->pgm_decal,"sampler");
  render->u_decal_alpha=glGetUniformLocation(render->pgm_decal,"alpha");
  render->u_decal_tint=glGetUniformLocation(render->pgm_decal,"tint");
  glBindAttribLocation(render->pgm_decal,0,"apos");
  glBindAttribLocation(render->pgm_decal,1,"atexcoord");
  
  glUseProgram(render->pgm_tile);
  render->u_tile_screensize=glGetUniformLocation(render->pgm_tile,"screensize");
  render->u_tile_sampler=glGetUniformLocation(render->pgm_tile,"sampler");
  render->u_tile_alpha=glGetUniformLocation(render->pgm_tile,"alpha");
  render->u_tile_tint=glGetUniformLocation(render->pgm_tile,"tint");
  render->u_tile_pointsize=glGetUniformLocation(render->pgm_tile,"pointsize");
  glBindAttribLocation(render->pgm_tile,0,"apos");
  glBindAttribLocation(render->pgm_tile,1,"atileid");
  glBindAttribLocation(render->pgm_tile,2,"axform");
  
  return 0;
}

/* Clear.
 */

void render_texture_clear(struct render *render,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if (render_texture_require_fb(texture)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

/* Set globals.
 */

void render_draw_mode(struct render *render,int xfermode,uint32_t tint,uint8_t alpha) {
  switch (xfermode) {
    case EGG_XFERMODE_ALPHA: glEnable(GL_BLEND); break;
    case EGG_XFERMODE_OPAQUE: glDisable(GL_BLEND); break;
  }
  render->tint=tint;
  render->alpha=alpha;
}

/* Flat rect.
 */

void render_draw_rect(struct render *render,int texid,int x,int y,int w,int h,uint32_t pixel) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  uint8_t r=pixel>>24,g=pixel>>16,b=pixel>>8,a=pixel;
  if (render->alpha<0xff) a=(a*render->alpha)>>8;
  if (!a) return;
  if (render_texture_require_fb(texture)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glUseProgram(render->pgm_raw);
  glViewport(0,0,texture->w,texture->h);
  glUniform2f(render->u_raw_screensize,texture->w,texture->h);
  struct render_vertex_raw vtxv[]={
    {x  ,y  ,r,g,b,a},
    {x,  y+h,r,g,b,a},
    {x+w,y  ,r,g,b,a},
    {x+w,y+h,r,g,b,a},
  };
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_raw),&vtxv[0].x);
  glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct render_vertex_raw),&vtxv[0].r);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

/* Decal.
 */

void render_draw_decal(
  struct render *render,
  int dsttexid,int srctexid,
  int dstx,int dsty,
  int srcx,int srcy,
  int w,int h,
  int xform
) {
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  if ((srctexid<1)||(srctexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  struct render_texture *srctex=render->texturev+srctexid-1;
  if ((srctex->w<1)||(srctex->h<1)) return;
  if (render_texture_require_fb(dsttex)<0) return;
  int dstw=w,dsth=h;
  if (xform&EGG_XFORM_SWAP) {
    dstw=h;
    dsth=w;
  }
  struct render_vertex_decal vtxv[]={
    {dstx     ,dsty     ,0.0f,0.0f},
    {dstx     ,dsty+dsth,0.0f,1.0f},
    {dstx+dstw,dsty     ,1.0f,0.0f},
    {dstx+dstw,dsty+dsth,1.0f,1.0f},
  };
  if (xform&EGG_XFORM_SWAP) {
    struct render_vertex_decal *vtx=vtxv;
    int i=4; for (;i-->0;vtx++) {
      GLfloat tmp=vtx->tx;
      vtx->tx=vtx->ty;
      vtx->ty=tmp;
    }
  }
  if (xform&EGG_XFORM_XREV) {
    struct render_vertex_decal *vtx=vtxv;
    int i=4; for (;i-->0;vtx++) vtx->tx=1.0f-vtx->tx;
  }
  if (xform&EGG_XFORM_YREV) {
    struct render_vertex_decal *vtx=vtxv;
    int i=4; for (;i-->0;vtx++) vtx->ty=1.0f-vtx->ty;
  }
  {
    GLfloat tx0=(GLfloat)srcx/(GLfloat)srctex->w;
    GLfloat tx1=(GLfloat)w/(GLfloat)srctex->w;
    GLfloat ty0=(GLfloat)srcy/(GLfloat)srctex->h;
    GLfloat ty1=(GLfloat)h/(GLfloat)srctex->h;
    struct render_vertex_decal *vtx=vtxv;
    int i=4; for (;i-->0;vtx++) {
      vtx->tx=tx0+tx1*vtx->tx;
      vtx->ty=ty0+ty1*vtx->ty;
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUseProgram(render->pgm_decal);
  glUniform2f(render->u_decal_screensize,dsttex->w,dsttex->h);
  glBindTexture(GL_TEXTURE_2D,srctex->texid);
  glUniform4f(render->u_decal_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_decal_alpha,render->alpha/255.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_decal),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct render_vertex_decal),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

/* Tiles.
 */

void render_draw_tile(
  struct render *render,
  int dsttexid,int srctexid,
  const struct egg_draw_tile *v,int c
) {
  if ((dsttexid<1)||(dsttexid>render->texturec)) return;
  if ((srctexid<1)||(srctexid>render->texturec)) return;
  struct render_texture *dsttex=render->texturev+dsttexid-1;
  struct render_texture *srctex=render->texturev+srctexid-1;
  if (render_texture_require_fb(dsttex)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
  glViewport(0,0,dsttex->w,dsttex->h);
  glUseProgram(render->pgm_tile);
  glUniform2f(render->u_tile_screensize,dsttex->w,dsttex->h);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(render->u_tile_sampler,0);
  glBindTexture(GL_TEXTURE_2D,srctex->texid);
  glUniform4f(render->u_tile_tint,(render->tint>>24)/255.0f,((render->tint>>16)&0xff)/255.0f,((render->tint>>8)&0xff)/255.0f,(render->tint&0xff)/255.0f);
  glUniform1f(render->u_tile_alpha,render->alpha/255.0f);
  glUniform1f(render->u_tile_pointsize,srctex->w>>4);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_draw_tile),&v[0].x);
  glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_draw_tile),&v[0].tileid);
  glVertexAttribPointer(2,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_draw_tile),&v[0].xform);
  glDrawArrays(GL_POINTS,0,c);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(2);
}

/* Draw to main.
 */
 
void render_draw_to_main(struct render *render,int mainw,int mainh,int texid) {
  if ((texid<1)||(texid>render->texturec)) return;
  struct render_texture *texture=render->texturev+texid-1;
  if ((texture->w<1)||(texture->h<1)) return;
  
  int dstx=0,dsty=0,w=mainw,h=mainh;
  int xscale=mainw/texture->w;
  int yscale=mainh/texture->h;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) scale=1;
  if (scale<3) { // Debatable: Below 3x, scale only to multiples of the framebuffer size, leaving a border.
    w=texture->w*scale;
    h=texture->h*scale;
  } else { // If scaling large enough, cover one axis and let pixels end up different sizes.
    int wforh=(texture->w*mainh)/texture->h;
    if (wforh<=mainw) {
      w=wforh;
    } else {
      h=(texture->h*mainw)/texture->w;
    }
  }
  dstx=(mainw>>1)-(w>>1);
  dsty=(mainh>>1)-(h>>1);
  render->outx=dstx;
  render->outy=dsty;
  render->outw=w;
  render->outh=h;
  
  struct render_vertex_decal vtxv[]={
    {dstx  ,dsty  ,0.0f,1.0f},
    {dstx  ,dsty+h,0.0f,0.0f},
    {dstx+w,dsty  ,1.0f,1.0f},
    {dstx+w,dsty+h,1.0f,0.0f},
  };
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glViewport(0,0,mainw,mainh);
  if ((w<mainw)||(h<mainh)) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glUseProgram(render->pgm_decal);
  glUniform2f(render->u_decal_screensize,mainw,mainh);
  glUniform1i(render->u_decal_sampler,0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,texture->texid);
  glDisable(GL_BLEND);
  glUniform4f(render->u_decal_tint,0.0f,0.0f,0.0f,0.0f);
  glUniform1f(render->u_decal_alpha,1.0f);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct render_vertex_decal),&vtxv[0].x);
  glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(struct render_vertex_decal),&vtxv[0].tx);
  glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}
