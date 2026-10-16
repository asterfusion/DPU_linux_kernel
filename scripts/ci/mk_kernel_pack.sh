
# This file should be run from the Kernel-root directory
# since it uses `pwd` as base for
#    (k-root)/lib/modules/$(KERNELRELEASE)/kernel/
# and for ZIP-file containing all KO and Image.
#
if test ! -e System.map; then
  if test -e arch; then
    echo "  ERROR: make the Kernel first"
  else
    echo "  ERROR: $0 should run only from Kernel-root-directory"
  fi
  exit 1
fi

# Other paths used by (base)/Makefile are:
#  MODLIB = $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)
#  KERNELRELEASE = $(shell cat include/config/kernel.release 2> /dev/null)
#  ARCH= arm or arm64

if test ! -e .scmversion; then
  # create EMPTY .scmversion for KERNELRELEASE without private git-extension
  # end rebuild Image and ko-modules with this short name
  touch .scmversion
  make -j32
  if [ $? != '0' ] ; then
    echo "  ABORT ---------------"
    exit 1
  fi
fi

rm -rf lib/modules/$KERNELRELEASE
make modules_install INSTALL_MOD_PATH=`pwd`

if [ $? != '0' ] ; then
    echo "  ABORT ---------------"
    exit 1
fi

export KERNELRELEASE=`cat include/config/kernel.release 2> /dev/null`
if [ "$1" = "" ] ; then
  export FNAME=$KERNELRELEASE
else
  export FNAME=$KERNELRELEASE-$1
fi
echo "  Create  $FNAME.tar.gz and Image_$FNAME in <./out> ..."

mkdir -p out
rm -rf out/$FNAME
rm -f out/mod-$FNAME.tar.gz
mv lib/modules/$KERNELRELEASE out
rm -f out/$FNAME/build
rm -f out/$FNAME/source
cd out
tar -zcf mod_$FNAME.tar.gz $KERNELRELEASE
rm -rf $KERNELRELEASE
cd - 1> /dev/null
rm -f out/Image_$FNAME
rm -f out/config_$FNAME
cp arch/$ARCH/boot/Image out/Image_$FNAME
cp .config out/config_$FNAME

echo "DONE. (Empty .scmversion file is kept)"
