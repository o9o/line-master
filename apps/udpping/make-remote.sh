#!/bin/sh

eval "$(/bin/sh ../../remote-config.sh)"

PROJECT=udpping
BUILD_DIR=$PWD
SRC_DIR=$SVN_DIR/apps/$PROJECT
REMOTE_USER=$REMOTE_USER_HOSTS
REMOTE_HOST=$REMOTE_HOST_HOSTS
REMOTE_DIR=/root
MAKE="qmake $PROJECT.pro -r -spec linux-g++-64 CONFIG+=$BUILD_CONFIG_HOSTS && make -w && make install"

cd $BUILD_DIR || exit 1
rm -f $PROJECT.zip || exit 1
cd .. || exit 1
pwd; echo "Zipping $PROJECT..."
zip -r $BUILD_DIR/$PROJECT.zip $PROJECT || exit 1
cd $BUILD_DIR || exit 1
pwd; echo "Copying $PROJECT.zip to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR..."
scp $PROJECT.zip $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR || exit 1
rm -f stdoutfifo
rm -f stderrfifo
mkfifo stdoutfifo || exit 1
cat stdoutfifo &
mkfifo stderrfifo || exit 1
(cat stderrfifo | perl -pi -w -e "s|$REMOTE_DIR/$PROJECT|$SRC_DIR|g;" >&2) &
pwd; echo "Building remotely..."
ssh $REMOTE_USER@$REMOTE_HOST "sh -c \"pwd && cd $REMOTE_DIR && pwd && rm -rf $PROJECT && unzip -o $PROJECT.zip && cd $PROJECT && find . -exec touch {} \; && $MAKE\"" 1>stdoutfifo 2>stderrfifo
rm stdoutfifo
rm stderrfifo
