/* SysExtra.js
 * Little helpers for the public API. Log, date, language...
 */
 
export class SysExtra {
  constructor(window) {
    this.window = window;
  }
  
  /* Returns:
   * {
   *   srcc: Length of (src) consumed.
   *   align: -1|0|1.
   *   fill: ' ' or '0'.
   *   len: integer or "*" to read from vargs.
   *   prec: integer or "*" to read from vargs, <0 if unspecified.
   *   value: null, you fill in from vargs.
   *   spec: Single character.
   * }
   * Or null if malformed.
   */
  parseLogFormatUnit(src, srcp) {
    const srcp0 = srcp;
    if (srcp >= src.length) return null;
    if (src[srcp++] !== '%') return null;
    
    let align = 1;
    if (src[srcp] === '-') {
      align = -1;
      srcp++;
    } else if (src[srcp] === '+') {
      align = 1;
      srcp++;
    } else if (src[srcp] === '=') {
      align = 0;
      srcp++;
    }
    
    let fill = ' ';
    if (src[srcp] === '0') {
      fill = '0';
      srcp++;
    }
    
    let len = 0;
    if (src[srcp] === '*') {
      len = '*';
      srcp++;
    } else while ((src[srcp] >= '0') && (src[srcp] <= '9')) {
      len *= 10;
      len += src.charCodeAt(srcp) - 0x30;
      srcp++;
    }
    
    let prec = -1;
    if (src[srcp] === '.') {
      srcp++;
      if (src[srcp] === '*') {
        prec = '*';
        srcp++;
      } else {
        prec = 0;
        while ((src[srcp] >= '0') && (src[srcp] <= '9')) {
          prec *= 10;
          prec += src.charCodeAt(srcp) - 0x30;
          srcp++;
        }
      }
    }
    
    let spec;
    switch (src[srcp]) {
      case 'd':
      case 'x':
      case 'p':
      case 'l':
      case 'f':
      case 's':
      case 'c': {
          spec = src[srcp];
          srcp++;
        } break;
      default: return null;
    }
    return { srcc: srcp - srcp0, fill, align, len, prec, spec, value: null };
  }
  
  applyLogFormatUnit(unit) {
    let pre = "";
    switch (unit.spec) {
      case 'u': pre = (~~unit.value || 0).toString(); break;
      case 'd': pre = (~~unit.value || 0).toString(); break;
      case 'x': pre = (~~unit.value || 0).toString(16); break;
      case 'p': pre = (~~unit.value || 0).toString(16); break;
      case 'l': pre = (~~unit.value || 0).toString(); break;
      case 'f': pre = (+unit.value).toString(); break; // TODO prec
      case 's': pre = (unit.value || "").toString(); if (unit.prec >= 0) pre = pre.substring(0, unit.prec); break;
      case 'c': pre = String.fromCharCode(~~unit.value || 0x3f); break;
    }
    let spaces = "                              ";
    if (unit.fill === '0') {
      spaces = "0000000000000000000000000000000000";
    }
    if (pre.length < unit.len) switch (unit.align) {
      case -1: pre += spaces.substring(0, unit.len - pre.length); break;
      case 0: {
          const addc = unit.len - pre.length;
          const each = addc >> 1;
          const more = addc & 1;
          pre = spaces.substring(0, each) + pre +spaces.substring(0, each + more);
        } break;
      case 1: pre = spaces.substring(0, unit.len - pre.length) + pre; break;
    }
    return pre;
  }
  
  applyLogFormat(fmt, vargs) {
    if (!fmt) return "";
    if (!vargs) return fmt;
    let fmtp=0, vargsp=0, dst="";
    while (fmtp < fmt.length) {
      let nextp = fmt.indexOf('%', fmtp);
      if (nextp < 0) nextp = fmt.length;
      if (nextp > fmtp) {
        dst += fmt.substring(fmtp, nextp);
        fmtp = nextp;
        if (fmtp >= fmt.length) break;
      }
      if (fmt[fmtp + 1] === '%') {
        dst += "%";
        fmtp += 2;
        continue;
      }
      const unit = this.parseLogFormatUnit(fmt, fmtp);
      if (!unit) {
        dst += fmt[fmtp++];
        continue;
      }
      fmtp += unit.srcc;
      if (typeof(vargs) === "function") {
        if (unit.len === "*") unit.len = vargs('u');
        if (unit.prec === "*") unit.len = vargs('u');
        unit.value = vargs(unit.spec);
      } else {
        if (unit.len === "*") unit.len = vargs[vargsp++] || 0;
        if (unit.prec === "*") unit.prec = vargs[vargsp++] || 0;
        unit.value = vargs[vargsp++];
      }
      dst += this.applyLogFormatUnit(unit);
    }
    return dst;
  }
  
  /* (vargs) can be an array of values or an iterator function: (spec:[udxplfsc]) => any
   */
  log(fmt, vargs) {
    console.log("GAME: " + this.applyLogFormat(fmt, vargs));
  }
  
  store_set(k, v) {
    //TODO User-imposed storage limits.
    this.window.localStorage.setItem(k, v);
    return 0;
  }
  
  store_get(k) {
    return this.window.localStorage.getItem(k);
  }
  
  store_key_by_index(p) {
    return this.window.localStorage.key(p) || "";
  }
  
  // The related time_real() is implemented directly by Runtime.
  time_get() {
    const d = new Date();
    return {
      year: d.getFullYear(),
      month: 1 + d.getMonth(),
      day: d.getDate(),
      hour: d.getHours(),
      minute: d.getMinutes(),
      second: d.getSeconds(),
      milli: d.getMilliseconds(),
    };
  }
  
  get_user_languages() {
    let list = this.window.navigator.languages;
    if (list && (list.length > 0)) {
      list = Array.from(new Set(list.map(l => this.evalLang(l)).filter(v => v)));
    }
    if (!list) list = [];
    if (!list.length) {
      const code = this.evalLang(this.window.navigator.language);
      if (code) list.push(code);
    }
    return list;
  }
  
  evalLang(src) {
    if (!src) return 0;
    if (src.length >= 2) {
      const a = src.charCodeAt(0);
      const b = src.charCodeAt(1);
      if ((a >= 0x61) && (a <= 0x7a) && (b >= 0x61) && (b <= 0x7a)) {
        return (a << 8) | b;
      }
    }
    return 0;
  }
}
