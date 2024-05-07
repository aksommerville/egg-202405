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
for ARG in \$* ; do
  case "\$ARG" in
    -o*) EXEPATH="\$(realpath \${ARG:2})" ;;
    --rom=*) ROMPATH="\$(realpath \${ARG:6})" ;;
    *) echo "Unexpected argument '\$ARG'" ; exit 1 ;;
  esac
done
if [ -z "\$EXEPATH" ] || [ -z "\$ROMPATH" ] ; then
  echo "Usage: \$0 -oOUTPUT --rom=INPUT"
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
$LD -o\$EXEPATH $LIB \$TMPPATH $LDPOST
rm \$TMPPATH
EOF

chmod +x "$DSTPATH"
