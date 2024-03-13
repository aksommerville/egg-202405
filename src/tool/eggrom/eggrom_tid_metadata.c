#include "eggrom_internal.h"

/* Compile metadata.
 */
 
int eggrom_metadata_compile(struct sr_encoder *dst,const struct romw_res *res) {
  struct sr_decoder decoder={.v=res->serial,.c=res->serialc};
  int lineno=0,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&(line[linep]!=':')&&(line[linep]!='=')) { linep++; kc++; }
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    linep++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *v=line+linep;
    int vc=linec-linep;
    if (vc<0) vc=0;
    
    if (kc>0xff) {
      fprintf(stderr,"%s:%d: Key length %d, limit 255\n",res->path,lineno,kc);
      return -2;
    }
    if (vc>0xff) {
      fprintf(stderr,"%s:%d: Length %d for key '%.*s', limit 255\n",res->path,lineno,vc,kc,k);
      return -2;
    }
    if (sr_encode_u8(dst,kc)<0) return -1;
    if (sr_encode_u8(dst,vc)<0) return -1;
    if (sr_encode_raw(dst,k,kc)<0) return -1;
    if (sr_encode_raw(dst,v,vc)<0) return -1;
  }
  return 0;
}
