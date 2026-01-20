SCRIPT_DIR=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
bash $SCRIPT_DIR/run.sh -j 16 -o ant.txt ./build/ant