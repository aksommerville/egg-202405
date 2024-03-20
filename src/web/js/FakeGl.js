/* FakeGl.js
 * OpenGL ES 2 for web browsers!
 * Our WebAssembly games need it exposed like this, so they can also build natively.
 */
 
export class FakeGl {
  constructor(window, canvas, gl, wasm) {
    this.window = window;
    this.canvas = canvas;
    this.gl = gl; // Can be null initially, but owner must supply before doing anything useful.
    this.wasm = wasm;
    
    /* Most objects are live objects in WebGL, but integers in GLES:
     *  program, shader, texture, buffer, framebuffer, renderbuffer
     * We'll make up integers for the C side, indices in this array.
     * We don't track what kind of object each is.
     */
    this.o = [null];
  }
  
  createObject() {
    for (let i=1; ; i++) if (!this.o[i]) return i;
  }
  
  measureImage(w, h, fmt, type) {
    let chanc;
    switch (fmt) {
      case this.gl.ALPHA:
      case this.gl.LUMINANCE: chanc = 1; break;
      case this.gl.LUMINANCE_ALPHA: chanc = 2; break;
      case this.gl.RGB: chanc = 3; break;
      case this.gl.RGBA: chanc = 4; break;
      default: return -1;
    }
    let pixelsize;
    switch (type) {
      case GL_UNSIGNED_BYTE: pixelsize = chanc; break;
      default: return -1;
    }
    const stride = (w * pixelsize + 3) & ~3;
    return stride * h;
  }
  
