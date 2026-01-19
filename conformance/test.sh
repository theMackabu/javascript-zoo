SCRIPT_DIR=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
bash $SCRIPT_DIR/run.sh -j 16 ./build/ant $SCRIPT_DIR/kangax-es5 $SCRIPT_DIR/es1 $SCRIPT_DIR/es3 $SCRIPT_DIR/es5