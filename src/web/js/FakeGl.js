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
     * We'll make up integers for the C side, indices in these arrays.
     */
    this.programs = [null];
    this.shaders = [null];
    this.textures = [null];
    this.buffers = [null];
    this.framebuffers = [null];
    this.renderbuffers = [null];
  }
  
  getExports() {
    return {
      glActiveTexture: (texid) => this.gl.activeTexture(this.textures[texid]),
      glAttachShader: (p, s) => this.gl.attachShader(this.programs[p], this.shaders[s]),
      glBindAttribLocation: (p, i, n) => this.gl.bindAttribLocation(this.programs[p], i, this.wasm.zstringFromMemory(n)),
      glBindBuffer: (t, b) => this.gl.bindBuffer(t, this.buffers[b]),
      glBindFramebuffer: (t, b) => this.gl.bindFramebuffer(t, this.framebuffers[b]),
      glBindRenderbuffer: (t, b) => this.gl.bindRenderbuffer(t, this.renderbuffers[b]),
      glBindTexture: (tar, tex) => this.gl.bindTexture(tar, this.textures[tex]),
      glBlendColor: (r, g, b, a) => this.gl.blendColor(r, g, b, a),
      glBlendEquation: (i) => this.gl.blendEquation(i),
      glBlendEquationSeparate: (a, b) => this.gl.blendEquationSeparate(a, b),
      glBlendFunc: (s, d) => this.gl.blendFunc(s, d),
      glBlendFuncSeparate: (sc, dc, sa, da) => this.gl.blendFuncSeparate(sc, dc, sa, da),
      glBufferData: () => {},//(ii*i)"},
      glBufferSubData: () => {},//(iii*)"},
      glCheckFramebufferStatus: (b) => this.gl.checkFramebufferStatus(this.framebuffers[b]),
      glClear: (m) => this.gl.clear(m),
      glClearColor: (r, g, b, a) => this.gl.clearColor(r, g, b, a),
      glClearDepthf: (d) => this.gl.clearDepth(d),
      glClearStencil: (s) => this.gl.clearStencil(s),
      glColorMask: (r, g, b, a) => this.gl.colorMask(r, g, b, a),
      glCompileShader: (s) => this.gl.compileShader(this.shaders[s]),
      glCompressedTexImage2D: () => {},//(iiiiiii*)"},
      glCompressedTexSubImage2D: () => {},//(iiiiiiii*)"},
      glCopyTexImage2D: () => {},//(iiiiiiii)"},
      glCopyTexSubImage2D: () => {},//(iiiiiiii)"},
      glCreateProgram: () => this._createProgram(),
      glCreateShader: (t) => this._createShader(t),
      glCullFace: (f) => this.gl.cullFace(f),
      glDeleteBuffers: () => {},//(i*)"},
      glDeleteFramebuffers: () => {},//(i*)"},
      glDeleteProgram: (p) => this._deleteProgram(p),
      glDeleteRenderbuffers: () => {},//(i*)"},
      glDeleteShader: (s) => this._deleteShader(s),
      glDeleteTextures: () => {},//(i*)"},
      glDepthFunc: (f) => this.gl.depthFunc(f),
      glDepthMask: (m) => this.gl.depthMask(m),
      glDepthRangef: (n, f) => this.gl.depthRangef(n, f),
      glDetachShader: (p, s) => this.gl.detachShader(this.programs[p], this.shaders[s]),
      glDisable: (f) => this.gl.disable(f),
      glDisableVertexAttribArray: (i) => this.gl.disableVertexAttribArray(i),
      glDrawArrays: (m, p, c) => this.gl.drawArrays(m, p, c),
      glDrawElements: () => {},//(iii*)"},
      glEnable: (f) => this.gl.enable(f),
      glEnableVertexAttribArray: (i) => this.gl.enableVertexAttribArray(i),
      glFinish: () => this.gl.finish(),
      glFlush: () => this.gl.flush(),
      glFramebufferRenderbuffer: (t, a, rt, rb) => this.gl.framebufferRenderbuffer(t, a, rt, this.renderbuffers[rb]),
      glFramebufferTexture2D: (t, a, tt, tex, l) => this.gl.framebufferTexture2D(t, a, tt, this.textures[tex], l),
      glFrontFace: (f) => this.gl.frontFace(f),
      glGenBuffers: () => {},//(i*)"},
      glGenerateMipmap: (t) => this.gl.generateMipMap(t),
      glGenFramebuffers: () => {},//(i*)"},
      glGenRenderbuffers: () => {},//(i*)"},
      glGenTextures: () => {},//(i*)"},
      glGetActiveAttrib: () => {},//(iii****)"},
      glGetActiveUniform: () => {},//(iii****)"},
      glGetAttachedShaders: () => {},//(ii**)"},
      glGetAttribLocation: (p, n) => this.gl.getAttribLocation(this.programs[p], this.wasm.zstringFromMemory(n)),
      glGetBooleanv: () => {},//(i*)"},
      glGetBufferParameteriv: () => {},//(ii*)"},
      glGetError: () => this.gl.getError(),
      glGetFloatv: () => {},//(i*)"},
      glGetFramebufferAttachmentParameteriv: () => {},//(iii*)"},
      glGetIntegerv: () => {},//(i*)"},
      glGetProgramiv: () => {},//(ii*)"},
      glGetProgramInfoLog: () => {},//(ii**)"},
      glGetRenderbufferParameteriv: () => {},//(ii*)"},
      glGetShaderiv: () => {},//(ii*)"},
      glGetShaderInfoLog: () => {},//(ii**)"},
      glGetShaderPrecisionFormat: () => {},//(ii**)"},
      glGetShaderSource: () => {},//(ii**)"},
      glGetTexParameterfv: () => {},//(ii*)"},
      glGetTexParameteriv: () => {},//(ii*)"},
      glGetUniformfv: () => {},//(ii*)"},
      glGetUniformiv: () => {},//(ii*)"},
      glGetUniformLocation: () => {},//(i*)i"},
      glGetVertexAttribfv: () => {},//(ii*)"},
      glGetVertexAttribiv: () => {},//(ii*)"},
      singleGetVertexAttribPointerv: () => {},//(ii)*"},
      glHint: (t, m) => this.gl.hint(t, m),
      glIsBuffer: (i) => this.gl.isBuffer(this.buffers[i]),
      glIsEnabled: (f) => this.gl.isEnabled(f),
      glIsFramebuffer: (i) => this.gl.isFramebuffer(this.framebuffers[i]),
      glIsProgram: (i) => this.gl.isProgram(this.programs[i]),
      glIsRenderbuffer: (i) => this.gl.isRenderbuffer(this.renderbuffers[i]),
      glIsShader: (i) => this.gl.isShader(this.shaders[i]),
      glIsTexture: (i) => this.gl.isTexture(this.textures[i]),
      glLineWidth: (w) => this.gl.lineWidth(w),
      glLinkProgram: (p) => this.gl.linkProgram(this.programs[p]),
      glPixelStorei: (k, v) => this.gl.pixelStorei(k, v),
      glPolygonOffset: (f, u) => this.gl.polygonOffset(f, u),
      glReadPixels: () => {},//(iiiiii*)"},
      glReleaseShaderCompiler: () => this.gl.releaseShaderCompiler(),
      glRenderbufferStorage: (t, i, w, h) => this.gl.renderbufferStorage(t, i, w, h),
      glSampleCoverage: (v, i) => this.gl.sampleCoverage(v, i),
      glScissor: (x, y, w, h) => this.gl.scissor(x, y, w, h),
      glShaderBinary: () => {},//(i*i*i)"},
      singleShaderSource: () => {},//(ii*i)"},
      glStencilFunc: (f, r, m) => this.gl.stencilFunc(f, r, m),
      glStencilFuncSeparate: (f, fn, r, m) => this.gl.stencilFuncSeparate(f, fn, r, m),
      glStencilMask: (m) => this.gl.stencilMask(m),
      glStencilMaskSeparate: (f, m) => this.gl.stencilMaskSeparate(f, m),
      glStencilOp: (f, zf, zp) => this.gl.stencilOp(f, zf, zp),
      glStencilOpSeparate: (f, s, df, dp) => this.gl.stencilOpSeparate(f, s, df, dp),
      glTexImage2D: () => {},//(iiiiiiii*)"},
      glTexParameterf: (t, k, v) => this.gl.texParameterf(t, k, v),
      glTexParameterfv: () => {},//(ii*)"},
      glTexParameteri: (t, k, v) => this.gl.texParameteri(t, k, v),
      glTexParameteriv: () => {},//(ii*)"},
      glTexSubImage2D: () => {},//(iiiiiiii*)"},
      glUniform1f: (k, v) => this.gl.uniform1f(k, v),
      glUniform1fv: () => {},//(ii*)"},
      glUniform1i: (k, v) => this.gl.uniform1i(k, v),
      glUniform1iv: () => {},//(ii*)"},
      glUniform2f: (k, v0, v1) => this.gl.uniform2f(k, v0, v1),
      glUniform2fv: () => {},//(ii*)"},
      glUniform2i: (k, v0, v1) => this.gl.uniform2i(k, v0, v1),
      glUniform2iv: () => {},//(ii*)"},
      glUniform3f: (k, v0, v1, v2) => this.gl.uniform3f(k, v0, v1, v2),
      glUniform3fv: () => {},//(ii*)"},
      glUniform3i: (k, v0, v1, v2) => this.gl.uniform3i(k, v0, v1, v2),
      glUniform3iv: () => {},//(ii*)"},
      glUniform4f: (k, v0, v1, v2, v3) => this.gl.uniform4f(k, v0, v1, v2, v3),
      glUniform4fv: () => {},//(ii*)"},
      glUniform4i: (k, v0, v1, v2, v3) => this.gl.uniform4i(k, v0, v1, v2, v3),
      glUniform4iv: () => {},//(ii*)"},
      glUniformMatrix2fv: () => {},//(iii*)"},
      glUniformMatrix3fv: () => {},//(iii*)"},
      glUniformMatrix4fv: () => {},//(iii*)"},
      glUseProgram: (p) => this.gl.useProgram(this.programs[p]),
      glValidateProgram: (p) => this.gl.validateProgram(this.programs[p]),
      glVertexAttrib1f: (k, v) => this.gl.vertexAttrib1f(k, v),
      glVertexAttrib1fv: () => {},//(i*)"},
      glVertexAttrib2f: (k, v0, v1) => this.gl.vertexAttrib2f(k, v0, v1),
      glVertexAttrib2fv: () => {},//(i*)"},
      glVertexAttrib3f: (k, v0, v1, v2) => this.gl.vertexAttrib3f(k, v0, v1, v2),
      glVertexAttrib3fv: () => {},//(i*)"},
      glVertexAttrib4f: (k, v0, v1, v2, v3) => this.gl.vertexAttrib4f(k, v0, v1, v2, v3),
      glVertexAttrib4fv: () => {},//(i*)"},
      glVertexAttribPointer: () => {},//(iiiii*)"},
      glViewport: (x, y, w, h) => this.gl.viewport(x, y, w, h),
    };
  }
  
  _createProgram() {
    const program = this.gl.createProgram();
    if (!program) return 0;
    this.programs.push(program);
    return this.programs.length - 1;
  }
  
  _createShader(type) {
    const shader = this.gl.createShader(type);
    if (!shader) return 0;
    this.shaders.push(shader);
    return this.shaders.length - 1;
  }
  
  _deleteProgram(i) {
    const program = this.programs[i];
    if (!program) return;
    this.gl.deleteProgram(program);
    this.programs[i] = null;
  }
  
  _deleteShader(i) {
    const shader = this.shaders[i];
    if (!shader) return;
    this.gl.deleteShader(shader);
    this.shaders[i] = null;
  }
}