  getExports() {
    return {
      glActiveTexture: (texid) => this.gl.activeTexture(this.o[texid]),
      glAttachShader: (p, s) => this.gl.attachShader(this.o[p], this.o[s]),
      glBindAttribLocation: (p, i, n) => this.gl.bindAttribLocation(this.o[p], i, this.wasm.zstringFromMemory(n)),
      glBindBuffer: (t, b) => this.gl.bindBuffer(t, this.o[b]),
      glBindFramebuffer: (t, b) => this.gl.bindFramebuffer(t, this.o[b]),
      glBindRenderbuffer: (t, b) => this.gl.bindRenderbuffer(t, this.o[b]),
      glBindTexture: (tar, tex) => this.gl.bindTexture(tar, this.o[tex]),
      glBlendColor: (r, g, b, a) => this.gl.blendColor(r, g, b, a),
      glBlendEquation: (i) => this.gl.blendEquation(i),
      glBlendEquationSeparate: (a, b) => this.gl.blendEquationSeparate(a, b),
      glBlendFunc: (s, d) => this.gl.blendFunc(s, d),
      glBlendFuncSeparate: (sc, dc, sa, da) => this.gl.blendFuncSeparate(sc, dc, sa, da),
      glBufferData: (a,b,c,d) => this._bufferData(a,b,c,d),
      glBufferSubData: (a,b,c,d) => this._bufferSubData(a,b,c,d),
      glCheckFramebufferStatus: (b) => this.gl.checkFramebufferStatus(this.o[b]),
      glClear: (m) => this.gl.clear(m),
      glClearColor: (r, g, b, a) => this.gl.clearColor(r, g, b, a),
      glClearDepthf: (d) => this.gl.clearDepth(d),
      glClearStencil: (s) => this.gl.clearStencil(s),
      glColorMask: (r, g, b, a) => this.gl.colorMask(r, g, b, a),
      glCompileShader: (s) => this.gl.compileShader(this.o[s]),
      glCompressedTexImage2D: (a,b,c,d,e,f,g,h) => this._compressedTexImage2D(a,b,c,d,e,f,g,h),
      glCompressedTexSubImage2D: (a,b,c,d,e,f,g,h,i) => this._compressedTexSubImage2D(a,b,c,d,e,f,g,h,i),
      glCopyTexImage2D: (a,b,c,d,e,f,g,h) => this._copyTexImage2D(a,b,c,d,e,f,g,h),
      glCopyTexSubImage2D: (a,b,c,d,e,f,g,h) => this._copyTexSubImage2D(a,b,c,d,e,f,g,h),
      glCreateProgram: () => this._createProgram(),
      glCreateShader: (t) => this._createShader(t),
      glCullFace: (f) => this.gl.cullFace(f),
      glDeleteBuffers: (a,b) => this._deleteBuffers(a,b),
      glDeleteFramebuffers: (a,b) => this._deleteFramebuffers(a,b),
      glDeleteProgram: (p) => this._deleteProgram(p),
      glDeleteRenderbuffers: (a,b) => this._deleteRenderbuffers(a,b),
      glDeleteShader: (s) => this._deleteShader(s),
      glDeleteTextures: (a,b) => this._deleteTextures(a,b),
      glDepthFunc: (f) => this.gl.depthFunc(f),
      glDepthMask: (m) => this.gl.depthMask(m),
      glDepthRangef: (n, f) => this.gl.depthRangef(n, f),
      glDetachShader: (p, s) => this.gl.detachShader(this.o[p], this.o[s]),
      glDisable: (f) => this.gl.disable(f),
      glDisableVertexAttribArray: (i) => this.gl.disableVertexAttribArray(i),
      glDrawArrays: (m, p, c) => this.gl.drawArrays(m, p, c),
      glDrawElements: (a,b,c,d) => this._drawElements(a,b,c,d),
      glEnable: (f) => this.gl.enable(f),
      glEnableVertexAttribArray: (i) => this.gl.enableVertexAttribArray(i),
      glFinish: () => this.gl.finish(),
      glFlush: () => this.gl.flush(),
      glFramebufferRenderbuffer: (t, a, rt, rb) => this.gl.framebufferRenderbuffer(t, a, rt, this.o[rb]),
      glFramebufferTexture2D: (t, a, tt, tex, l) => this.gl.framebufferTexture2D(t, a, tt, this.o[tex], l),
      glFrontFace: (f) => this.gl.frontFace(f),
      glGenBuffers: (a,b) => this._genBuffers(a,b),
      glGenerateMipmap: (t) => this.gl.generateMipMap(t),
      glGenFramebuffers: (a,b) => this._genFramebuffers(a,b),
      glGenRenderbuffers: (a,b) => this._genRenderbuffers(a,b),
      glGenTextures: (a,b) => this._genTextures(a,b),
      glGetActiveAttrib: (a,b,c,d,e,f,g) => this._getActiveAttrib(a,b,c,d,e,f,g),
      glGetActiveUniform: (a,b,c,d,e,f,g) => this._getActiveUniform(a,b,c,d,e,f,g),
      glGetAttachedShaders: (a,b,c,d) => this._getAttachedShaders(a,b,c,d),
      glGetAttribLocation: (p, n) => this.gl.getAttribLocation(this.o[p], this.wasm.zstringFromMemory(n)),
      glGetBooleanv: (a,b) => this._getBooleanv(a,b),
      glGetBufferParameteriv: (a,b,c) => this._getBufferParameteriv(a,b,c),
      glGetError: () => this.gl.getError(),
      glGetFloatv: (a,b) => this._getFloatv(a,b),
      glGetFramebufferAttachmentParameteriv: (a,b,c,d) => this._getFramebufferAttachmentParameteriv(a,b,c,d),
      glGetIntegerv: (a,b) => this._getIntegerv(a,b),
      glGetProgramiv: (a,b,c) => this._getProgramiv(a,b,c),
      glGetProgramInfoLog: (a,b,c,d) => this._getProgramInfoLog(a,b,c,d),
      glGetRenderbufferParameteriv: (a,b,c) => this._getRenderbufferParameteriv(a,b,c),
      glGetShaderiv: (a,b,c) => this._getShaderiv(a,b,c),
      glGetShaderInfoLog: (a,b,c,d) => this._getShaderInfoLog(a,b,c,d),
      glGetShaderPrecisionFormat: (a,b,c,d) => this._getShaderPrecisionFormat(a,b,c,d),
      glGetShaderSource: (a,b,c,d) => this._getShaderSource(a,b,c,d),
      glGetTexParameterfv: (a,b,c) => this._getTexParameterfv(a,b,c),
      glGetTexParameteriv: (a,b,c) => this._getTexParameteriv(a,b,c),
      glGetUniformfv: (a,b,c) => this._getUniformfv(a,b,c),
      glGetUniformiv: (a,b,c) => this._getUniformiv(a,b,c),
      glGetUniformLocation: (a,b) => this._getUniformLocation(a,b),
      glGetVertexAttribfv: (a,b,c) => this._getVertexAttribfv(a,b,c),
      glGetVertexAttribiv: (a,b,c) => this._getVertexAttribiv(a,b,c),
      singleGetVertexAttribPointerv: (a,b) => this._getVertexAttribPointerv(a,b),
      glHint: (t, m) => this.gl.hint(t, m),
      glIsBuffer: (i) => this.gl.isBuffer(this.o[i]),
      glIsEnabled: (f) => this.gl.isEnabled(f),
      glIsFramebuffer: (i) => this.gl.isFramebuffer(this.o[i]),
      glIsProgram: (i) => this.gl.isProgram(this.o[i]),
      glIsRenderbuffer: (i) => this.gl.isRenderbuffer(this.o[i]),
      glIsShader: (i) => this.gl.isShader(this.o[i]),
      glIsTexture: (i) => this.gl.isTexture(this.o[i]),
      glLineWidth: (w) => this.gl.lineWidth(w),
      glLinkProgram: (p) => this.gl.linkProgram(this.o[p]),
      glPixelStorei: (k, v) => this.gl.pixelStorei(k, v),
      glPolygonOffset: (f, u) => this.gl.polygonOffset(f, u),
      glReadPixels: (a,b,c,d,e,f,g) => this._readPixels(a,b,c,d,e,f,g),
      glReleaseShaderCompiler: () => this.gl.releaseShaderCompiler(),
      glRenderbufferStorage: (t, i, w, h) => this.gl.renderbufferStorage(t, i, w, h),
      glSampleCoverage: (v, i) => this.gl.sampleCoverage(v, i),
      glScissor: (x, y, w, h) => this.gl.scissor(x, y, w, h),
      glShaderBinary: (a,b,c,d,e) => this._shaderBinary(a,b,c,d,e),
      singleShaderSource: (a,b,c,d) => this._shaderSource(a,b,c,d),
      glStencilFunc: (f, r, m) => this.gl.stencilFunc(f, r, m),
      glStencilFuncSeparate: (f, fn, r, m) => this.gl.stencilFuncSeparate(f, fn, r, m),
      glStencilMask: (m) => this.gl.stencilMask(m),
      glStencilMaskSeparate: (f, m) => this.gl.stencilMaskSeparate(f, m),
      glStencilOp: (f, zf, zp) => this.gl.stencilOp(f, zf, zp),
      glStencilOpSeparate: (f, s, df, dp) => this.gl.stencilOpSeparate(f, s, df, dp),
      glTexImage2D: (a,b,c,d,e,f,g,h,i) => this._texImage2D(a,b,c,d,e,f,g,h,i),
      glTexParameterf: (t, k, v) => this.gl.texParameterf(t, k, v),
      glTexParameterfv: (a,b,c) => this._texParameterfv(a,b,c),
      glTexParameteri: (t, k, v) => this.gl.texParameteri(t, k, v),
      glTexParameteriv: (a,b,c) => this._texParameteriv(a,b,c),
      glTexSubImage2D: (a,b,c,d,e,f,g,h,i) => this._texSubImage2D(a,b,c,d,e,f,g,h,i),
      glUniform1f: (k, v) => this.gl.uniform1f(k, v),
      glUniform1fv: (a,b,c) => this._uniform1fv(a,b,c),
      glUniform1i: (k, v) => this.gl.uniform1i(k, v),
      glUniform1iv: (a,b,c) => this._uniform2iv(a,b,c),
      glUniform2f: (k, v0, v1) => this.gl.uniform2f(k, v0, v1),
      glUniform2fv: (a,b,c) => this._uniform2fv(a,b,c),
      glUniform2i: (k, v0, v1) => this.gl.uniform2i(k, v0, v1),
      glUniform2iv: (a,b,c) => this._uniform2iv(a,b,c),
      glUniform3f: (k, v0, v1, v2) => this.gl.uniform3f(k, v0, v1, v2),
      glUniform3fv: (a,b,c) => this._uniform3fv(a,b,c),
      glUniform3i: (k, v0, v1, v2) => this.gl.uniform3i(k, v0, v1, v2),
      glUniform3iv: (a,b,c) => this._uniform3iv(a,b,c),
      glUniform4f: (k, v0, v1, v2, v3) => this.gl.uniform4f(k, v0, v1, v2, v3),
      glUniform4fv: (a,b,c) => this._uniform4fv(a,b,c),
      glUniform4i: (k, v0, v1, v2, v3) => this.gl.uniform4i(k, v0, v1, v2, v3),
      glUniform4iv: (a,b,c) => this._uniform4iv(a,b,c),
      glUniformMatrix2fv: (a,b,c,d) => this._uniformMatrix2fv(a,b,c,d),
      glUniformMatrix3fv: (a,b,c,d) => this._uniformMatrix3fv(a,b,c,d),
      glUniformMatrix4fv: (a,b,c,d) => this._uniformMatrix4fv(a,b,c,d),
      glUseProgram: (p) => this.gl.useProgram(this.o[p]),
      glValidateProgram: (p) => this.gl.validateProgram(this.o[p]),
      glVertexAttrib1f: (k, v) => this.gl.vertexAttrib1f(k, v),
      glVertexAttrib1fv: (a,b) => this._vertexAttrib1fv(a,b),
      glVertexAttrib2f: (k, v0, v1) => this.gl.vertexAttrib2f(k, v0, v1),
      glVertexAttrib2fv: (a,b) => this._vertexAttrib2fv(a,b),
      glVertexAttrib3f: (k, v0, v1, v2) => this.gl.vertexAttrib3f(k, v0, v1, v2),
      glVertexAttrib3fv: (a,b) => this._vertexAttrib3fv(a,b),
      glVertexAttrib4f: (k, v0, v1, v2, v3) => this.gl.vertexAttrib4f(k, v0, v1, v2, v3),
      glVertexAttrib4fv: (a,b) => this._vertexAttrib4fv(a,b),
      glVertexAttribPointer: (a,b,c,d,e,f) => this._vertexAttribPointer(a,b,c,d,e,f),
      glViewport: (x, y, w, h) => this.gl.viewport(x, y, w, h),
    };
  }
  
