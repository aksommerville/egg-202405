#include "eggrom_internal.h"

/* Expand string resource, main entry point.
 */
 
int eggrom_string_expand(const char *src,int srcc,uint16_t qual,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  int lineno=0,linec;
  const char *line;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    lineno++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    struct romw_res *res=romw_res_add(eggrom.romw);
    if (!res) return -1;
    res->tid=EGG_TID_string;
    res->qual=qual;
    res->lineno=lineno;
    
    int linep=0;
    const char *k=line+linep;
    int kc=0;
    while ((linep<linec)&&((unsigned char)line[linep]>0x20)) { linep++; kc++; }
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    
    if (!kc) {
      fprintf(stderr,"%s:%d: Malformed line, expected 'ID TEXT'\n",path,lineno);
      return -2;
    }
    if ((k[0]>='0')&&(k[0]<='9')) {
      int v;
      if ((sr_int_eval(&v,k,kc)<2)||(v<1)||(v>0xffff)) {
        fprintf(stderr,"%s:%d: Malformed ID '%.*s', expected integer in 1..65535\n",path,lineno,kc,k);
        return -2;
      }
      res->rid=v;
    } else {
      if (romw_res_set_name(res,k,kc)<0) return -1;
    }
    
    if ((linep<linec)&&(line[linep]=='"')&&(line[linec-1]=='"')) {
      char tmp[1024];
      int tmpc=sr_string_eval(tmp,sizeof(tmp),line+linep,linec-linep);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) {
        fprintf(stderr,"%s:%d: Malformed string token\n",path,lineno);
        return -2;
      }
      if (romw_res_set_serial(res,tmp,tmpc)<0) return -1;
    } else {
      if (romw_res_set_serial(res,line+linep,linec-linep)<0) return -1;
    }
  }
  return 0;
}
