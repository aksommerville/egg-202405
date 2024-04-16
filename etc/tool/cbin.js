/* cbin.js
 * Produce a C file exporting the content of any file.
 */
 
const fs = require("fs");

let srcpath = "";
let dstpath = "";
let name = "";
for (const arg of process.argv.slice(2)) {
  if (arg.startsWith("-o")) dstpath = arg.substring(2);
  else if (arg.startsWith("--name=")) name = arg.substring(7);
  else if (arg.startsWith("-")) throw new Error(`${process.argv[1]}: Unexpected option ${JSON.stringify(arg)}`);
  else srcpath = arg;
}
if (!srcpath || !dstpath || !name) {
  throw new Error(`Usage: ${process.argv[1]} -oOUTPUT --name=NAME INPUT`);
}

const src = fs.readFileSync(srcpath);
let dst = `const unsigned char ${name}[]={\n`;
let line = "";
for (const b of src) {
  line += b + ',';
  if (line.length >= 100) {
    dst += line + "\n";
    line = "";
  }
}
if (line) dst += line + "\n";
dst += `};\nconst unsigned int ${name}_length=${src.length};\n`;

fs.writeFileSync(dstpath, dst);