  _bufferData(target, size, data, usage) {
    this.gl.bufferData(target, this.wasm.getMemoryView(data, size), usage);
  }
  
  _bufferSubData(target, offset, size, data) {
    this.gl.bufferSubData(target, offset, this.wasm.getMemoryView(data, size));
  }
  
  _compressedTexImage2D(target, level, ifmt, w, h, border, size, data) {
    //TODO
  }
  
  _compressedTexSubImage2D(target, level, x, y, w, h, format, size, data) {
    //TODO
  }
  
  _copyTexImage2D(target, level, ifmt, x, y, w, h, border) {
    //TODO
  }
  
  _copyTexSubImage2D(target, level, xo, yo, x, y, w, h) {
    //TODO
  }
  
  _createProgram() {
    const program = this.gl.createProgram();
    if (!program) return 0;
    const id = this.createObject();
    this.o[id] = program;
    return id;
  }
  
  _createShader(type) {
    const shader = this.gl.createShader(type);
    if (!shader) return 0;
    const id = this.createObject();
    this.o[id] = shader;
    return id;
  }
  
  _deleteBuffers(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.wasm.memU32[v];
      this.gl.deleteBuffer(this.o[id]);
      this.o[id] = null;
    }
  }
  
  _deleteFramebuffers(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.wasm.memU32[v];
      this.gl.deleteFramebuffer(this.o[id]);
      this.o[id] = null;
    }
  }
  
  _deleteProgram(i) {
    const program = this.o[i];
    if (!program) return;
    this.gl.deleteProgram(program);
    this.o[i] = null;
  }
  
  _deleteRenderbuffers(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.wasm.memU32[v];
      this.gl.deleteRenderbuffer(this.o[id]);
      this.o[id] = null;
    }
  }
  
  _deleteShader(i) {
    const shader = this.o[i];
    if (!shader) return;
    this.gl.deleteShader(shader);
    this.o[i] = null;
  }
  
  _deleteTextures(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.wasm.memU32[v];
      this.gl.deleteTexture(this.o[id]);
      this.o[id] = null;
    }
  }
  
  _drawElements(mode, count, type, indices) {
    //TODO
  }
  
  _genBuffers(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.createObject();
      this.wasm.memU32[v] = id;
      this.o[id] = this.gl.createBuffer();
    }
  }
  
  _genFramebuffers(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.createObject();
      this.wasm.memU32[v] = id;
      this.o[id] = this.gl.createFramebuffer();
    }
  }
  
  _genTextures(c, v) {
    v >>= 2;
    for (; c-->0; v++) {
      const id = this.createObject();
      this.wasm.memU32[v] = id;
      this.o[id] = this.gl.createTexture();
    }
  }
  
  writeActiveInfoToWasm(at, bufSize, length, size, type, name) {
    this.wasm.memU32[size >> 2] = at.size;
    this.wasm.memU32[type >> 2] = at.type;
    const namelen = at.name.length;
    if (namelen < bufSize) {
      const view = this.wasm.getMemoryView(name, at.name.length + 1);
      for (let i=0; i<at.name.length; i++) {
        view[i] = at.name.charCodeAt(i);
      }
      view[at.name.length] = 0;
      this.wasm.memU32[length >> 2] = at.name.length;
    } else {
      this.wasm.memU32[length >> 2] = 0;
    }
  }
  
  _getActiveAttrib(program, index, bufSize, length, size, type, name) {
    const at = this.gl.getActiveAttrib(this.o[program], index);
    this.writeActiveInfoToWasm(at, bufSize, length, size, type, name);
  }
  
  _getActiveUniform(program, index, bufSize, length, size, type, name) {
    const at = this.gl.getActiveUniform(this.o[program], index);
    this.writeActiveInfoToWasm(at, bufSize, length, size, type, name);
  }
  
  _getAttachedShaders(program, maxcount, countp, shadersp) {
    let dstc = 0;
    shadersp >>= 2;
    for (const shader of this.gl.getAttachedShaders(this.o[program])) {
      if (dstc < maxcount) {
        let id = this.o.indexOf(shader);
        if (id < 0) id = 0;
        this.wasm.memU32[shadersp++] = id;
      }
      dstc++;
    }
    this.wasm.memU32[countp >> 2] = dstc;
  }
  
  _getBooleanv(k, vp) {
    const v = this.gl.getParameter(k);
    if (typeof v === "boolean") {
      this.wasm.memU32[vp >> 2] = v;
    }
  }
  
  _getBufferParameteriv(t, k, vp) {
    const v = this.gl.getParameter(k);
    this.wasm.memU32[vp >> 2] = v;
  }
  
  _getFloatv(k, vp) {
    const v = this.gl.getParameter(k);
    if (v instanceof Float32Array) {
      vp >>= 2;
      for (const vs of v) {
        this.wasm.memF32[vp++] = vs;
      }
    }
  }
  
  _getFramebufferAttachmentParameteriv(t, a, k, vp) {
    const v = this.gl.getFramebufferAttachmentParameter(t, a, k);
    this.wasm.memU32[vp >> 2] = v;
  }
  
  _getIntegerv(k, vp) {
    const v = this.gl.getParameter(k);
    if (typeof(v) === "number") {
      this.wasm.memU32[vp >> 2] = v;
    }
  }
  
  _getProgramiv(p, k, vp) {
    const v = this.gl.getProgramParameter(this.o[p], k);
    this.wasm.memU32[vp >> 2] = v;
  }
  
  writeInfoLogToWasm(max, c, v, src) {
    if (src.length < max) {
      const view = this.wasm.getMemoryView(v, src.length + 1);
      for (let i=0; i<src.length; i++) {
        view[i] = src.charCodeAt(i);
      }
      view[src.length] = 0;
      this.wasm.memU32[c >> 2] = src.length;
    } else {
      this.wasm.memU32[c >> 2] = 0;
    }
  }
  
  _getProgramInfoLog(p, max, c, v) {
    this.writeInfoLogToWasm(max, c, v, this.gl.getProgramInfoLog(this.o[p]))
  }
  
  _getShaderInfoLog(s, max, c, v) {
    this.writeInfoLogToWasm(max, c, v, this.gl.getShaderInfoLog(this.o[s]));
  }
  
  _getRenderbufferParameteriv(r, k, vp) {
    const v = this.gl.getRenderbufferParameter(r, k);
    this.wasm.memU32[vp >> 2] = v;
  }
  
  _getShaderiv(s, k, vp) {
    const v = this.gl.getShaderParameter(this.o[s], k);
    this.wasm.memU32[vp >> 2] = v;
  }
  
  _getShaderPrecisionFormat(stype, ptype, rangep, precisionp) {
    const fmt = this.gl.getShaderPrecisionFormat(stype, ptype);
    if (!fmt) return;
    this.wasm.memU32[rangep >> 2] = fmt.rangeMax;
    this.wasm.memU32[precisionp >> 2] = fmt.precision;
  }
  
  _getShaderSource(s, max, c, v) {
    this.writeInfoLogToWasm(max, c, v, this.gl.getShaderSource(this.o[s]));
  }
  
  _getTexParameterfv(t, k, vp) {
    // I don't believe there are any vector texture parameters.
  }
  
  _getTexParameteriv(t, k, vp) {
    // I don't believe there are any vector texture parameters.
  }
  
  _getUniformfv(program, k, vp) {
    const v = this.gl.getUniform(this.o[program], k);
    if (typeof(v) === "number") {
      this.wasm.memF32[vp >> 2] = v;
    } else if (v instanceof Float32Array) {
      for (let i=0, vp>>=2; i<v.length; i++, vp++) {
        this.wasm.memF32[vp] = v[i];
      }
    }
  }
  
  _getUniformiv(program, k, vp) {
    const v = this.gl.getUniform(this.o[program], k);
    if (typeof(v) === "number") {
      this.wasm.memU32[vp >> 2] = v;
    } else if (v instanceof Uint32Array) {
      for (let i=0, vp>>=2; i<v.length; i++, vp++) {
        this.wasm.memU32[vp] = v[i];
      }
    }
  }
  
  _getUniformLocation(program, name) {
    name = this.wasm.zstringFromMemory(name);
    return this.gl.getUniformLocation(this.o[program], name);
  }
  
  _getVertexAttribfv(ix, k, v) {
    //TODO
  }
  
  _getVertexAttribiv(ix, k, v) {
    //TODO
  }
  
  _getVertexAttribPointer(a, b) { // => *
    //TODO
  }
  
  _readPixels(x, y, w, h, fmt, type, v) {
    //TODO
  }
  
  _shaderBinary(count, shaders, binary, format, length) {
    //TODO
  }
  
  _shaderSource(shader, one, v, c) {
    const src = this.wasm.stringFromMemory(v, c);
    this.gl.shaderSource(this.o[shader], src);
  }
  
  _texImage2D(t, l, ifmt, w, h, border, fmt, type, pixels) {
    const len = this.measureImage(w, h, fmt, type);
    const src = pixels ? this.wasm.getMemoryView(pixels, len) : null;
    this.gl.texImage2D(t, l, ifmt, w, h, border, fmt, type, src);
  }
  
  _texParameterfv(t, k, v) {
    //TODO
  }
  
  _texParameteriv(t, k, v) {
    //TODO
  }
  
  _texSubImage2D(t, l, x, y, w, h, fmt, type, v) {
    const len = this.measureImage(w, h, fmt, type);
    const src = this.wasm.getMemoryView(v, len);
    this.gl.texSubImage2D(t, l, x, y, w, h, fmt, type, src);
  }
  
  _uniform1fv(k, c, v) {
    //TODO
  }
  
  _uniform1iv(k, c, v) {
    //TODO
  }
  
  _uniform2fv(k, c, v) {
    //TODO
  }
  
  _uniform2iv(k, c, v) {
    //TODO
  }
  
  _uniform3fv(k, c, v) {
    //TODO
  }
  
  _uniform3iv(k, c, v) {
    //TODO
  }
  
  _uniform4fv(k, c, v) {
    //TODO
  }
  
  _uniform4iv(k, c, v) {
    //TODO
  }
  
  _uniformMatrix2fv(k, c, trans, v) {
    //TODO
  }
  
  _uniformMatrix3fv(k, c, trans, v) {
    //TODO
  }
  
  _uniformMatrix4fv(k, c, trans, v) {
    //TODO
  }
  
  _vertexAttrib1fv(p, v) {
    //TODO
  }
  
  _vertexAttrib2fv(p, v) {
    //TODO
  }
  
  _vertexAttrib3fv(p, v) {
    //TODO
  }
  
  _vertexAttrib4fv(p, v) {
    //TODO
  }
  
  _vertexAttribPointer(p, size, type, norm, stride, v) {
    //TODO
  }
}
