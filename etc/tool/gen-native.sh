#!/bin/bash

if [ "$#" -ne 1 ] ; then
  echo "Usage: $0 OUTPUT"
  exit 1
fi

DSTPATH="$1"
rm -f "$DSTPATH"
touch "$DSTPATH"

cat - >>"$DSTPATH" <<EOF
#!/bin/bash
EXEPATH=
ROMPATH=
CODEPATH=
for ARG in \$* ; do
  case "\$ARG" in
    -o*) EXEPATH="\$(realpath \${ARG:2})" ;;
    --rom=*) ROMPATH="\$(realpath \${ARG:6})" ;;
    --code=*) CODEPATH="\$(realpath \${ARG:7})" ;;
    *) echo "Unexpected argument '\$ARG'" ; exit 1 ;;
  esac
done
if [ -z "\$EXEPATH" ] || [ -z "\$ROMPATH" ] || [ -z "\$CODEPATH" ] ; then
  echo "Usage: \$0 -oOUTPUT --rom=YOUR_EGG_FILE --code=YOUR_STATIC_LIB"
  exit 1
fi
cd $PWD
TMPPATH=egg-tmp.s
cat - >"\$TMPPATH" <<EOFS
.globl egg_bundled_rom
.globl egg_bundled_rom_length
egg_bundled_rom: .incbin "\$ROMPATH"
egg_bundled_rom_length: .int (egg_bundled_rom_length-egg_bundled_rom)
EOFS
$LD -o\$EXEPATH \$ROMPATH \$CODEPATH \$TMPPATH $LIB $LDPOST
rm \$TMPPATH
EOF

chmod +x "$DSTPATH"
